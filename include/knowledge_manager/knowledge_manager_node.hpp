#pragma once

#include <memory>
#include <shared_mutex>
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

  json build_objects_json(const WorldState &state, const std::string &category,
                          const std::string &id = "",
                          const std::string &attribute = "") const;

  json build_locations_json(const WorldState &state) const;

  json build_people_json(const WorldState &state, const std::string &category,
                         const std::string &id = "",
                         const std::string &attribute = "") const;

  json build_analytics_json(const WorldState &state,
                            const std::string &category,
                            const std::string &id = "",
                            const std::string &attribute = "") const;

  std::string build_world_json(const WorldState &state) const;

  // Actualización del estado en memoria
  // Reciben (category, id, attribute, value) — misma lógica que el get
  void update_robot_attribute(const std::string &attribute,
                              const std::string &value);

  void update_object_attribute(const std::string &category,
                               const std::string &id,
                               const std::string &attribute,
                               const std::string &value);

  void update_person_attribute(const std::string &category,
                               const std::string &id,
                               const std::string &attribute,
                               const std::string &value);

  void update_analytics_attribute(const std::string &id,
                                  const std::string &attribute,
                                  const std::string &value);

  // Persistencia
  void save_state(const WorldState &state,
                  const std::string &output_path) const;

  // Utilidades
  std::string normalize(const std::string &s) const;

  json person_to_json(const PersonDescription &person) const;
  json person_attribute_to_json(const PersonDescription &person,
                                const std::string &attribute) const;
  void apply_person_attribute(PersonDescription &person,
                              const std::string &attribute,
                              const std::string &value);

  json object_to_json(const ObjectDescription &obj) const;
  json object_attribute_to_json(const ObjectDescription &obj,
                                const std::string &attribute) const;
  void apply_object_attribute(ObjectDescription &obj,
                              const std::string &attribute,
                              const std::string &value);

  // Helpers de búsqueda en los maps dinámicos
  std::vector<ObjectDescription> &resolve_objects(const std::string &category);
  const std::vector<ObjectDescription> &
  resolve_objects(const std::string &category) const;

  std::vector<PersonDescription> &resolve_people(const std::string &category);
  const std::vector<PersonDescription> &
  resolve_people(const std::string &category) const;

  std::vector<ObjectDescription>::iterator
  find_object(std::vector<ObjectDescription> &objects, short int number);

  std::vector<ObjectDescription>::const_iterator
  find_object(const std::vector<ObjectDescription> &objects,
              short int number) const;

  std::vector<PersonDescription>::iterator
  find_person(std::vector<PersonDescription> &persons, short int number);

  std::vector<PersonDescription>::const_iterator
  find_person(const std::vector<PersonDescription> &persons,
              short int number) const;

  // Estado
  WorldState state_;

  // Mutex para lecturas concurrentes / escrituras exclusivas
  mutable std::shared_mutex state_mutex_;

  // Rutas
  std::string bank_path_;
  std::string volatile_path_;
  std::string world_path_;
  std::string output_path_;

  // Callback groups — permiten que get y update corran en hilos distintos
  rclcpp::CallbackGroup::SharedPtr cb_group_get_;
  rclcpp::CallbackGroup::SharedPtr cb_group_update_;

  // Servicios
  rclcpp::Service<GetWorldState>::SharedPtr get_state_service_;
  rclcpp::Service<UpdateWorldState>::SharedPtr update_state_service_;
};
