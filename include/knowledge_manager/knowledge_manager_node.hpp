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
  WorldState build_initial_state();

  // Callbacks de servicios
  void get_world_state_callback(
      const std::shared_ptr<GetWorldState::Request> request,
      const std::shared_ptr<GetWorldState::Response> response);

  void update_world_state_callback(
      const std::shared_ptr<UpdateWorldState::Request> request,
      const std::shared_ptr<UpdateWorldState::Response> response);

  // Serialización a JSON
  json build_robot_json(const WorldState &state,
                        const std::string &attribute = "") const;

  json build_objects_json(const WorldState &state,
                          const std::string &category = "",
                          const std::string &id = "",
                          const std::string &attribute = "") const;

  json build_locations_json(const WorldState &state) const;

  json build_people_json(const WorldState &state,
                         const std::string &category = "",
                         const std::string &id = "",
                         const std::string &attribute = "") const;

  std::string build_world_json(const WorldState &state) const;

  // Actualización del estado en memoria
  void update_world_state(const std::string &json_str);
  void update_robot_state(const json &robot);
  void update_object_state(const json &object, const std::string &category);
  void update_objects_table(const json &objects, const std::string &category);
  void update_person_state(const json &person, const std::string &category);
  void update_people_table(const json &persons, const std::string &category);

  // Persistencia
  void save_state(const WorldState &state,
                  const std::string &output_path) const;

  // Utilidades
  std::string sym(const json &value) const;

  json person_to_json(const PersonDescription &person) const;
  json person_attribute_to_json(const PersonDescription &person,
                                const std::string &attribute) const;

  std::vector<PersonDescription> &
  resolve_person_category(const std::string &category);
  const std::vector<PersonDescription> &
  resolve_person_category(const std::string &category) const;

  std::vector<PersonDescription>::const_iterator
  find_person_by_id(const std::vector<PersonDescription> &persons,
                    short int number) const;

  json object_to_json(const ObjectDescription &obj) const;
  json object_attribute_to_json(const ObjectDescription &obj,
                                const std::string &attribute) const;

  std::vector<ObjectDescription> &
  resolve_object_category(const std::string &category);
  const std::vector<ObjectDescription> &
  resolve_object_category(const std::string &category) const;

  std::vector<ObjectDescription>::const_iterator
  find_object_by_id(const std::vector<ObjectDescription> &objects,
                    short int number) const;

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

  // Servicios
  rclcpp::Service<GetWorldState>::SharedPtr get_state_service_;
  rclcpp::Service<UpdateWorldState>::SharedPtr update_state_service_;
};
