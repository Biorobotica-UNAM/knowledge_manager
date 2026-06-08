#include "knowledge_manager/knowledge_manager_node.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <stdexcept>

#include <ament_index_cpp/get_package_share_directory.hpp>
#include <rclcpp/logging.hpp>
#include <string>
#include <yaml-cpp/emitter.h>
#include <yaml-cpp/emittermanip.h>
#include <yaml-cpp/yaml.h>

#include "loaders/world_loader.hpp"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

WorldStateNode::WorldStateNode() : Node("world_state_node") {
  // Callback groups independientes → el executor puede despacharlos en
  // hilos distintos sin que uno bloquee al otro
  cb_group_get_ =
      this->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
  cb_group_update_ =
      this->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);

  state_ = build_initial_state();

  //rclcpp::ServicesQoS qos;
  // Guys, please. Make sure the code compiles with ROS 2 Humble too. We talked about this!!!
  //const rclcpp::QoS qos = rclcpp::ServicesQoS();
  const rmw_qos_profile_t qos = rmw_qos_profile_services_default;

  get_state_service_ = this->create_service<GetWorldState>(
      "world/get_state",
      std::bind(&WorldStateNode::get_world_state_callback, this,
                std::placeholders::_1, std::placeholders::_2),
      qos, cb_group_get_);

  update_state_service_ = this->create_service<UpdateWorldState>(
      "world/update_state",
      std::bind(&WorldStateNode::update_world_state_callback, this,
                std::placeholders::_1, std::placeholders::_2),
      qos, cb_group_update_);

  RCLCPP_INFO(this->get_logger(), "Knowledge manager services ready.");
}

// ---------------------------------------------------------------------------
// Inicialización
// ---------------------------------------------------------------------------

void WorldStateNode::build_paths() {
  bank_path_ = ament_index_cpp::get_package_share_directory("knowledge_bank");
  volatile_path_ = bank_path_ + "/runtime/";
  world_path_ = volatile_path_ + "world.yaml";
  output_path_ = volatile_path_ + "runtime.yaml";
}

