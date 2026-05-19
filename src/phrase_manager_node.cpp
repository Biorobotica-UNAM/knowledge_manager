#include "knowledge_manager/phrase_manager_node.hpp"

#include <random>

#include <ament_index_cpp/get_package_share_directory.hpp>
#include <rclcpp/logging.hpp>
#include <yaml-cpp/yaml.h>

#include "loaders/phrase_loader.hpp"

PhraseManagerNode::PhraseManagerNode() : Node("phrase_manager_node") {
  const std::string bank_path =
      ament_index_cpp::get_package_share_directory("knowledge_bank");

  phrases_dir_ = bank_path + "/phrases/";
  slots_path_ = bank_path + "/tokens/slots.yaml";

  load_phrases();
  load_slots();

  get_phrase_service_ = this->create_service<GetPhrase>(
      "phrases/get_phrase",
      std::bind(&PhraseManagerNode::get_phrase_callback, this,
                std::placeholders::_1, std::placeholders::_2));

  RCLCPP_INFO(this->get_logger(),
              "Phrase manager ready. %zu contexts loaded, %zu slots loaded.",
              phrases_.size(), slots_.size());
}

void PhraseManagerNode::load_phrases() {
  phrases_ = PhraseLoader(phrases_dir_).build();
}

void PhraseManagerNode::load_slots() {
  YAML::Node root = YAML::LoadFile(slots_path_);

  for (const auto &node : root) {
    const std::string key = node.first.as<std::string>();
    if (node.second.IsSequence() && node.second.size() > 0) {
      slots_[key] = node.second[0].as<std::string>();
    }
  }
}

void PhraseManagerNode::get_phrase_callback(
    const std::shared_ptr<GetPhrase::Request> request,
    const std::shared_ptr<GetPhrase::Response> response) {
  const std::string &key = request->phrase;
  const std::string &context = request->context;

  RCLCPP_INFO(this->get_logger(), "Phrase requested: key='%s' context='%s'",
              key.c_str(), context.c_str());

  const std::string raw = pick_random_phrase(context, key);

  if (raw.empty()) {
    response->success = false;
    response->text = "";
    RCLCPP_WARN(this->get_logger(),
                "Phrase key '%s' not found in context '%s'.", key.c_str(),
                context.empty() ? "*" : context.c_str());
    return;
  }

  response->success = true;
  response->text = resolve_slots(raw);

  RCLCPP_INFO(this->get_logger(), "Phrase resolved: '%s'",
              response->text.c_str());
}

std::string
PhraseManagerNode::pick_random_phrase(const std::string &context,
                                      const std::string &key) const {
  // Recolectar candidatos según el contexto
  std::vector<std::string> candidates;

  if (context.empty()) {
    // Buscar en todos los contextos
    for (const auto &[ctx, keys] : phrases_) {
      auto it = keys.find(key);
      if (it != keys.end()) {
        candidates.insert(candidates.end(), it->second.begin(),
                          it->second.end());
      }
    }
  } else {
    auto ctx_it = phrases_.find(context);
    if (ctx_it != phrases_.end()) {
      auto key_it = ctx_it->second.find(key);
      if (key_it != ctx_it->second.end()) {
        candidates = key_it->second;
      }
    }
  }

  if (candidates.empty()) {
    return "";
  }

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<std::size_t> dist(0, candidates.size() - 1);

  return candidates[dist(gen)];
}

std::string PhraseManagerNode::resolve_slots(const std::string &phrase) const {
  std::string result = phrase;

  for (const auto &[token, value] : slots_) {
    const std::string placeholder = "{" + token + "}";
    std::size_t pos = 0;

    while ((pos = result.find(placeholder, pos)) != std::string::npos) {
      result.replace(pos, placeholder.size(), value);
      pos += value.size();
    }
  }

  return result;
}
