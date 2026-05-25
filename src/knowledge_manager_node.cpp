#include "knowledge_manager/knowledge_manager_node.hpp"

#include <algorithm>
#include <boost/mpl/aux_/na_fwd.hpp>
#include <cctype>
#include <exception>
#include <fstream>
#include <functional>
#include <memory>

#include <ament_index_cpp/get_package_share_directory.hpp>
#include <rclcpp/logging.hpp>
#include <string>
#include <yaml-cpp/emitter.h>
#include <yaml-cpp/emittermanip.h>
#include <yaml-cpp/yaml.h>

#include "loaders/default_loader.hpp"
#include "loaders/world_loader.hpp"

WorldStateNode::WorldStateNode() : Node("world_state_node") {
  state_ = build_initial_state();

  get_state_service_ = this->create_service<GetWorldState>(
      "world/get_state",
      std::bind(&WorldStateNode::get_world_state_callback, this,
                std::placeholders::_1, std::placeholders::_2));

  update_state_service_ = this->create_service<UpdateWorldState>(
      "world/update_state",
      std::bind(&WorldStateNode::update_world_state_callback, this,
                std::placeholders::_1, std::placeholders::_2));

  RCLCPP_INFO(this->get_logger(), "Knowledge manager services ready.");
}

// ---------------------------------------------------------------------------
// Inicialización
// ---------------------------------------------------------------------------

void WorldStateNode::build_paths() {
  bank_path_ = ament_index_cpp::get_package_share_directory("knowledge_bank");
  keys_path_ = bank_path_ + "/domains/";
  volatile_path_ = bank_path_ + "/runtime/";

  locations_path_ = keys_path_ + "locations.yaml";
  objects_path_ = keys_path_ + "objects.yaml";
  people_path_ = keys_path_ + "people.yaml";
  world_path_ = volatile_path_ + "world.yaml";
  output_path_ = volatile_path_ + "runtime.yaml";
}

void WorldStateNode::build_references() {
  locations_ref_ = DefaultLoader(locations_path_).build();
  objects_ref_ = DefaultLoader(objects_path_).build();
  people_ref_ = DefaultLoader(people_path_).build();

  RCLCPP_INFO(
      this->get_logger(),
      "References loaded: %zu locations, %zu objects, %zu people descriptions.",
      locations_ref_.size(), objects_ref_.size(), people_ref_.size());
}

