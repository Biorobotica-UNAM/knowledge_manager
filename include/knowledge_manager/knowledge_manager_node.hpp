#pragma once

#include <rclcpp/node.hpp>
#include <rclcpp/rclcpp.hpp>
#include <string>
#include <vector>

#include "loaders/world_loader.hpp"

class WorldStateNode : public rclcpp::Node {
public:
  WorldStateNode();

private:
  WorldState build_initial_state();
  void save_state(const WorldState &state, const std::string &output_path);
  void build_paths();
  void build_references();

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
};
