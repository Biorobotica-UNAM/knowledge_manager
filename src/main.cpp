#include <memory>

#include <rclcpp/executors/multi_threaded_executor.hpp>
#include <rclcpp/rclcpp.hpp>

#include "knowledge_manager/knowledge_manager_node.hpp"
#include "knowledge_manager/phrase_manager_node.hpp"

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);

  auto world_node = std::make_shared<WorldStateNode>();
  auto phrase_node = std::make_shared<PhraseManagerNode>();

  rclcpp::executors::MultiThreadedExecutor executor;
  executor.add_node(world_node);
  executor.add_node(phrase_node);

  executor.spin();

  rclcpp::shutdown();
  return 0;
}
