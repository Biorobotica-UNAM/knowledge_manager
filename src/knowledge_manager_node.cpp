/**
 * @file knowledge_manager_node.cpp
 * @brief Implementation of KnowledgeManagerNode.
 *
 * BioRobotics Laboratory, Facultad de Ingeniería, UNAM
 */

#include "knowledge_manager/knowledge_manager_node.hpp"

#include <stdexcept>

// ── Constructor
// ───────────────────────────────────────────────────────────────

KnowledgeManagerNode::KnowledgeManagerNode()
    : Node("knowledge_manager"), yaml_loader_(resolveKeysDir()),
      yaml_persistence_(resolveRuntimeYaml()) {

  loadWorldState();

  get_state_srv_ =
      this->create_service<knowledge_interfaces::srv::GetWorldState>(
          "/world/get_state",
          std::bind(&KnowledgeManagerNode::handleGetState, this,
                    std::placeholders::_1, std::placeholders::_2));

  update_state_srv_ =
      this->create_service<knowledge_interfaces::srv::UpdateWorldState>(
          "/world/update_state",
          std::bind(&KnowledgeManagerNode::handleUpdateState, this,
                    std::placeholders::_1, std::placeholders::_2));

  RCLCPP_INFO(this->get_logger(), "Knowledge manager node ready.");
  RCLCPP_INFO(this->get_logger(), "Robot at: %s, holding: %s",
              world_.getRobotRoom().c_str(), world_.getRobotHolding().c_str());
}

// ── Initialisation
// ────────────────────────────────────────────────────────────

std::filesystem::path KnowledgeManagerNode::resolveKeysDir() {
  try {
    const std::string share =
        ament_index_cpp::get_package_share_directory("knowledge_bank");
    return std::filesystem::path(share) / "keys";
  } catch (const std::exception &e) {
    throw std::runtime_error(
        std::string("KnowledgeManagerNode: cannot find knowledge_bank — ") +
        e.what());
  }
}

std::filesystem::path KnowledgeManagerNode::resolveRuntimeYaml() {
  try {
    const std::string share =
        ament_index_cpp::get_package_share_directory("knowledge_bank");
    return std::filesystem::path(share) / "volatile" / "runtime.yaml";
  } catch (const std::exception &e) {
    throw std::runtime_error(
        std::string("KnowledgeManagerNode: cannot find knowledge_bank — ") +
        e.what());
  }
}

void KnowledgeManagerNode::loadWorldState() {
  // Step 1 — static vocabulary + world.yaml defaults
  try {
    yaml_loader_.loadAll(world_);
    RCLCPP_INFO(this->get_logger(), "Static world state loaded from keys/.");
  } catch (const std::exception &e) {
    RCLCPP_ERROR(this->get_logger(), "Failed to load static world state: %s",
                 e.what());
    throw;
  }

  // Step 2 — volatile runtime state (overwrites defaults for mutable fields)
  try {
    yaml_persistence_.load(world_);
    RCLCPP_INFO(this->get_logger(),
                "Runtime state loaded from volatile/runtime.yaml.");
  } catch (const std::exception &e) {
    // Non-fatal: missing or corrupt runtime.yaml → use defaults.
    RCLCPP_WARN(this->get_logger(),
                "Could not load runtime state (%s). Using defaults.", e.what());
  }
}

// ── Service handlers
// ──────────────────────────────────────────────────────────

void KnowledgeManagerNode::handleGetState(
    const std::shared_ptr<knowledge_interfaces::srv::GetWorldState::Request>,
    std::shared_ptr<knowledge_interfaces::srv::GetWorldState::Response>
        response) {
  response->json = world_.toJson();
}

void KnowledgeManagerNode::handleUpdateState(
    const std::shared_ptr<knowledge_interfaces::srv::UpdateWorldState::Request>
        request,
    std::shared_ptr<knowledge_interfaces::srv::UpdateWorldState::Response>
        response) {
  try {
    world_.updateFromJson(request->json);
    yaml_persistence_.persist(world_);

    response->success = true;
    response->message = "World updated.";
  } catch (const std::exception &e) {
    RCLCPP_ERROR(this->get_logger(), "Failed to update world state: %s",
                 e.what());
    response->success = false;
    response->message = e.what();
  }
}
