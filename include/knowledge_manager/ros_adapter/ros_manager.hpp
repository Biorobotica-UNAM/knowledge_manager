#pragma once

#include <memory>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"

#include "knowledge_manager/application/result.hpp"
#include "knowledge_manager/application/world_state_service.hpp"
#include "knowledge_manager/persistence/logger.hpp"

namespace knowledge_manager::ros_adapter
{

/// Exposes WorldStateService to the rest of the robot as ROS 2
/// services, under the prefix "knowledge_manager/" (for example
/// "knowledge_manager/get_object"). It adds no rules of its own: every
/// service reads its request, makes exactly one call to
/// WorldStateService and writes the outcome into the response.
///
/// Every response carries `success` and `error`. When `success` is
/// false the rest of the response is not meaningful.
///
/// Threading: the repository below is not thread-safe, so the node must
/// be spun with a single-threaded executor (rclcpp::spin does this).
class ROSManager {
public:
  /// node, world and logger must outlive this object.
  ROSManager(
    rclcpp::Node::SharedPtr node,
    application::WorldStateService & world,
    persistence::Logger & logger);

  /// Resets the run state of the knowledge base and then advertises
  /// all services. If the reset fails the failure is logged and
  /// returned, and no service is advertised: the node must not answer
  /// requests over a world state whose run state was not reset.
  /// Calling it a second time fails.
  application::Result<void> initialize();

private:
  rclcpp::Node::SharedPtr node_;
  application::WorldStateService & world_;
  persistence::Logger & logger_;
  std::vector<rclcpp::ServiceBase::SharedPtr> services_;

  void advertiseQueries();
  void advertiseKnowledgeCommands();
  void advertiseObjectStateCommands();
  void advertiseRobotAndPersonStateCommands();
  void advertiseAdministration();

  /// Advertises one service. operation is called as
  /// operation(request, response) and returns a Result<void>; it writes
  /// any payload into response itself.
  template<typename Srv, typename Operation>
  void serve(const std::string & name, Operation operation);

  /// The fixed shape of every service call: run the operation, never let
  /// an exception reach the executor, and put the outcome into
  /// response.success and response.error.
  template<typename Request, typename Response, typename Operation>
  void handleServiceCall(
    const Request & request, Response & response,
    Operation operation);
};

} // namespace knowledge_manager::ros_adapter
