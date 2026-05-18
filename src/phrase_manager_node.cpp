#include "knowledge_manager/phrase_manager_node.hpp"

#include <algorithm>
#include <random>
#include <stdexcept>

#include <ament_index_cpp/get_package_share_directory.hpp>
#include <rclcpp/logging.hpp>

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
              "Phrase manager ready. %zu keys loaded, %zu slots loaded.",
              phrases_.size(), slots_.size());
}

void PhraseManagerNode::load_phrases() {
  phrases_ = PhraseLoader(phrases_dir_).build();
}

void PhraseManagerNode::load_slots() {
  YAML::Node root = YAML::LoadFile(slots_path_);

  for (const auto &node : root) {
    const std::string key = node.first.as<std::string>();
    // Cada slot tiene una lista; se toma el primer valor
    if (node.second.IsSequence() && node.second.size() > 0) {
      slots_[key] = node.second[0].as<std::string>();
    }
  }
}

void PhraseManagerNode::get_phrase_callback(
    const std::shared_ptr<GetPhrase::Request> request,
    const std::shared_ptr<GetPhrase::Response> response) {
  const std::string &key = request->phrase;

  RCLCPP_INFO(this->get_logger(), "Phrase requested: '%s'", key.c_str());

  if (phrases_.find(key) == phrases_.end()) {
    response->success = false;
    response->text = "";
    RCLCPP_WARN(this->get_logger(), "Phrase key not found: '%s'", key.c_str());
    return;
  }

  const std::string raw = pick_random_phrase(key);
  const std::string result = resolve_slots(raw);

  response->success = true;
  response->text = result;

  RCLCPP_INFO(this->get_logger(), "Phrase resolved: '%s'", result.c_str());
}

std::string
PhraseManagerNode::pick_random_phrase(const std::string &key) const {
  const auto &list = phrases_.at(key);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<std::size_t> dist(0, list.size() - 1);

  return list[dist(gen)];
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
