#include <rclcpp/rclcpp.hpp>

#include "knowledge_manager/phrase_manager_node.hpp"

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<PhraseManagerNode>());
  rclcpp::shutdown();
  return 0;
}