WorldState WorldStateNode::build_initial_state() {
  build_paths();
  build_references();

  WorldLoader loader(world_path_, locations_ref_, objects_ref_, people_ref_);
  WorldState state = loader.build();

  RCLCPP_INFO(
      this->get_logger(),
      "World state built. Robot at: %s | right_hand: %s | left_hand: %s",
      state.robot.location.c_str(), state.robot.right_hand.c_str(),
      state.robot.left_hand.c_str());

  save_state(state, output_path_);
  return state;
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

void WorldStateNode::get_world_state_callback(
    const std::shared_ptr<GetWorldState::Request> request,
    const std::shared_ptr<GetWorldState::Response> response) {
  const std::string &category = request->category;
  const std::string &id = request->id;
  const std::string &attribute = request->attribute;

  if (category == "robot") {
    response->json = build_robot_json(state_, attribute).dump(2);
  } else if (category == "people") {
    response->json = build_people_json(state_, id, attribute).dump(2);
  } else if (category == "objects") {
    response->json = build_objects_json(state_, id, attribute).dump(2);
  } else if (category == "known_locations") {
    response->json = build_locations_json(state_).dump(2);
  } else if (category == "world") {
    response->json = build_world_json(state_);
  } else {
    RCLCPP_WARN(this->get_logger(), "Unknown category \'%s\'",
                category.c_str());
    response->json = "{}";
  }
}

void WorldStateNode::update_world_state_callback(
    const std::shared_ptr<UpdateWorldState::Request> request,
    const std::shared_ptr<UpdateWorldState::Response> response) {
  try {
    update_world_state(request->json);
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
    json j;
    j["name"] = robot.name;
    j["location"] = robot.location;
    j["left_hand"] = robot.left_hand;
    j["right_hand"] = robot.right_hand;
    return j;
  }

  if (attribute == "name") {
    return robot.name;
  }

  if (attribute == "location") {
    return robot.location;
  }

  if (attribute == "left_hand") {
    return robot.left_hand;
  }

  if (attribute == "right_hand") {
    return robot.right_hand;
  }

  // Invalid attribute
  return nullptr;
}

json WorldStateNode::build_objects_json(const WorldState &state,
                                        const std::string &id,
                                        const std::string &attribute) const {
  // Todos los objetos
  if (id.empty()) {
    json arr = json::array();
    for (const auto &obj : state.objects) {
      arr.push_back(object_to_json(obj));
    }
    return {{"objects", arr}};
  }

  // Buscar objeto
  const short int number = static_cast<short int>(std::stoi(id));
  auto it = find_object_by_id(state, number);
  if (it == state.objects.end()) {
    return nullptr;
  }

  // Objeto completo
  if (attribute.empty()) {
    return object_to_json(*it);
  }

  // Attributo específico
  return object_attribute_to_json(*it, attribute);
}

json WorldStateNode::build_locations_json(const WorldState &state) const {
  json j;
  j["known_locations"] = state.known_locations;
  return j;
}

json WorldStateNode::build_people_json(const WorldState &state,
                                       const std::string &id,
                                       const std::string &attribute) const {
  // Todas las personas
  if (id.empty()) {
    json arr = json::array();
    for (const auto &person : state.persons) {
      arr.push_back(person_to_json(person));
    }
    return {{"people", arr}};
  }

  // Buscar persona
  const short int number = static_cast<short int>(std::stoi(id));
  auto it = find_person_by_id(state, number);
  if (it == state.persons.end()) {
    return nullptr;
  }

  // Persona completa
  if (attribute.empty()) {
    return person_to_json(*it);
  }

  // Atributo específico
  return person_attribute_to_json(*it, attribute);
}

std::string WorldStateNode::build_world_json(const WorldState &state) const {
  json j;
  j["robot"] = build_robot_json(state);
  j["objects"] = build_objects_json(state);
  j["known_locations"] = build_locations_json(state);
  j["people"] = build_people_json(state);
  return j.dump(2);
}

// ---------------------------------------------------------------------------
// Actualización de estado en memoria
// ---------------------------------------------------------------------------

void WorldStateNode::update_world_state(const std::string &json_str) {
  const json data = json::parse(json_str);

  if (data.contains("robot")) {
    update_robot_state(data["robot"]);
  }
  if (data.contains("object")) {
    update_object_state(data["object"]);
  }
  if (data.contains("objects")) {
    update_objects_table(data["objects"]);
  }
  if (data.contains("person")) {
    update_person_state(data["person"]);
  }
  if (data.contains("persons")) {
    update_people_table(data["persons"]);
  }
}

void WorldStateNode::update_robot_state(const json &robot) {
  if (robot.contains("name")) {
    state_.robot.name = sym(robot["name"]);
  }
  if (robot.contains("room")) {
    const std::string room = sym(robot["room"]);
    const bool valid =
        room == "unknown" ||
        std::binary_search(locations_ref_.begin(), locations_ref_.end(), room);
    if (valid) {
      state_.robot.location = room;
    } else {
      RCLCPP_WARN(this->get_logger(),
                  "update_robot_state: invalid location '%s', ignoring.",
                  room.c_str());
    }
  }
  if (robot.contains("left_hand")) {
    state_.robot.left_hand = sym(robot["left_hand"]);
  }
  if (robot.contains("right_hand")) {
    state_.robot.right_hand = sym(robot["right_hand"]);
  }
}

void WorldStateNode::update_object_state(const json &obj) {
  short int number = 0;

  if (obj.contains("number")) {
    number = obj["number"].get<short int>();
  } else {
    short int max_number = 0;
    for (const auto &o : state_.objects) {
      max_number = std::max(max_number, o.number);
    }
    number = max_number + 1;
  }

  auto it = std::find_if(
      state_.objects.begin(), state_.objects.end(),
      [&](const ObjectDescription &o) { return o.number == number; });

  if (it == state_.objects.end()) {

    ObjectDescription object{};
    object.number = number;
    state_.objects.push_back(object);
    it = std::prev(state_.objects.end());
  }

  if (obj.contains("name")) {
    it->name = sym(obj["name"]);
  }

  if (obj.contains("room")) {

    const std::string room = sym(obj["room"]);
    const bool valid =
        room == "unknown" ||
        std::binary_search(locations_ref_.begin(), locations_ref_.end(), room);
    if (!valid) {
      RCLCPP_WARN(this->get_logger(),
                  "update_object_state: invalid location '%s' "
                  "for object #%d, ignoring.",
                  room.c_str(), number);
      return;
    }

    it->locations = {room};
  }

  if (obj.contains("navigation") && obj["navigation"].is_array()) {
    it->navigation.clear();

    for (const auto &coord : obj["navigation"]) {
      it->navigation.push_back(coord);
    }
  }

  if (obj.contains("manipulation") && obj["manipulation"].is_array()) {
    it->manipulation.clear();

    for (const auto &coord : obj["manipulation"]) {
      it->manipulation.push_back(coord);
    }
  }
}

void WorldStateNode::update_objects_table(const json &objects) {
  state_.objects.clear();
  for (const auto &obj : objects) {
    update_object_state(obj);
  }
}

void WorldStateNode::update_person_state(const json &person_json) {
  short int number = 0;

  if (person_json.contains("number")) {
    number = person_json["number"].get<short int>();

  } else {
    short int max_number = 0;

    for (const auto &p : state_.persons) {
      max_number = std::max(max_number, p.number);
    }

    number = max_number + 1;
  }

  auto it = std::find_if(
      state_.persons.begin(), state_.persons.end(),
      [&](const PersonDescription &p) { return p.number == number; });

  if (it == state_.persons.end()) {
    PersonDescription person{};
    person.number = number;
    state_.persons.push_back(person);
    it = std::prev(state_.persons.end());
  }

  if (person_json.contains("name")) {
    it->name = sym(person_json["name"]);
  }

  if (person_json.contains("room")) {
    const std::string room = sym(person_json["room"]);
    const bool valid =
        room == "unknown" ||
        std::binary_search(locations_ref_.begin(), locations_ref_.end(), room);
    if (valid) {
      it->room = room;
    } else {
      RCLCPP_WARN(this->get_logger(),
                  "update_person_state: invalid location '%s', ignoring.",
                  room.c_str());
    }
  }

  if (person_json.contains("gender")) {
    it->gender = sym(person_json["gender"]);
  }

  if (person_json.contains("emotion")) {
    it->emotion = sym(person_json["emotion"]);
  }

  if (person_json.contains("shirt_color")) {
    it->shirt_color = sym(person_json["shirt_color"]);
  }

  if (person_json.contains("hair_color")) {
    it->hair_color = sym(person_json["hair_color"]);
  }

  if (person_json.contains("hair_style")) {
    it->hair_style = sym(person_json["hair_style"]);
  }

  if (person_json.contains("drink")) {
    it->drink = sym(person_json["drink"]);
  }

  if (person_json.contains("age_group")) {
    it->age_group = sym(person_json["age_group"]);
  }

  if (person_json.contains("accessories") &&
      person_json["accessories"].is_array()) {

    it->accessories.clear();

    for (const auto &acc : person_json["accessories"]) {
      it->accessories.push_back(sym(acc));
    }
  }

  if (person_json.contains("order") && person_json["order"].is_array()) {
    it->order.clear();

    for (const auto &ord : person_json["order"]) {
      it->order.push_back(sym(ord));
    }
  }

  if (person_json.contains("navigation") &&
      person_json["navigation"].is_array()) {
    it->navigation.clear();

    for (const auto &coord : person_json["navigation"]) {
      it->navigation.push_back(coord);
    }
  }
}

void WorldStateNode::update_people_table(const json &persons) {
  state_.persons.clear();
  for (const auto &p : persons) {
    update_person_state(p);
  }
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
  out << YAML::Key << "room" << YAML::Value << state.robot.location;
  out << YAML::Key << "left_hand" << YAML::Value << state.robot.left_hand;
  out << YAML::Key << "right_hand" << YAML::Value << state.robot.right_hand;
  out << YAML::EndMap;

  // Known locations
  out << YAML::Key << "known_locations" << YAML::Value << YAML::BeginSeq;
  for (const auto &loc : state.known_locations) {
    out << loc;
  }
  out << YAML::EndSeq;

  // Objects
  out << YAML::Key << "objects" << YAML::Value << YAML::BeginSeq;

  for (const auto &obj : state.objects) {

    out << YAML::BeginMap;
    out << YAML::Key << "number" << YAML::Value << obj.number;
    out << YAML::Key << "name" << YAML::Value << obj.name;
    out << YAML::Key << "locations" << YAML::Value << YAML::Flow
        << YAML::BeginSeq;

    for (const auto &loc : obj.locations) {
      out << loc;
    }
    out << YAML::EndSeq;
    out << YAML::Key << "navigation" << YAML::Value << YAML::Flow
        << YAML::BeginSeq;
    for (const auto &coord : obj.navigation) {
      out << coord;
    }
    out << YAML::EndSeq;
    out << YAML::Key << "manipulation" << YAML::Value << YAML::Flow
        << YAML::BeginSeq;
    for (const auto &coord : obj.manipulation) {
      out << coord;
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;
  }
  out << YAML::EndSeq;

  // People
  out << YAML::Key << "people" << YAML::Value << YAML::BeginSeq;
  for (const auto &person : state.persons) {
    out << YAML::BeginMap;
    out << YAML::Key << "number" << YAML::Value << person.number;
    out << YAML::Key << "name" << YAML::Value << person.name;
    out << YAML::Key << "gender" << YAML::Value << person.gender;
    out << YAML::Key << "age_group" << YAML::Value << person.age_group;
    out << YAML::Key << "emotion" << YAML::Value << person.emotion;
    out << YAML::Key << "hair_color" << YAML::Value << person.hair_color;
    out << YAML::Key << "hair_style" << YAML::Value << person.hair_style;
    out << YAML::Key << "shirt_color" << YAML::Value << person.shirt_color;
    out << YAML::Key << "drink" << YAML::Value << person.drink;
    out << YAML::Key << "room" << YAML::Value << person.room;
    out << YAML::Key << "accessories" << YAML::Value << YAML::Flow
        << YAML::BeginSeq;
    for (const auto &acc : person.accessories) {
      out << acc;
    }
    out << YAML::EndSeq;
    out << YAML::Key << "order" << YAML::Value << YAML::Flow << YAML::BeginSeq;
    for (const auto &ord : person.order) {
      out << ord;
    }
    out << YAML::EndSeq;
    out << YAML::Key << "navigation" << YAML::Value << YAML::Flow
        << YAML::BeginSeq;
    for (const auto &coord : person.navigation) {
      out << coord;
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;
  }
  out << YAML::EndSeq;

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

std::string WorldStateNode::sym(const json &value) const {
  if (value.is_null()) {
    return "unknown";
  }

  std::string s = value.get<std::string>();
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);
  std::replace(s.begin(), s.end(), ' ', '_');
  return s.empty() ? "unknown" : s;
}

json WorldStateNode::person_to_json(const PersonDescription &person) const {
  return {{"number", person.number},
          {"name", person.name},
          {"gender", person.gender},
          {"age_group", person.age_group},
          {"emotion", person.emotion},
          {"hair_color", person.hair_color},
          {"hair_style", person.hair_style},
          {"shirt_color", person.shirt_color},
          {"drink", person.drink},
          {"room", person.room},
          {"accessories", person.accessories},
          {"order", person.order},
          {"navigation", person.navigation}};
}

json WorldStateNode::person_attribute_to_json(
    const PersonDescription &person, const std::string &attribute) const {
  if (attribute == "number") {
    return person.number;
  } else if (attribute == "name") {
    return person.name;
  } else if (attribute == "gender") {
    return person.gender;
  } else if (attribute == "age_group") {
    return person.age_group;
  } else if (attribute == "emotion") {
    return person.emotion;
  } else if (attribute == "hair_color") {
    return person.hair_color;
  } else if (attribute == "hair_style") {
    return person.hair_style;
  } else if (attribute == "shirt_color") {
  } else if (attribute == "drink") {
    return person.drink;
  } else if (attribute == "room") {
    return person.room;
  } else if (attribute == "accessories") {
    return person.accessories;
  } else if (attribute == "order") {
    return person.order;
  } else if (attribute == "navigation") {
    return person.navigation;
  }

  return nullptr;
}

std::vector<PersonDescription>::const_iterator
WorldStateNode::find_person_by_id(const WorldState &state,
                                  short int number) const {
  return std::find_if(
      state.persons.begin(), state.persons.end(),
      [&](const PersonDescription &p) { return p.number == number; });
}

json WorldStateNode::object_to_json(const ObjectDescription &obj) const {
  return {
      {"number", obj.number},
      {"name", obj.name},
      {"locations", obj.locations},
      {"navigation", obj.navigation},
      {"manipulation", obj.manipulation},
  };
}

json WorldStateNode::object_attribute_to_json(
    const ObjectDescription &obj, const std::string &attribute) const {
  if (attribute == "number") {
    return obj.number;
  }

  if (attribute == "name") {
    return obj.name;
  }

  if (attribute == "locations") {
    return obj.locations;
  }

  if (attribute == "navigation") {
    return obj.navigation;
  }

  if (attribute == "manipulation") {
    return obj.manipulation;
  }

  return nullptr;
}

std::vector<ObjectDescription>::const_iterator
WorldStateNode::find_object_by_id(const WorldState &state,
                                  const short int number) const {
  return std::find_if(
      state.objects.begin(), state.objects.end(),
      [&](const ObjectDescription &obj) { return obj.number == number; });
}
