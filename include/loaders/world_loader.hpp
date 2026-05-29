#pragma once

#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

struct RobotDescription {
  std::string name{};
  std::string location{};
  std::string left_hand{};
  std::string right_hand{};
};

struct ObjectDescription {
  short int number{};
  std::string name{};
  std::vector<std::string> locations{};
  std::vector<float> navigation{};
  std::vector<float> manipulation{};
};

struct ObjectCategory {
  std::vector<ObjectDescription> cutlery;
  std::vector<ObjectDescription> tableware;
  std::vector<ObjectDescription> trash;
  std::vector<ObjectDescription> breakfast_items;
  std::vector<ObjectDescription> cabinet_objects;
  std::vector<ObjectDescription> dishwasher_tab;
  std::vector<ObjectDescription> floor_objects;
  std::vector<ObjectDescription> people;
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

struct PersonCategory {
  std::vector<PersonDescription> guest;
  std::vector<PersonDescription> host;
  std::vector<PersonDescription> customer;
};

struct WorldState {
  RobotDescription robot{};
  ObjectCategory objects{};
  std::vector<std::string> known_locations{};
  PersonCategory people{};
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
