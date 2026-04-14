/**
 * @brief Implementation of YamlPersistence.
 *
 * BioRobotics Laboratory, Facultad de Ingeniería, UNAM
 */

#include "knowledge_manager/yaml_persistence.hpp"

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

// ── Internal helpers
// ──────────────────────────────────────────────────────────

namespace {

/**
 * @brief Recursively convert a YAML::Node to a nlohmann::json value.
 *
 * yaml-cpp provides no built-in YAML->nlohmann::json converter, so we
 * traverse the node tree manually. The five YAML node types map to
 * JSON as follows:
 *
 *   Null      -> null
 *   Scalar    -> bool / int / double / string  (attempted in that order)
 *   Sequence  -> array
 *   Map       -> object
 *   Undefined -> null (treated as missing)
 */
json yamlToJson(const YAML::Node &node) {
  switch (node.Type()) {
  case YAML::NodeType::Null:
  case YAML::NodeType::Undefined:
    return nullptr;

  case YAML::NodeType::Scalar: {
    // Only accept canonical YAML boolean spellings to avoid treating
    // arbitrary strings (e.g. "unknown") as booleans.
    const std::string raw = node.Scalar();
    if (raw == "true" || raw == "True" || raw == "TRUE")
      return true;
    if (raw == "false" || raw == "False" || raw == "FALSE")
      return false;

    // Try integer.
    try {
      return std::stoll(raw);
    } catch (...) {
    }

    // Try float.
    try {
      return std::stod(raw);
    } catch (...) {
    }

    // Fall back to string.
    return raw;
  }

  case YAML::NodeType::Sequence: {
    json arr = json::array();
    for (const auto &child : node) {
      arr.push_back(yamlToJson(child));
    }
    return arr;
  }

  case YAML::NodeType::Map: {
    json obj = json::object();
    for (const auto &kv : node) {
      obj[kv.first.as<std::string>()] = yamlToJson(kv.second);
    }
    return obj;
  }
  }
  return nullptr; // unreachable — silences compiler warning
}

} // namespace

// ── Constructor
// ───────────────────────────────────────────────────────────────

YamlPersistence::YamlPersistence(std::filesystem::path runtime_yaml_path)
    : runtime_yaml_path_(std::move(runtime_yaml_path)) {}

// ── Public API
// ────────────────────────────────────────────────────────────────

void YamlPersistence::load(WorldModel &model) const {
  if (!std::filesystem::exists(runtime_yaml_path_)) {
    return; // First run -- world defaults from YamlLoader are already set.
  }

  YAML::Node doc;
  try {
    doc = YAML::LoadFile(runtime_yaml_path_.string());
  } catch (const YAML::Exception &e) {
    throw std::runtime_error("YamlPersistence: failed to parse " +
                             runtime_yaml_path_.string() + " -- " + e.what());
  }

  // Convert YAML -> nlohmann::json -> WorldModel.
  // yamlToJson() bridges the two libraries; WorldModel only ever sees
  // a JSON string, keeping the coupling unidirectional.
  try {
    const json j = yamlToJson(doc);
    model.updateFromJson(j.dump());
  } catch (const std::exception &e) {
    throw std::runtime_error(
        "YamlPersistence: failed to apply runtime state -- " +
        std::string(e.what()));
  }
}

void YamlPersistence::persist(const WorldModel &model) const {
  // JSON is a strict subset of YAML, so YAML::Load() parses it directly.
  // This gives us a valid YAML::Node without a custom JSON->YAML converter.
  YAML::Node doc;
  try {
    doc = YAML::Load(model.toJson());
  } catch (const YAML::Exception &e) {
    throw std::runtime_error(
        "YamlPersistence: JSON->YAML conversion failed -- " +
        std::string(e.what()));
  }

  // Atomic write: write to a temporary file, then rename.
  // rename() is atomic on POSIX filesystems, so readers never see
  // a half-written file.
  const auto tmp_path = runtime_yaml_path_.string() + ".tmp";

  {
    std::ofstream out(tmp_path);
    if (!out) {
      throw std::runtime_error("YamlPersistence: cannot open for writing: " +
                               tmp_path);
    }
    out << doc;
  }

  std::filesystem::rename(tmp_path, runtime_yaml_path_);
}

// ── Private helpers
// ───────────────────────────────────────────────────────────

// jsonStringToYaml is kept for API compatibility but the logic
// now lives inline in persist() above.
YAML::Node YamlPersistence::jsonStringToYaml(const std::string &json_str) {
  try {
    return YAML::Load(json_str);
  } catch (const YAML::Exception &e) {
    throw std::runtime_error(
        "YamlPersistence: JSON->YAML conversion failed -- " +
        std::string(e.what()));
  }
}
