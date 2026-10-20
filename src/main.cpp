#include <memory>
#include <string>

#include "ament_index_cpp/get_package_share_directory.hpp"
#include "rclcpp/rclcpp.hpp"

#include "knowledge_manager/application/world_state_service.hpp"
#include "knowledge_manager/persistence/knowledge_repository.hpp"
#include "knowledge_manager/persistence/repository_exception.hpp"
#include "knowledge_manager/ros_adapter/ros_logger.hpp"
#include "knowledge_manager/ros_adapter/ros_manager.hpp"

// Wires the package together, in the order its layers depend on each
// other: logger -> repository -> world state service -> ROS services.
int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);

  auto node = std::make_shared<rclcpp::Node>("knowledge_manager_node");
  knowledge_manager::ros_adapter::RosLogger logger(node->get_logger());

  const std::string databasePath = node->declare_parameter<std::string>(
      "database_path", "knowledge_base.db");
  const std::string schemaPath = node->declare_parameter<std::string>(
      "schema_path",
      ament_index_cpp::get_package_share_directory("knowledge_manager") +
          "/schema.sql");

  try {
    knowledge_manager::persistence::KnowledgeRepository repository(
      databasePath, schemaPath, logger);
    knowledge_manager::application::WorldStateService world(repository);
    knowledge_manager::ros_adapter::ROSManager manager(node, world, logger);

    if (manager.initialize().isFailure()) {
      rclcpp::shutdown();
      return 1;
    }
    // Single-threaded on purpose: the repository is not thread-safe.
    rclcpp::spin(node);
  } catch (const knowledge_manager::persistence::RepositoryException & e) {
    logger.error(std::string("the knowledge base could not be opened: ") +
                 e.what());
    rclcpp::shutdown();
    return 1;
  }

  rclcpp::shutdown();
  return 0;
}
