#include "knowledge_manager/knowledge_manager_node.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <functional>
#include <memory>

#include <ament_index_cpp/get_package_share_directory.hpp>
#include <rclcpp/logging.hpp>
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
  // El loader de locations excluye keys que terminan en "_name"
  locations_ref_ = DefaultLoader(locations_path_, "_name").build();
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
  const WorldState current = build_current_state();
  const std::string &field = request->field;

  RCLCPP_INFO(this->get_logger(), "Get request received: '%s'", field.c_str());

  if (field == "robot") {
    response->json = build_robot_json(current).dump(2);
  } else if (field == "objects") {
    response->json = build_objects_json(current).dump(2);
  } else if (field == "known_locations") {
    response->json = build_locations_json(current).dump(2);
  } else if (field == "people") {
    response->json = build_people_json(current).dump(2);
  } else if (field.empty()) {
    response->json = build_world_json(current);
  } else {
    RCLCPP_WARN(this->get_logger(), "Unknown field requested: '%s'",
                field.c_str());
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
// Lectura
// ---------------------------------------------------------------------------

WorldState WorldStateNode::build_current_state() const {
  return WorldLoader(output_path_, locations_ref_, objects_ref_, people_ref_)
      .build();
}

// ---------------------------------------------------------------------------
// Serialización JSON
// ---------------------------------------------------------------------------

json WorldStateNode::build_robot_json(const WorldState &state) const {
  json j;
  j["name"] = state.robot.name;
  j["location"] = state.robot.location;
  j["left_hand"] = state.robot.left_hand;
  j["right_hand"] = state.robot.right_hand;
  return j;
}

json WorldStateNode::build_objects_json(const WorldState &state) const {
  json j_objects = json::array();

  for (const auto &obj : state.objects) {
    json j_obj;
    j_obj["name"] = obj.name;
    j_obj["locations"] = obj.locations;
    j_objects.push_back(j_obj);
  }

  json result;
  result["objects"] = j_objects;
  return result;
}

json WorldStateNode::build_locations_json(const WorldState &state) const {
  json j;
  j["known_locations"] = state.known_locations;
  return j;
}

json WorldStateNode::build_people_json(const WorldState &state) const {
  json j_people = json::array();

  for (const auto &person : state.persons) {
    json j;
    j["name"] = person.name;
    j["gender"] = person.gender;
    j["age_group"] = person.age_group;
    j["emotion"] = person.emotion;
    j["hair_color"] = person.hair_color;
    j["hair_style"] = person.hair_style;
    j["shirt_color"] = person.shirt_color;
    j["room"] = person.room;
    j["accessories"] = person.accessories;
    j_people.push_back(j);
  }

  json result;
  result["people"] = j_people;
  return result;
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
    state_.robot.location = sym(robot["room"]);
  }
  if (robot.contains("left_hand")) {
    state_.robot.left_hand = sym(robot["left_hand"]);
  }
  if (robot.contains("right_hand")) {
    state_.robot.right_hand = sym(robot["right_hand"]);
  }
}

void WorldStateNode::update_object_state(const json &obj) {
  const std::string name = sym(obj.value("name", "unknown"));

  auto it =
      std::find_if(state_.objects.begin(), state_.objects.end(),
                   [&](const ObjectDescription &o) { return o.name == name; });

  if (it == state_.objects.end()) {
    ObjectDescription new_obj;
    new_obj.name = name;
    if (obj.contains("room")) {
      new_obj.locations.push_back(sym(obj["room"]));
    }
    state_.objects.push_back(new_obj);
    return;
  }

  if (obj.contains("room")) {
    it->locations = {sym(obj["room"])};
  }
}

void WorldStateNode::update_objects_table(const json &objects) {
  state_.objects.clear();
  for (const auto &obj : objects) {
    update_object_state(obj);
  }
}

void WorldStateNode::update_person_state(const json &person_json) {
  const std::string name = sym(person_json.value("name", "unknown"));

  auto it =
      std::find_if(state_.persons.begin(), state_.persons.end(),
                   [&](const PersonDescription &p) { return p.name == name; });

  if (it == state_.persons.end()) {
    PersonDescription person{};
    person.name = name;
    state_.persons.push_back(person);
    it = std::prev(state_.persons.end());
  }

  if (person_json.contains("room")) {
    it->room = sym(person_json["room"]);
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
  out << YAML::Key << "objects" << YAML::Value << YAML::BeginMap;
  for (const auto &obj : state.objects) {
    out << YAML::Key << obj.name << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "locations" << YAML::Value << YAML::BeginSeq;
    for (const auto &loc : obj.locations) {
      out << loc;
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;
  }
  out << YAML::EndMap;

  // People
  out << YAML::Key << "people" << YAML::Value << YAML::BeginSeq;
  for (const auto &person : state.persons) {
    out << YAML::BeginMap;
    out << YAML::Key << "name" << YAML::Value << person.name;
    out << YAML::Key << "gender" << YAML::Value << person.gender;
    out << YAML::Key << "age_group" << YAML::Value << person.age_group;
    out << YAML::Key << "emotion" << YAML::Value << person.emotion;
    out << YAML::Key << "hair_color" << YAML::Value << person.hair_color;
    out << YAML::Key << "hair_style" << YAML::Value << person.hair_style;
    out << YAML::Key << "shirt_color" << YAML::Value << person.shirt_color;
    out << YAML::Key << "room" << YAML::Value << person.room;
    out << YAML::Key << "accessories" << YAML::Value << YAML::BeginSeq;
    for (const auto &acc : person.accessories) {
      out << acc;
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
