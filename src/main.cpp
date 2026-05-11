#include "knowledge_manager/knowledge_manager_node.hpp"
#include <rclcpp/rclcpp.hpp>

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<WorldStateNode>());
  rclcpp::shutdown();
  return 0;
}