WorldState WorldStateNode::build_initial_state() {
  build_paths();

  WorldLoader loader(world_path_);
  WorldState state = loader.build();

  RCLCPP_INFO(
      this->get_logger(),
      "World state built. Robot at: %s | right_hand: %s | left_hand: %s",
      state.robot.room.c_str(), state.robot.right_hand.c_str(),
      state.robot.left_hand.c_str());

  save_state(state, output_path_);
  return state;
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

// GET  →  category + id (opcional) + attribute (opcional)
//
//   category = "robot"           → robot completo o un atributo
//   category = "known_locations" → lista de ubicaciones
//   category = "world"           → estado completo
//   cualquier otra clave         → se busca primero en objects, luego en people

void WorldStateNode::get_world_state_callback(
    const std::shared_ptr<GetWorldState::Request> request,
    const std::shared_ptr<GetWorldState::Response> response) {

  std::shared_lock lock(state_mutex_); // múltiples lecturas simultáneas

  const std::string &category = request->category;
  const std::string &id = request->id;
  const std::string &attribute = request->attribute;

  try {
    if (category == "robot") {
      response->json = build_robot_json(state_, attribute).dump(2);
    } else if (category == "known_locations") {
      response->json = build_locations_json(state_).dump(2);
    } else if (category == "world") {
      response->json = build_world_json(state_);
    } else if (state_.objects.count(category)) {
      response->json =
          build_objects_json(state_, category, id, attribute).dump(2);
    } else if (state_.people.count(category)) {
      response->json =
          build_people_json(state_, category, id, attribute).dump(2);
    } else {
      RCLCPP_WARN(this->get_logger(), "Unknown category '%s'",
                  category.c_str());
      response->json = "{}";
    }
  } catch (const std::exception &e) {
    RCLCPP_ERROR(this->get_logger(), "get_world_state error: %s", e.what());
    response->json = "{}";
  }
}

// UPDATE  →  category + id (opcional) + attribute + value
//
//   El campo `value` siempre es string. Convenciones:
//     - string simple:   "kitchen"
//     - array de string: '["milk","apple"]'
//     - array de float:  '[0.0, 1.0, 0.0]'
//
//   El callback detecta el tipo por contenido, igual que el loader.

void WorldStateNode::update_world_state_callback(
    const std::shared_ptr<UpdateWorldState::Request> request,
    const std::shared_ptr<UpdateWorldState::Response> response) {

  std::unique_lock lock(state_mutex_); // escritura exclusiva

  try {
    const std::string &category = request->category;
    const std::string &id = request->id;
    const std::string &attribute = request->attribute;
    const std::string &value = request->value;

    if (category == "robot") {
      update_robot_attribute(attribute, value);
    } else if (state_.objects.count(category)) {
      update_object_attribute(category, id, attribute, value);
    } else if (state_.people.count(category)) {
      update_person_attribute(category, id, attribute, value);
    } else {
      throw std::invalid_argument("Unknown category: " + category);
    }

    save_state(state_, output_path_);
    response->success = true;
    response->message = "World state updated.";

  } catch (const std::exception &e) {
    response->success = false;
    response->message = e.what();
    RCLCPP_ERROR(this->get_logger(), "Failed updating world state: %s",
                 e.what());
  }
}

// ---------------------------------------------------------------------------
// Serialización JSON
// ---------------------------------------------------------------------------

json WorldStateNode::build_robot_json(const WorldState &state,
                                      const std::string &attribute) const {
  const auto &robot = state.robot;

  if (attribute.empty()) {
    return {{"name", robot.name},
            {"room", robot.room},
            {"left_hand", robot.left_hand},
            {"right_hand", robot.right_hand}};
  }

  if (attribute == "name")
    return robot.name;
  if (attribute == "room")
    return robot.room;
  if (attribute == "left_hand")
    return robot.left_hand;
  if (attribute == "right_hand")
    return robot.right_hand;

  return nullptr;
}

json WorldStateNode::build_objects_json(const WorldState &state,
                                        const std::string &category,
                                        const std::string &id,
                                        const std::string &attribute) const {
  const auto &objects = resolve_objects(category);

  if (id.empty()) {
    json arr = json::array();
    for (const auto &obj : objects)
      arr.push_back(object_to_json(obj));
    return {{category, arr}};
  }

  const short int number = static_cast<short int>(std::stoi(id));
  auto it = find_object(objects, number);
  if (it == objects.end())
    return nullptr;

  if (attribute.empty())
    return object_to_json(*it);

  return object_attribute_to_json(*it, attribute);
}

json WorldStateNode::build_locations_json(const WorldState &state) const {
  return {{"known_locations", state.known_locations}};
}

json WorldStateNode::build_people_json(const WorldState &state,
                                       const std::string &category,
                                       const std::string &id,
                                       const std::string &attribute) const {
  const auto &persons = resolve_people(category);

  if (id.empty()) {
    json arr = json::array();
    for (const auto &person : persons)
      arr.push_back(person_to_json(person));
    return {{category, arr}};
  }

  const short int number = static_cast<short int>(std::stoi(id));
  auto it = find_person(persons, number);
  if (it == persons.end())
    return nullptr;

  if (attribute.empty())
    return person_to_json(*it);

  return person_attribute_to_json(*it, attribute);
}

std::string WorldStateNode::build_world_json(const WorldState &state) const {
  json j;
  j["robot"] = build_robot_json(state);
  j["known_locations"] = build_locations_json(state);

  for (const auto &[cat, _] : state.objects)
    j[cat] = build_objects_json(state, cat);

  for (const auto &[cat, _] : state.people)
    j[cat] = build_people_json(state, cat);

  return j.dump(2);
}

// ---------------------------------------------------------------------------
// Actualización del estado en memoria
// ---------------------------------------------------------------------------

void WorldStateNode::update_robot_attribute(const std::string &attribute,
                                            const std::string &value) {
  const std::string v = normalize(value);

  if (attribute == "name") {
    state_.robot.name = v;
    return;
  }
  if (attribute == "room") {
    state_.robot.room = v;
    return;
  }
  if (attribute == "left_hand") {
    state_.robot.left_hand = v;
    return;
  }
  if (attribute == "right_hand") {
    state_.robot.right_hand = v;
    return;
  }

  throw std::invalid_argument("Unknown robot attribute: " + attribute);
}

void WorldStateNode::update_object_attribute(const std::string &category,
                                             const std::string &id,
                                             const std::string &attribute,
                                             const std::string &value) {
  auto &objects = resolve_objects(category);

  if (id.empty())
    throw std::invalid_argument("id is required to update an object");

  const short int number = static_cast<short int>(std::stoi(id));
  auto it = find_object(objects, number);

  if (it == objects.end()) {
    // Insertar nuevo objeto si no existe
    ObjectDescription obj{};
    obj.number = number;
    objects.push_back(obj);
    it = std::prev(objects.end());
  }

  apply_object_attribute(*it, attribute, value);
}

void WorldStateNode::update_person_attribute(const std::string &category,
                                             const std::string &id,
                                             const std::string &attribute,
                                             const std::string &value) {
  auto &persons = resolve_people(category);

  if (id.empty())
    throw std::invalid_argument("id is required to update a person");

  const short int number = static_cast<short int>(std::stoi(id));
  auto it = find_person(persons, number);

  if (it == persons.end()) {
    // Insertar nueva persona si no existe
    PersonDescription person{};
    person.number = number;
    persons.push_back(person);
    it = std::prev(persons.end());
  }

  apply_person_attribute(*it, attribute, value);
}

// ---------------------------------------------------------------------------
// Persistencia
// ---------------------------------------------------------------------------

void WorldStateNode::save_state(const WorldState &state,
                                const std::string &output_path) const {
  YAML::Emitter out;
  out << YAML::BeginMap;

  // Robot
  out << YAML::Key << "robot" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "name" << YAML::Value << state.robot.name;
  out << YAML::Key << "room" << YAML::Value << state.robot.room;
  out << YAML::Key << "left_hand" << YAML::Value << state.robot.left_hand;
  out << YAML::Key << "right_hand" << YAML::Value << state.robot.right_hand;
  out << YAML::EndMap;

  // Known locations
  out << YAML::Key << "known_locations" << YAML::Value << YAML::BeginSeq;
  for (const auto &loc : state.known_locations)
    out << loc;
  out << YAML::EndSeq;

  // Objetos — itera el map dinámico
  auto emit_objects = [&](const std::string &key,
                          const std::vector<ObjectDescription> &objects) {
    out << YAML::Key << key << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "type" << YAML::Value << "object";
    out << YAML::Key << "entries" << YAML::Value << YAML::BeginSeq;
    for (const auto &obj : objects) {
      out << YAML::BeginMap;
      out << YAML::Key << "number" << YAML::Value << obj.number;
      out << YAML::Key << "name" << YAML::Value << obj.name;
      out << YAML::Key << "locations" << YAML::Value << YAML::Flow
          << YAML::BeginSeq;
      for (const auto &loc : obj.locations)
        out << loc;
      out << YAML::EndSeq;
      out << YAML::Key << "navigation" << YAML::Value << YAML::Flow
          << YAML::BeginSeq;
      for (const auto &c : obj.navigation)
        out << c;
      out << YAML::EndSeq;
      out << YAML::Key << "manipulation" << YAML::Value << YAML::Flow
          << YAML::BeginSeq;
      for (const auto &c : obj.manipulation)
        out << c;
      out << YAML::EndSeq;
      out << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;
  };

  for (const auto &[cat, objs] : state.objects)
    emit_objects(cat, objs);

  // Personas — itera el map dinámico
  auto emit_people = [&](const std::string &key,
                         const std::vector<PersonDescription> &persons) {
    out << YAML::Key << key << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "type" << YAML::Value << "people";
    out << YAML::Key << "entries" << YAML::Value << YAML::BeginSeq;
    for (const auto &person : persons) {
      out << YAML::BeginMap;
      out << YAML::Key << "number" << YAML::Value << person.number;
      out << YAML::Key << "name" << YAML::Value << person.name;
      out << YAML::Key << "description" << YAML::Value << YAML::Flow
          << YAML::BeginSeq;
      for (const auto &d : person.description)
        out << d;
      out << YAML::EndSeq;
      out << YAML::Key << "drink" << YAML::Value << person.drink;
      out << YAML::Key << "room" << YAML::Value << person.room;
      out << YAML::Key << "accessories" << YAML::Value << YAML::Flow
          << YAML::BeginSeq;
      for (const auto &a : person.accessories)
        out << a;
      out << YAML::EndSeq;
      out << YAML::Key << "order" << YAML::Value << YAML::Flow
          << YAML::BeginSeq;
      for (const auto &o : person.order)
        out << o;
      out << YAML::EndSeq;
      out << YAML::Key << "navigation" << YAML::Value << YAML::Flow
          << YAML::BeginSeq;
      for (const auto &c : person.navigation)
        out << c;
      out << YAML::EndSeq;
      out << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;
  };

  for (const auto &[cat, persons] : state.people)
    emit_people(cat, persons);

  out << YAML::EndMap;

  std::ofstream fout(output_path);
  if (!fout.is_open()) {
    RCLCPP_ERROR(this->get_logger(), "Could not open output file: %s",
                 output_path.c_str());
    return;
  }

  fout << out.c_str();
  RCLCPP_INFO(this->get_logger(), "Runtime state saved to: %s",
              output_path.c_str());
}

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------

std::string WorldStateNode::normalize(const std::string &s) const {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(), ::tolower);
  std::replace(out.begin(), out.end(), ' ', '_');
  return out.empty() ? "unknown" : out;
}

// --- JSON helpers ---

json WorldStateNode::person_to_json(const PersonDescription &person) const {
  return {{"number", person.number},
          {"name", person.name},
          {"description", person.description},
          {"drink", person.drink},
          {"room", person.room},
          {"accessories", person.accessories},
          {"order", person.order},
          {"navigation", person.navigation}};
}

json WorldStateNode::person_attribute_to_json(
    const PersonDescription &person, const std::string &attribute) const {
  if (attribute == "number")
    return person.number;
  if (attribute == "name")
    return person.name;
  if (attribute == "description")
    return person.description;
  if (attribute == "drink")
    return person.drink;
  if (attribute == "room")
    return person.room;
  if (attribute == "accessories")
    return person.accessories;
  if (attribute == "order")
    return person.order;
  if (attribute == "navigation")
    return person.navigation;
  return nullptr;
}

// Aplica un único atributo a una PersonDescription.
// `value` llega como string; si empieza con '[' se parsea como JSON array.
void WorldStateNode::apply_person_attribute(PersonDescription &person,
                                            const std::string &attribute,
                                            const std::string &value) {
  if (attribute == "name") {
    person.name = normalize(value);
    return;
  }
  if (attribute == "drink") {
    person.drink = normalize(value);
    return;
  }
  if (attribute == "room") {
    person.room = normalize(value);
    return;
  }

  // Atributos que son arrays de string
  if (attribute == "description" || attribute == "accessories" ||
      attribute == "order") {
    const json arr = json::parse(value);
    std::vector<std::string> &target =
        (attribute == "description")   ? person.description
        : (attribute == "accessories") ? person.accessories
                                       : person.order;
    target.clear();
    for (const auto &item : arr)
      target.push_back(normalize(item.get<std::string>()));
    return;
  }

  // Atributo de array de float
  if (attribute == "navigation") {
    const json arr = json::parse(value);
    person.navigation.clear();
    for (const auto &coord : arr)
      person.navigation.push_back(coord.get<float>());
    return;
  }

  throw std::invalid_argument("Unknown person attribute: " + attribute);
}

json WorldStateNode::object_to_json(const ObjectDescription &obj) const {
  return {{"number", obj.number},
          {"name", obj.name},
          {"locations", obj.locations},
          {"navigation", obj.navigation},
          {"manipulation", obj.manipulation}};
}

json WorldStateNode::object_attribute_to_json(
    const ObjectDescription &obj, const std::string &attribute) const {
  if (attribute == "number")
    return obj.number;
  if (attribute == "name")
    return obj.name;
  if (attribute == "locations")
    return obj.locations;
  if (attribute == "navigation")
    return obj.navigation;
  if (attribute == "manipulation")
    return obj.manipulation;
  return nullptr;
}

// Aplica un único atributo a un ObjectDescription.
void WorldStateNode::apply_object_attribute(ObjectDescription &obj,
                                            const std::string &attribute,
                                            const std::string &value) {
  if (attribute == "name") {
    obj.name = normalize(value);
    return;
  }

  if (attribute == "locations") {
    const json arr = json::parse(value);
    obj.locations.clear();
    for (const auto &loc : arr)
      obj.locations.push_back(normalize(loc.get<std::string>()));
    return;
  }

  if (attribute == "navigation" || attribute == "manipulation") {
    const json arr = json::parse(value);
    std::vector<float> &target =
        (attribute == "navigation") ? obj.navigation : obj.manipulation;
    target.clear();
    for (const auto &coord : arr)
      target.push_back(coord.get<float>());
    return;
  }

  throw std::invalid_argument("Unknown object attribute: " + attribute);
}

// --- Resolvers sobre maps dinámicos ---

std::vector<ObjectDescription> &
WorldStateNode::resolve_objects(const std::string &category) {
  auto it = state_.objects.find(category);
  if (it == state_.objects.end())
    throw std::invalid_argument("Unknown object category: " + category);
  return it->second;
}

const std::vector<ObjectDescription> &
WorldStateNode::resolve_objects(const std::string &category) const {
  auto it = state_.objects.find(category);
  if (it == state_.objects.end())
    throw std::invalid_argument("Unknown object category: " + category);
  return it->second;
}

std::vector<PersonDescription> &
WorldStateNode::resolve_people(const std::string &category) {
  auto it = state_.people.find(category);
  if (it == state_.people.end())
    throw std::invalid_argument("Unknown person category: " + category);
  return it->second;
}

const std::vector<PersonDescription> &
WorldStateNode::resolve_people(const std::string &category) const {
  auto it = state_.people.find(category);
  if (it == state_.people.end())
    throw std::invalid_argument("Unknown person category: " + category);
  return it->second;
}

// --- find por number (versiones mutable y const) ---

std::vector<ObjectDescription>::iterator
WorldStateNode::find_object(std::vector<ObjectDescription> &objects,
                            short int number) {
  return std::find_if(
      objects.begin(), objects.end(),
      [&](const ObjectDescription &o) { return o.number == number; });
}

std::vector<ObjectDescription>::const_iterator
WorldStateNode::find_object(const std::vector<ObjectDescription> &objects,
                            short int number) const {
  return std::find_if(
      objects.begin(), objects.end(),
      [&](const ObjectDescription &o) { return o.number == number; });
}

std::vector<PersonDescription>::iterator
WorldStateNode::find_person(std::vector<PersonDescription> &persons,
                            short int number) {
  return std::find_if(
      persons.begin(), persons.end(),
      [&](const PersonDescription &p) { return p.number == number; });
}

std::vector<PersonDescription>::const_iterator
WorldStateNode::find_person(const std::vector<PersonDescription> &persons,
                            short int number) const {
  return std::find_if(
      persons.begin(), persons.end(),
      [&](const PersonDescription &p) { return p.number == number; });
}
