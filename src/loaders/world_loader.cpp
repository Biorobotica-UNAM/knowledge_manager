#include "loaders/world_loader.hpp"
#include <algorithm>
#include <yaml-cpp/yaml.h>

WorldLoader::WorldLoader(const std::string &world_path,
                         const std::vector<std::string> &locations_ref,
                         const std::vector<std::string> &objects_ref,
                         const std::vector<std::string> &persons_ref)
    : world_path_(world_path), locations_ref_(locations_ref),
      objects_ref_(objects_ref), persons_ref_(persons_ref) {}

WorldState WorldLoader::build() {
  WorldState state{};

  YAML::Node root = YAML::LoadFile(world_path_);

  for (const auto &node : root) {
    std::string key = node.first.as<std::string>();

    if (key == "robot") {

      YAML::Node robot = node.second;

      if (robot["name"]) {
        std::string robot_name = robot["name"].as<std::string>();
        state.robot.name = robot_name;
      }
      if (robot["room"]) {
        std::string robot_room = robot["room"].as<std::string>();
        state.robot.location =
            (check_known_location(robot_room)) ? robot_room : "unknown";
      }

      if (robot["holding"]) {
        std::string robot_holding = robot["holding"].as<std::string>();

        state.robot.holding =
            (check_object(robot_holding)) ? robot_holding : "unknown";
      }

    } else if (key == "known_locations") {
      YAML::Node locations = node.second;
      for (const auto &loc : locations) {
        std::string validated_location{loc.as<std::string>()};
        if (check_known_location(validated_location)) {
          state.known_locations.push_back(validated_location);
        }
      }

    } else if (key == "objects") {
      YAML::Node objects = node.second;

      for (const auto &object_node : objects) {
        std::string object_name = object_node.first.as<std::string>();
        YAML::Node object_data = object_node.second;

        if (!check_object(object_name))
          continue;

        std::vector<std::string> locations{};

        if (object_data["locations"] && object_data["locations"].IsSequence()) {
          for (const auto &loc : object_data["locations"]) {
            std::string location = loc.as<std::string>();

            if (check_known_location(location)) {
              locations.push_back(location);
            }
          }
        }
        ObjectDescription object{};
        object.name = object_name;
        object.locations = locations;

        state.objects.push_back(object);
      }

    } else if (key == "people") {
      YAML::Node people = node.second;

      for (const auto &person_node : people) {
        std::string name = person_node["name"]
                               ? person_node["name"].as<std::string>()
                               : "unknown";
        std::string gender = person_node["gender"]
                                 ? person_node["gender"].as<std::string>()
                                 : "unknown";
        std::string age_group = person_node["age_group"]
                                    ? person_node["age_group"].as<std::string>()
                                    : "unknown";
        std::string emotion = person_node["emotion"]
                                  ? person_node["emotion"].as<std::string>()
                                  : "unknown";
        std::string hair_color =
            person_node["hair_color"]
                ? person_node["hair_color"].as<std::string>()
                : "unknown";
        std::string hair_style =
            person_node["hair_style"]
                ? person_node["hair_style"].as<std::string>()
                : "unknown";
        std::string shirt_color =
            person_node["shirt_color"]
                ? person_node["shirt_color"].as<std::string>()
                : "unknown";
        std::string room = person_node["room"]
                               ? person_node["room"].as<std::string>()
                               : "unknown";

        std::vector<std::string> accessories{};

        if (person_node["accessories"] &&
            person_node["accessories"].IsSequence()) {
          for (const auto &acc : person_node["accessories"]) {
            accessories.push_back(acc.as<std::string>());
          }
        }

        bool valid = is_valid_field(name, persons_ref_) &&
                     is_valid_field(gender, persons_ref_) &&
                     is_valid_field(age_group, persons_ref_) &&
                     is_valid_field(emotion, persons_ref_) &&
                     is_valid_field(hair_color, persons_ref_) &&
                     is_valid_field(hair_style, persons_ref_) &&
                     is_valid_field(shirt_color, persons_ref_) &&
                     check_known_location(room);

        for (const auto &acc : accessories) {
          valid = valid && is_valid_field(acc, persons_ref_);
        }

        if (valid) {
          PersonDescription person{};
          person.name = name;
          person.gender = gender;
          person.age_group = age_group;
          person.emotion = emotion;
          person.hair_color = hair_color;
          person.hair_style = hair_style;
          person.shirt_color = shirt_color;
          person.room = room;
          person.accessories = accessories;

          state.persons.push_back(person);
        }
      }
    }
  }

  return state;
}

bool WorldLoader::check_known_location(const std::string &location) {
  return is_valid_field(location, locations_ref_);
}

bool WorldLoader::check_object(const std::string &object) {
  return object == "nothing" || is_valid_field(object, objects_ref_);
}

bool WorldLoader::is_valid_field(const std::string &value,
                                 const std::vector<std::string> &ref) {
  return std::binary_search(ref.begin(), ref.end(), value);
}
