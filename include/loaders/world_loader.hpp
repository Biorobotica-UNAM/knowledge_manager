#pragma once

#include <yaml-cpp/yaml.h>

#include "loaders/structs.hpp"

class WorldLoader {
public:
  WorldLoader(const std::string &world_path);

  WorldState build();

private:
  std::string world_path_;

  RobotDescription parse_robot(const YAML::Node &node) const;
  std::vector<std::string> parse_known_locations(const YAML::Node &node) const;
  std::vector<ObjectDescription> parse_objects(const YAML::Node &node) const;
  std::vector<PersonDescription> parse_people(const YAML::Node &node) const;
};
