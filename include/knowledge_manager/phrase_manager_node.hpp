#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <rclcpp/node.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/service.hpp>

#include "knowledge_interfaces/srv/get_phrase.hpp"

using GetPhrase = knowledge_interfaces::srv::GetPhrase;

// context -> key -> [frases]
using PhraseMap =
    std::map<std::string, std::map<std::string, std::vector<std::string>>>;

class PhraseManagerNode : public rclcpp::Node {
public:
  PhraseManagerNode();

private:
  void load_phrases();
  void load_slots();

  void get_phrase_callback(const std::shared_ptr<GetPhrase::Request> request,
                           const std::shared_ptr<GetPhrase::Response> response);

  // Selecciona una frase aleatoria dado un context y una key.
  // Si context está vacío, busca en todos los contextos.
  std::string pick_random_phrase(const std::string &context,
                                 const std::string &key) const;

  // Reemplaza todas las ocurrencias de {token} con su valor en slots_
  std::string resolve_slots(const std::string &phrase) const;

  PhraseMap phrases_;
  std::map<std::string, std::string> slots_;

  std::string phrases_dir_;
  std::string slots_path_;

  rclcpp::Service<GetPhrase>::SharedPtr get_phrase_service_;
};
