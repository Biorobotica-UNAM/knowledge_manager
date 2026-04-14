/**
 * @file yaml_loader.hpp
 * @brief Read-only loader for the static knowledge in knowledge_bank/keys/.
 *
 * Responsibility (SRP): read YAML files from the keys/ directory and
 * populate the read-only vocabulary and default object positions in a
 * WorldModel. Never writes to disk.
 *
 * Design notes:
 *   - DIP: depends on WorldModel's public mutators, not on file paths
 *     known at compile time.
 *   - OCP: new YAML sections can be added by extending loadAll()
 *     without touching any other class.
 *
 * Usage:
 * @code
 *   YamlLoader loader(package_share_dir);
 *   loader.loadAll(world_model);
 * @endcode
 *
 * BioRobotics Laboratory, Facultad de Ingeniería, UNAM
 */

#pragma once

#include "knowledge_manager/world_model.hpp"

#include <filesystem>
#include <string>
#include <yaml-cpp/yaml.h>

/**
 * @brief Loads static knowledge from knowledge_bank/keys/ into a WorldModel.
 *
 * All methods are const — this class never modifies its own state.
 */
class YamlLoader {
public:
  /**
   * @param keys_dir  Absolute path to the knowledge_bank/keys/ directory.
   *                  Obtained via ament_index_python / ament_index_cpp at
   *                  node startup.
   */
  explicit YamlLoader(std::filesystem::path keys_dir);

  /**
   * @brief Load all static YAML files and populate @p model.
   *
   * Files loaded (in order):
   *   - locations.yaml  → WorldModel::setKnownLocations()
   *   - objects.yaml    → WorldModel::setKnownObjectNames()
   *   - people.yaml     → WorldModel::setKnownPersonNames()
   *   - world.yaml      → object default locations, robot default room
   *
   * Throws std::runtime_error if any required file is missing or malformed.
   */
  void loadAll(WorldModel &model) const;

private:
  std::filesystem::path keys_dir_;

  void loadLocations(WorldModel &model) const;
  void loadObjects(WorldModel &model) const;
  void loadPeople(WorldModel &model) const;
  void loadWorldDefaults(WorldModel &model) const;

  /// Return the parsed YAML for @p filename, or throw if not found.
  YAML::Node parseFile(const std::string &filename) const;
};
