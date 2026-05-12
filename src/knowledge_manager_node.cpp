#include "knowledge_manager/knowledge_manager_node.hpp"

#include <ament_index_cpp/get_package_share_directory.hpp>
#include <fstream>
#include <functional>
#include <memory>
#include <rclcpp/logging.hpp>
#include <yaml-cpp/emitter.h>
#include <yaml-cpp/emittermanip.h>
#include <yaml-cpp/yaml.h>

#include "loaders/default_loader.hpp"
#include "loaders/locations_loader.hpp"
#include "loaders/world_loader.hpp"

WorldStateNode::WorldStateNode() : Node("world_state_node") {
  state_ = build_initial_state();
  get_state_service_ = this->create_service<GetInterfaces>(
      "/world/get_state",
      std::bind(&WorldStateNode::get_world_state_callback, this,
                std::placeholders::_1, std::placeholders::_2));

  RCLCPP_INFO(this->get_logger(), "Knowkedge manager services ready.");
}

WorldState WorldStateNode::build_initial_state() {
  build_paths();
  build_references();

  // --- Build initial state ---
  WorldLoader world_loader(world_path_, locations_ref_, objects_ref_,
                           people_ref_);
  WorldState state = world_loader.build();

  RCLCPP_INFO(this->get_logger(),
              "World state built. Robot at: %s, holding: %s",
              state.robot.location.c_str(), state.robot.holding.c_str());

  save_state(state, output_path_);

  return state;
}

void WorldStateNode::get_world_state_callback(
    const std::shared_ptr<GetInterfaces::Request> request,
    const std::shared_ptr<GetInterfaces::Response> response) {
  WorldState current_state = build_current_state();
  std::string requested_field = request->field;

  RCLCPP_INFO(this->get_logger(), "Request recived: \'%s\'",
              requested_field.c_str());

  if (requested_field == "robot") {
    response->json = build_robot_json(current_state).dump(2);
    RCLCPP_INFO(this->get_logger(), "Response \'robot\' type.");
  } else if (requested_field == "objects") {
    response->json = build_objects_json(current_state).dump(2);
    RCLCPP_INFO(this->get_logger(), "Response \'objects\' type.");
  } else if (requested_field == "known_locations") {
    response->json = build_locations_json(current_state).dump(2);
    RCLCPP_INFO(this->get_logger(), "Response \'locations\' type.");
  } else if (requested_field == "people") {
    response->json = build_people_json(current_state).dump(2);
    RCLCPP_INFO(this->get_logger(), "Response \'people\' type.");
  } else if (requested_field == "") {
    response->json = build_world_json(current_state);
    RCLCPP_INFO(this->get_logger(), "Response \'world\' type.");
  } else {
    RCLCPP_INFO(this->get_logger(), "Invalid request.");
  }
}

void WorldStateNode::save_state(const WorldState &state,
                                const std::string &output_path) {
  YAML::Emitter out;
  out << YAML::BeginMap;

  // Robot
  out << YAML::Key << "robot" << YAML::Value;
  out << YAML::BeginMap;
  out << YAML::Key << "name" << YAML::Value << state.robot.name;
  out << YAML::Key << "location" << YAML::Value << state.robot.location;
  out << YAML::Key << "holding" << YAML::Value << state.robot.holding;
  out << YAML::EndMap;

  // Known locations
  out << YAML::Key << "known_locations" << YAML::Value;
  out << YAML::BeginSeq;
  for (const auto &loc : state.known_locations) {
    out << loc;
  }
  out << YAML::EndSeq;

  // Objects
  out << YAML::Key << "objects" << YAML::Value;
  out << YAML::BeginMap;
  for (const auto &obj : state.objects) {
    out << YAML::Key << obj.name << YAML::Value;
    out << YAML::BeginMap;
    out << YAML::Key << "locations" << YAML::Value;
    out << YAML::BeginSeq;
    for (const auto &loc : obj.locations) {
      out << loc;
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;
  }
  out << YAML::EndMap;

  // People
  out << YAML::Key << "people" << YAML::Value;
  out << YAML::BeginSeq;
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
    out << YAML::Key << "accessories" << YAML::Value;
    out << YAML::BeginSeq;
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

void WorldStateNode::build_paths() {
  bank_path_ = ament_index_cpp::get_package_share_directory("knowledge_bank");

  keys_path_ = bank_path_ + "/keys/";
  volatile_path_ = bank_path_ + "/volatile/";

  locations_path_ = keys_path_ + "locations.yaml";
  objects_path_ = keys_path_ + "objects.yaml";
  people_path_ = keys_path_ + "people.yaml";
  world_path_ = keys_path_ + "world.yaml";
  output_path_ = volatile_path_ + "runtime.yaml";
}

void WorldStateNode::build_references() {
  locations_ref_ = LocationsLoader(locations_path_).build();
  objects_ref_ = DefaultLoader(objects_path_).build();
  people_ref_ = DefaultLoader(people_path_).build();

  RCLCPP_INFO(
      this->get_logger(),
      "Referencies loaded: %zu locations, %zu objects, %zu people descriptions",
      locations_ref_.size(), objects_ref_.size(), people_ref_.size());
}

WorldState WorldStateNode::build_current_state() {
  WorldLoader current_world_loader(output_path_, locations_ref_, objects_ref_,
                                   people_ref_);

  WorldState current_state = current_world_loader.build();

  return current_state;
}

json WorldStateNode::build_objects_json(const WorldState &current_state) {
  json j_objects = json::array();

  for (const auto &obj : current_state.objects) {
    json j_obj;

    j_obj["name"] = obj.name;
    j_obj["locations"] = obj.locations;

    j_objects.push_back(j_obj);
  }

  json result;
  result["objects"] = j_objects;
  return result;
}

json WorldStateNode::build_locations_json(const WorldState &current_state) {
  json j_locations;
  j_locations["known_locations"] = current_state.known_locations;
  return j_locations;
}

json WorldStateNode::build_robot_json(const WorldState &current_state) {
  json j_robot;
  j_robot["name"] = current_state.robot.name;
  j_robot["location"] = current_state.robot.location;
  j_robot["holding"] = current_state.robot.holding;
  return j_robot;
}

json WorldStateNode::build_people_json(const WorldState &current_state) {
  json j_people = json::array();
  for (const auto &person : current_state.persons) {
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

std::string WorldStateNode::build_world_json(const WorldState &current_state) {

  json j_world;

  j_world["robot"] = build_robot_json(current_state);

  j_world["objects"] = build_objects_json(current_state);

  j_world["known_locations"] = build_locations_json(current_state);

  j_world["people"] = build_people_json(current_state);

  return j_world.dump(2);
}
