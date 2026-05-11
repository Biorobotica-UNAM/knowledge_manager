#pragma once

#include <algorithm>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

struct RobotDescription {
  std::string name{};
  std::string location{};
  std::string holding{};
};

struct ObjectDescription {
  std::string name{};
  std::vector<std::string> location{};
};

struct PersonDescription {
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

  bool check_known_location(const std::string &location);
  bool check_object(const std::string &object);
  bool is_valid_field(const std::string &value,
                      const std::vector<std::string> &ref);
};
