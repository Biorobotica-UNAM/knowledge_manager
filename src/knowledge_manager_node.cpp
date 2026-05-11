#include "knowledge_manager/knowledge_manager_node.hpp"

#include <ament_index_cpp/get_package_share_directory.hpp>
#include <fstream>
#include <rclcpp/logging.hpp>
#include <yaml-cpp/emitter.h>
#include <yaml-cpp/emittermanip.h>
#include <yaml-cpp/yaml.h>

#include "loaders/default_loader.hpp"
#include "loaders/locations_loader.hpp"
#include "loaders/world_loader.hpp"

WorldStateNode::WorldStateNode() : Node("world_state_node") {
  state_ = build_initial_state();
}

WorldState WorldStateNode::build_initial_state() {
  // --- Paths ---
  std::string bank_path =
      ament_index_cpp::get_package_share_directory("knowledge_bank");

  std::string keys_path = bank_path + "/keys/";
  std::string volatile_path = bank_path + "/volatile/";

  std::string locations_path = keys_path + "locations.yaml";
  std::string objects_path = keys_path + "objects.yaml";
  std::string people_path = keys_path + "people.yaml";
  std::string world_path = keys_path + "world.yaml";
  std::string output_path = volatile_path + "runtime.yaml";

  // --- Load references ---
  auto locations_ref = LocationsLoader(locations_path).build();
  auto objects_ref = DefaultLoader(objects_path).build();
  auto people_ref = DefaultLoader(people_path).build();

  RCLCPP_INFO(
      this->get_logger(),
      "Referencies loaded: %zu locations, %zu objects, %zu people descriptions",
      locations_ref.size(), objects_ref.size(), people_ref.size());

  // --- Build initial state ---
  WorldLoader world_loader(world_path, locations_ref, objects_ref, people_ref);
  WorldState state = world_loader.build();

  RCLCPP_INFO(this->get_logger(),
              "World state built. Robot at: %s, holding: %s",
              state.robot.location.c_str(), state.robot.holding.c_str());

  save_state(state, output_path);

  return state;
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
