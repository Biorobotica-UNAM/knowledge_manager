#include "knowledge_manager/phrase_manager_node.hpp"

#include <random>
#include <regex>
#include <sstream>

#include <ament_index_cpp/get_package_share_directory.hpp>
#include <rclcpp/logging.hpp>

#include "loaders/phrase_loader.hpp"

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

PhraseManagerNode::PhraseManagerNode()
    : Node("phrase_manager_node",
           rclcpp::NodeOptions().use_intra_process_comms(false)) {

  const std::string bank_path =
      ament_index_cpp::get_package_share_directory("knowledge_bank");

  phrases_dir_ = bank_path + "/phrases/";

  // Separate callback groups so the client call inside the service callback
  // does not deadlock (requires a MultiThreadedExecutor).
  service_callback_group_ =
      this->create_callback_group(rclcpp::CallbackGroupType::Reentrant);

  client_callback_group_ =
      this->create_callback_group(rclcpp::CallbackGroupType::Reentrant);

  load_phrases();

  rclcpp::ServicesQoS service_qos;
  get_phrase_service_ = this->create_service<GetPhrase>(
      "phrases/get_phrase",
      std::bind(&PhraseManagerNode::get_phrase_callback, this,
                std::placeholders::_1, std::placeholders::_2),
      service_qos, service_callback_group_);

  world_state_client_ = this->create_client<GetWorldState>(
      "world/get_state", service_qos, client_callback_group_);

  RCLCPP_INFO(this->get_logger(),
              "Phrase manager ready. %zu contexts loaded. "
              "Waiting for world state service.",
              phrases_.size());
}

// ---------------------------------------------------------------------------
// Phrase loading
// ---------------------------------------------------------------------------

void PhraseManagerNode::load_phrases() {
  phrases_ = PhraseLoader(phrases_dir_).build();
}

// ---------------------------------------------------------------------------
// Service callback
// ---------------------------------------------------------------------------

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
    response->text.clear();
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

// ---------------------------------------------------------------------------
// Phrase selection
// ---------------------------------------------------------------------------

std::string
PhraseManagerNode::pick_random_phrase(const std::string &context,
                                      const std::string &key) const {
  std::vector<std::string> candidates;

  if (context.empty()) {
    for (const auto &[ctx, keys] : phrases_) {
      if (const auto it = keys.find(key); it != keys.end()) {
        candidates.insert(candidates.end(), it->second.begin(),
                          it->second.end());
      }
    }
  } else {
    if (const auto ctx_it = phrases_.find(context); ctx_it != phrases_.end()) {
      if (const auto key_it = ctx_it->second.find(key);
          key_it != ctx_it->second.end()) {
        candidates = key_it->second;
      }
    }
  }

  if (candidates.empty()) {
    return {};
  }

  std::mt19937 gen{std::random_device{}()};
  std::uniform_int_distribution<std::size_t> dist{0, candidates.size() - 1};
  return candidates[dist(gen)];
}

// ---------------------------------------------------------------------------
// Slot resolution
// ---------------------------------------------------------------------------

std::string PhraseManagerNode::resolve_slots(const std::string &phrase) const {
  static const std::regex kPlaceholder(R"(\{([^}]+)\})");

  std::string result = phrase;
  std::smatch match;

  auto search_start = result.cbegin();
  while (std::regex_search(search_start, result.cend(), match, kPlaceholder)) {
    const std::string token = match[1].str();
    std::string replacement = get_world_state_value(token);

    if (replacement.empty()) {
      RCLCPP_WARN(this->get_logger(),
                  "Could not resolve placeholder {%s} — leaving as-is.",
                  token.c_str());
      replacement = match[0].str();
    }

    const std::size_t pos =
        static_cast<std::size_t>(match.position(0)) +
        static_cast<std::size_t>(search_start - result.cbegin());

    result.replace(pos, static_cast<std::size_t>(match.length(0)), replacement);
    search_start = result.cbegin() + pos + replacement.size();
  }

  return result;
}

// ---------------------------------------------------------------------------
// World state query
// ---------------------------------------------------------------------------

std::string
PhraseManagerNode::get_world_state_value(const std::string &token) const {
  // Token formats:
  //   "category.attribute"        (e.g. robot.name)
  //   "category.id.attribute"     (e.g. objects.1.name)

  std::vector<std::string> parts;
  {
    std::stringstream ss(token);
    std::string part;
    while (std::getline(ss, part, '.')) {
      parts.push_back(std::move(part));
    }
  }

  if (parts.size() < 2 || parts.size() > 3) {
    RCLCPP_ERROR(this->get_logger(),
                 "Invalid placeholder format: '%s' "
                 "(expected 'cat.attr' or 'cat.id.attr')",
                 token.c_str());
    return {};
  }

  if (!world_state_client_->wait_for_service(std::chrono::seconds(1))) {
    RCLCPP_ERROR(this->get_logger(), "World state service not available.");
    return {};
  }

  auto request = std::make_shared<GetWorldState::Request>();
  if (parts.size() == 2) {
    request->category = parts[0];
    request->id = "";
    request->attribute = parts[1];
  } else {
    request->category = parts[0];
    request->id = parts[1];
    request->attribute = parts[2];
  }

  auto future = world_state_client_->async_send_request(request);

  if (future.wait_for(std::chrono::seconds(1)) != std::future_status::ready) {
    RCLCPP_ERROR(this->get_logger(),
                 "Timeout calling world state for token '%s'.", token.c_str());
    return {};
  }

  std::string value = future.get()->json;

  // Strip surrounding quotes if present (JSON string value)
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    value = value.substr(1, value.size() - 2);
  }

  return value;
}
