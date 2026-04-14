/**
 * @brief Implementation of YamlLoader.
 *
 * BioRobotics Laboratory, Facultad de Ingeniería, UNAM
 */

#include "knowledge_manager/yaml_loader.hpp"

#include <yaml-cpp/yaml.h>

#include <stdexcept>
#include <string>
#include <vector>

// ── Constructor
// ───────────────────────────────────────────────────────────────

YamlLoader::YamlLoader(std::filesystem::path keys_dir)
    : keys_dir_(std::move(keys_dir)) {}

// ── Public API
// ────────────────────────────────────────────────────────────────

void YamlLoader::loadAll(WorldModel &model) const {
  loadLocations(model);
  loadObjects(model);
  loadPeople(model);
  loadWorldDefaults(model);
}

// ── Private loaders
// ───────────────────────────────────────────────────────────

void YamlLoader::loadLocations(WorldModel &model) const {
  const YAML::Node doc = parseFile("locations.yaml");

  // "known_locations" lives inside world.yaml, but every place-name key
  // that appears under a room section is also a valid location symbol.
  // We collect the flattened place list from each room group.
  std::vector<std::string> locations;

  for (const auto &section : doc) {
    const std::string key = section.first.as<std::string>();

    // Skip *_name entries — those are human-readable labels, not symbols.
    if (key.size() > 5 && key.substr(key.size() - 5) == "_name") {
      continue;
    }

    if (section.second.IsSequence()) {
      for (const auto &place : section.second) {
        locations.push_back(place.as<std::string>());
      }
    }
  }

  model.setKnownLocations(locations);
}

void YamlLoader::loadObjects(WorldModel &model) const {
  const YAML::Node doc = parseFile("objects.yaml");

  std::vector<std::string> names;

  for (const auto &section : doc) {
    // Each top-level key is a category (drinks, cutlery, …).
    // The values are the actual object name strings.
    if (section.second.IsSequence()) {
      for (const auto &item : section.second) {
        names.push_back(item.as<std::string>());
      }
    }
  }

  model.setKnownObjectNames(names);
}

void YamlLoader::loadPeople(WorldModel &model) const {
  const YAML::Node doc = parseFile("people.yaml");

  std::vector<std::string> names;

  if (doc["names"] && doc["names"].IsSequence()) {
    for (const auto &name : doc["names"]) {
      names.push_back(name.as<std::string>());
    }
  }

  model.setKnownPersonNames(names);
}

void YamlLoader::loadWorldDefaults(WorldModel &model) const {
  const YAML::Node doc = parseFile("world.yaml");

  // Robot default room
  if (doc["room"] && doc["room"].IsSequence() && doc["room"].size() > 0) {
    model.setRobotRoom(doc["room"][0].as<std::string>());
  }

  // Default object locations
  // Convention: keys ending in "_location" are object-position entries.
  // The object name is derived by stripping "_location" from the key.
  for (const auto &entry : doc) {
    const std::string key = entry.first.as<std::string>();

    const std::string suffix = "_location";
    if (key.size() <= suffix.size()) {
      continue;
    }

    const bool ends_with_location =
        key.substr(key.size() - suffix.size()) == suffix;

    if (!ends_with_location) {
      continue;
    }

    const std::string object_name = key.substr(0, key.size() - suffix.size());

    if (entry.second.IsSequence() && entry.second.size() > 0) {
      const std::string room = entry.second[0].as<std::string>();
      model.setObjectLocation(object_name, room);
    }
  }
}

// ── Helpers
// ───────────────────────────────────────────────────────────────────

YAML::Node YamlLoader::parseFile(const std::string &filename) const {
  const auto path = keys_dir_ / filename;

  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("YamlLoader: required file not found: " +
                             path.string());
  }

  try {
    return YAML::LoadFile(path.string());
  } catch (const YAML::Exception &e) {
    throw std::runtime_error("YamlLoader: failed to parse " + path.string() +
                             " — " + e.what());
  }
}
