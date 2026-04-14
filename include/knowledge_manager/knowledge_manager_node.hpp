/**
 * @file knowledge_manager_node.hpp
 * @brief ROS2 node that exposes the world state as services.
 *
 * Responsibility (SRP): handle ROS2 communication and orchestrate
 * the interaction between WorldModel, YamlLoader, and YamlPersistence.
 * Contains no YAML parsing, no JSON logic, and no world-state business rules.
 *
 * Services exposed:
 *   /world/get_state    (knowledge_interfaces/GetWorldState)
 *   /world/update_state (knowledge_interfaces/UpdateWorldState)
 *
 * Design patterns applied:
 *   Facade   (GoF Structural)  — hides the three-component pipeline
 *                                (loader, model, persistence) behind
 *                                two simple service endpoints.
 *
 * Principles applied:
 *   SRP  — this class handles only ROS2 communication.
 *   DIP  — depends on WorldModel, YamlLoader, and YamlPersistence
 *           abstractions, not on YAML or JSON directly.
 *
 * Startup sequence:
 *   1. Resolve the knowledge_bank package share directory.
 *   2. YamlLoader::loadAll()  → populates vocabulary + world defaults.
 *   3. YamlPersistence::load() → overlays last-known runtime state.
 *      (Step 3 overwrites step 2 for mutable fields, so volatile
 *       values always win over static defaults.)
 *
 * BioRobotics Laboratory, Facultad de Ingeniería, UNAM
 */

#pragma once

#include "knowledge_interfaces/srv/get_world_state.hpp"
#include "knowledge_interfaces/srv/update_world_state.hpp"
#include "knowledge_manager/world_model.hpp"
#include "knowledge_manager/yaml_loader.hpp"
#include "knowledge_manager/yaml_persistence.hpp"
#include "rclcpp/rclcpp.hpp"

#include <ament_index_cpp/get_package_share_directory.hpp>
#include <filesystem>
#include <memory>

class KnowledgeManagerNode : public rclcpp::Node {
public:
  KnowledgeManagerNode();

private:
  // ── World state ────────────────────────────────────────────────────────────
  WorldModel world_;
  YamlLoader yaml_loader_;
  YamlPersistence yaml_persistence_;

  // ── ROS2 services ──────────────────────────────────────────────────────────
  rclcpp::Service<knowledge_interfaces::srv::GetWorldState>::SharedPtr
      get_state_srv_;
  rclcpp::Service<knowledge_interfaces::srv::UpdateWorldState>::SharedPtr
      update_state_srv_;

  // ── Initialisation ─────────────────────────────────────────────────────────

  /**
   * @brief Resolve the absolute path to knowledge_bank/keys/.
   *
   * Uses ament_index to find the package share directory at runtime,
   * so the node works regardless of where the workspace is installed.
   *
   * Throws std::runtime_error if the package is not found.
   */
  static std::filesystem::path resolveKeysDir();

  /**
   * @brief Resolve the absolute path to volatile/runtime.yaml.
   */
  static std::filesystem::path resolveRuntimeYaml();

  /**
   * @brief Load static vocabulary and last-known runtime state into world_.
   *
   * Called once in the constructor. The load order guarantees that
   * volatile/runtime.yaml values override world.yaml defaults.
   */
  void loadWorldState();

  // ── Service handlers ───────────────────────────────────────────────────────

  void handleGetState(
      const std::shared_ptr<knowledge_interfaces::srv::GetWorldState::Request>,
      std::shared_ptr<knowledge_interfaces::srv::GetWorldState::Response>
          response);

  void handleUpdateState(
      const std::shared_ptr<
          knowledge_interfaces::srv::UpdateWorldState::Request>
          request,
      std::shared_ptr<knowledge_interfaces::srv::UpdateWorldState::Response>
          response);
};
