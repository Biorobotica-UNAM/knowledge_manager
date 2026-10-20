#pragma once

#include <string>

#include "rclcpp/rclcpp.hpp"

#include "knowledge_manager/persistence/logger.hpp"

namespace knowledge_manager::ros_adapter
{

/// Sends the package's Logger messages to the ROS 2 logging system, so
/// they show up in the node's console output and in rosout.
class RosLogger : public persistence::Logger {
public:
  explicit RosLogger(rclcpp::Logger logger)
  : logger_(std::move(logger)) {}

  void error(const std::string & message) override
  {
    RCLCPP_ERROR(logger_, "%s", message.c_str());
  }
  void warn(const std::string & message) override
  {
    RCLCPP_WARN(logger_, "%s", message.c_str());
  }
  void info(const std::string & message) override
  {
    RCLCPP_INFO(logger_, "%s", message.c_str());
  }

private:
  rclcpp::Logger logger_;
};

} // namespace knowledge_manager::ros_adapter
