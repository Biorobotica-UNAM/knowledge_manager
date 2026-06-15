#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <yaml-cpp/yaml.h>

struct RobotDescription {
  std::string name{};
  std::string room{};
  std::string left_hand{};
  std::string right_hand{};
  std::string safe_drop_mode{};
  std::string safe_drop_frame{};
  std::vector<float> safe_drop_pose{};
};

struct ObjectDescription {
  short int number{};
  std::string name{};
  std::vector<std::string> locations{};
  std::vector<float> navigation{};
  std::vector<float> manipulation{};
};

struct PersonDescription {
  short int number{};
  std::string name{"unknown"};
  std::vector<std::string> description{};
  std::string drink{"unknown"};
  std::string room{"unknown"};
  std::vector<std::string> accessories{};
  std::vector<std::string> order{};
  std::vector<float> navigation{};
};

struct WorldState {
  RobotDescription robot{};
  std::vector<std::string> known_locations{};
  std::unordered_map<std::string, std::vector<ObjectDescription>> objects;
  std::unordered_map<std::string, std::vector<PersonDescription>> people;
};

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
