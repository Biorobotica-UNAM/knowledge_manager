/**
 * @file yaml_persistence.hpp
 * @brief Read/write persistence for volatile/runtime.yaml.
 *
 * Responsibility (SRP): load the last-known runtime state into a
 * WorldModel at startup, and flush the current state back to
 * volatile/runtime.yaml whenever the world changes.
 *
 * This class is the only one in the system allowed to write to disk.
 * WorldModel never calls it — the KnowledgeManagerNode orchestrates
 * the interaction after every successful update.
 *
 * Design notes:
 *   - SRP: YamlPersistence handles only I/O; WorldModel handles only
 *     state. Neither knows about the other's internals.
 *   - DIP: depends on WorldModel's toJson() / updateFromJson() API,
 *     not on YAML field names.
 *
 * Usage:
 * @code
 *   YamlPersistence persistence(runtime_yaml_path);
 *   persistence.load(world_model);   // at startup
 *   // ... after update_state call ...
 *   persistence.persist(world_model); // after every write
 * @endcode
 *
 * BioRobotics Laboratory, Facultad de Ingeniería, UNAM
 */

#pragma once

#include "knowledge_manager/world_model.hpp"

#include <filesystem>
#include <yaml-cpp/yaml.h>

/**
 * @brief Persists WorldModel state to volatile/runtime.yaml.
 *
 * The file format mirrors the JSON schema of WorldModel::toJson(),
 * converted to YAML for human readability and easy manual editing.
 */
class YamlPersistence {
public:
  /**
   * @param runtime_yaml_path  Absolute path to volatile/runtime.yaml.
   *
   * The file does not need to exist at construction time.
   * If missing, load() is a no-op and persist() creates it.
   */
  explicit YamlPersistence(std::filesystem::path runtime_yaml_path);

  /**
   * @brief Populate @p model from the last-persisted runtime state.
   *
   * No-op if the file does not exist (first run).
   * Throws std::runtime_error on malformed YAML.
   */
  void load(WorldModel &model) const;

  /**
   * @brief Overwrite volatile/runtime.yaml with the current world state.
   *
   * Converts WorldModel::toJson() to YAML and writes atomically
   * (write to a temp file, then rename) to avoid partial writes.
   *
   * Throws std::runtime_error if the file cannot be written.
   */
  void persist(const WorldModel &model) const;

private:
  std::filesystem::path runtime_yaml_path_;

  /// Convert a JSON string to a YAML::Node for writing.
  static YAML::Node jsonStringToYaml(const std::string &json_str);
};
