#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <rclcpp/node.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/service.hpp>

#include "knowledge_interfaces/srv/get_world_state.hpp"
#include "knowledge_interfaces/srv/update_world_state.hpp"
#include "loaders/world_loader.hpp"

using GetWorldState = knowledge_interfaces::srv::GetWorldState;
using UpdateWorldState = knowledge_interfaces::srv::UpdateWorldState;
using json = nlohmann::json;

class WorldStateNode : public rclcpp::Node {
public:
  WorldStateNode();

private:
  // Inicialización
  void build_paths();
  void build_references();
  WorldState build_initial_state();

  // Callbacks de servicios
  void get_world_state_callback(
      const std::shared_ptr<GetWorldState::Request> request,
      const std::shared_ptr<GetWorldState::Response> response);

  void update_world_state_callback(
      const std::shared_ptr<UpdateWorldState::Request> request,
      const std::shared_ptr<UpdateWorldState::Response> response);

  // Serialización a JSON
  json build_robot_json(const WorldState &state) const;
  json build_objects_json(const WorldState &state) const;
  json build_locations_json(const WorldState &state) const;
  json build_people_json(const WorldState &state) const;
  std::string build_world_json(const WorldState &state) const;

  // Actualización del estado en memoria
  void update_world_state(const std::string &json_str);
  void update_robot_state(const json &robot);
  void update_object_state(const json &object);
  void update_objects_table(const json &objects);
  void update_person_state(const json &person);
  void update_people_table(const json &persons);

  // Persistencia
  void save_state(const WorldState &state,
                  const std::string &output_path) const;

  // Utilidades
  std::string sym(const json &value) const;

  // Estado
  WorldState state_;

  // Rutas
  std::string bank_path_;
  std::string keys_path_;
  std::string volatile_path_;
  std::string locations_path_;
  std::string objects_path_;
  std::string people_path_;
  std::string world_path_;
  std::string output_path_;

  // Referencias de validación
  std::vector<std::string> locations_ref_;
  std::vector<std::string> objects_ref_;
  std::vector<std::string> people_ref_;

  // Servicios
  rclcpp::Service<GetWorldState>::SharedPtr get_state_service_;
  rclcpp::Service<UpdateWorldState>::SharedPtr update_state_service_;
};
