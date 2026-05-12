#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <rclcpp/node.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/service.hpp>
#include <string>
#include <vector>

#include <knowledge_interfaces/srv/get_world_state.hpp>

#include "knowledge_interfaces/srv/get_world_state.hpp"
#include "loaders/world_loader.hpp"

using GetInterfaces = knowledge_interfaces::srv::GetWorldState;
using json = nlohmann::json;

class WorldStateNode : public rclcpp::Node {
public:
  WorldStateNode();

private:
  WorldState build_initial_state();
  void get_world_state_callback(
      const std::shared_ptr<GetInterfaces::Request> request,
      const std::shared_ptr<GetInterfaces::Response> response);

  void save_state(const WorldState &state, const std::string &output_path);
  void build_paths();
  void build_references();

  WorldState build_current_state();

  json build_objects_json(const WorldState &current_state);
  json build_locations_json(const WorldState &current_state);
  json build_robot_json(const WorldState &current_state);
  json build_people_json(const WorldState &current_state);

  std::string build_world_json(const WorldState &current_state);

  WorldState state_;

  std::string bank_path_;
  std::string keys_path_;
  std::string volatile_path_;
  std::string locations_path_;
  std::string objects_path_;
  std::string people_path_;
  std::string world_path_;
  std::string output_path_;

  std::vector<std::string> locations_ref_;
  std::vector<std::string> objects_ref_;
  std::vector<std::string> people_ref_;

  rclcpp::Service<GetInterfaces>::SharedPtr get_state_service_;
};
