#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

#include "ament_index_cpp/get_package_share_directory.hpp"
#include "rclcpp/rclcpp.hpp"

#include "knowledge_manager/application/world_state_service.hpp"
#include "knowledge_manager/persistence/knowledge_repository.hpp"
#include "knowledge_manager/persistence/repository_exception.hpp"
#include "knowledge_manager/ros_adapter/ros_logger.hpp"
#include "knowledge_manager/ros_adapter/ros_manager.hpp"

namespace
{

// Says which file the database really is, following symbolic links. A
// database inside an install/ folder is thrown away by a clean build, so
// that case is reported as a warning: knowledge_bank should be built with
// --symlink-install so install/ only links to the file kept in the
// repository (or database_path should point there).
void reportRealDatabasePath(
  const std::string & path,
  knowledge_manager::persistence::Logger & logger)
{
  std::error_code error;
  const std::string real =
    std::filesystem::weakly_canonical(path, error).string();
  if (error) {
    return;
  }
  logger.info("knowledge base file (real path): " + real);
  if (real.find("/install/") != std::string::npos) {
    logger.warn(
        "the knowledge base is inside an install/ folder: a clean build "
        "deletes it and git does not see it. Keep the file in the "
        "knowledge_bank repository and build with --symlink-install, or pass "
        "database_path.");
  }
}

} // namespace

// Wires the package together, in the order its layers depend on each
// other: logger -> repository -> world state service -> ROS services.
int main(int argc, char **argv)
{
  rclcpp::init(argc, argv);

  auto node = std::make_shared<rclcpp::Node>("knowledge_manager");
  knowledge_manager::ros_adapter::RosLogger logger(node->get_logger());

  // Where the knowledge base file lives. By default it is looked up in
  // the knowledge_bank package (share/knowledge_bank/runtime/); the
  // parameter overrides it, and then knowledge_bank is not needed.
  std::string databasePath =
    node->declare_parameter<std::string>("database_path", "");
  if (databasePath.empty()) {
    try {
      databasePath =
        ament_index_cpp::get_package_share_directory("knowledge_bank") +
        "/runtime/knowledge.db";
    } catch (const std::exception & e) {
      logger.error(
          std::string(
              "package knowledge_bank was not found (is its "
              "install/setup.bash sourced?) and no database_path was given: ") +
          e.what());
      rclcpp::shutdown();
      return 1;
    }
  }
  reportRealDatabasePath(databasePath, logger);
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
