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

  WorldState state_;
};
