#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <rclcpp/client.hpp>
#include <rclcpp/node.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/service.hpp>

#include "knowledge_interfaces/srv/get_phrase.hpp"
#include "knowledge_interfaces/srv/get_world_state.hpp"

using GetPhrase = knowledge_interfaces::srv::GetPhrase;
using GetWorldState = knowledge_interfaces::srv::GetWorldState;

// context -> key -> [phrases]
using PhraseMap =
    std::map<std::string, std::map<std::string, std::vector<std::string>>>;

class PhraseManagerNode : public rclcpp::Node {
public:
  PhraseManagerNode();

private:
  // --- Phrase loading ---
  void load_phrases();

  // --- Service callback ---
  void get_phrase_callback(const std::shared_ptr<GetPhrase::Request> request,
                           const std::shared_ptr<GetPhrase::Response> response);

  // --- Phrase resolution ---
  // Picks a random phrase for the given context and key.
  // If context is empty, searches across all contexts.
  std::string pick_random_phrase(const std::string &context,
                                 const std::string &key) const;

  // Resolves {category.id.attribute} or {category.attribute} placeholders
  // by querying /world/get_state
  std::string resolve_slots(const std::string &phrase) const;

  // Queries the world state service for a single token value
  std::string get_world_state_value(const std::string &token) const;

  // --- State ---
  PhraseMap phrases_;
  std::string phrases_dir_;

  // --- ROS interfaces ---
  rclcpp::CallbackGroup::SharedPtr service_callback_group_;
  rclcpp::CallbackGroup::SharedPtr client_callback_group_;
  rclcpp::Service<GetPhrase>::SharedPtr get_phrase_service_;
  rclcpp::Client<GetWorldState>::SharedPtr world_state_client_;
};
