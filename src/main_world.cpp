#include "knowledge_manager/knowledge_manager_node.hpp"
#include <rclcpp/executors/multi_threaded_executor.hpp>
#include <rclcpp/rclcpp.hpp>

int main(int argc, char *argv[]) {
  rclcpp::init(argc, argv);

  auto node = std::make_shared<WorldStateNode>();

  rclcpp::executors::MultiThreadedExecutor executor(rclcpp::ExecutorOptions{},
                                                    3);
  executor.add_node(node);
  executor.spin();

  rclcpp::shutdown();
  return 0;
}
