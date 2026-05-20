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
  std::string name{};
  std::vector<std::string> locations{};
};

struct PersonDescription {
  short int number{};
  std::string name{"unknown"};
  std::string gender{"unknown"};
  std::string age_group{"unknown"};
  std::string emotion{"unknown"};
  std::string hair_color{"unknown"};
  std::string hair_style{"unknown"};
  std::string shirt_color{"unknown"};
  std::string room{"unknown"};
  std::vector<std::string> accessories{};
};

struct WorldState {
  RobotDescription robot{};
  std::vector<ObjectDescription> objects{};
  std::vector<std::string> known_locations{};
  std::vector<PersonDescription> persons{};
};

class WorldLoader {
public:
  WorldLoader(const std::string &world_path,
              const std::vector<std::string> &locations_ref,
              const std::vector<std::string> &objects_ref,
              const std::vector<std::string> &persons_ref);

  WorldState build();

private:
  std::string world_path_;
  const std::vector<std::string> &locations_ref_;
  const std::vector<std::string> &objects_ref_;
  const std::vector<std::string> &persons_ref_;

  bool is_known_location(const std::string &location) const; // acepta "unknown"
  bool is_known_object(const std::string &object) const;     // acepta "nothing"
  bool is_valid_field(const std::string &value,
                      const std::vector<std::string> &ref) const;

  RobotDescription parse_robot(const YAML::Node &node) const;
  std::vector<std::string> parse_known_locations(const YAML::Node &node) const;
  std::vector<ObjectDescription> parse_objects(const YAML::Node &node) const;
  std::vector<PersonDescription> parse_people(const YAML::Node &node) const;
};
