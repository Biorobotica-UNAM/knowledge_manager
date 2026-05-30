#include "loaders/world_loader.hpp"

#include <algorithm>
#include <vector>
#include <yaml-cpp/node/node.h>

WorldLoader::WorldLoader(const std::string &world_path)
    : world_path_(world_path) {}

WorldState WorldLoader::build() {
  WorldState state{};
  YAML::Node root = YAML::LoadFile(world_path_);

  for (const auto &node : root) {
    const std::string key = node.first.as<std::string>();
    const YAML::Node &value = node.second;

    if (key == "robot") {
      state.robot = parse_robot(value);
    } else if (key == "known_locations") {
      state.known_locations = parse_known_locations(value);
    } else {
      if (!value["type"])
        continue;
      const std::string type = value["type"].as<std::string>();

      if (type == "object") {
        state.objects[key] = parse_objects(value["entries"]);
      } else if (type == "people") {
        state.people[key] = parse_people(value["entries"]);
      }
    }
  }

  return state;
}

RobotDescription WorldLoader::parse_robot(const YAML::Node &node) const {
  RobotDescription robot{};

  if (node["name"]) {
    robot.name = node["name"].as<std::string>();
  }
  if (node["room"]) {
    const std::string room = node["room"].as<std::string>();
    robot.room = room;
  }
  if (node["left_hand"]) {
    const std::string item = node["left_hand"].as<std::string>();
    robot.left_hand = item;
  }
  if (node["right_hand"]) {
    const std::string item = node["right_hand"].as<std::string>();
    robot.right_hand = item;
  }

  return robot;
}

std::vector<std::string>
WorldLoader::parse_known_locations(const YAML::Node &node) const {
  std::vector<std::string> locations;

  for (const auto &loc : node) {
    const std::string value = loc.as<std::string>();
    locations.push_back(value);
  }

  return locations;
}

std::vector<ObjectDescription>
WorldLoader::parse_objects(const YAML::Node &node) const {

  std::vector<ObjectDescription> objects;

  for (const auto &object_node : node) {

    auto field = [&](const std::string &key, auto default_value) {
      using T = decltype(default_value);
      return object_node[key] ? object_node[key].as<T>() : default_value;
    };

    const short int number = field("number", static_cast<short int>(0));
    const std::string name = field("name", std::string("unknown"));

    std::vector<std::string> locations;
    if (object_node["locations"] && object_node["locations"].IsSequence()) {
      for (const auto &loc : object_node["locations"]) {
        const std::string location = loc.as<std::string>();
        locations.push_back(location);
      }
    }

    std::vector<float> navigation;
    if (object_node["navigation"] && object_node["navigation"].IsSequence()) {
      for (const auto &coord : object_node["navigation"]) {
        navigation.push_back(coord.as<float>());
      }
    }

    std::vector<float> manipulation;
    if (object_node["manipulation"] &&
        object_node["manipulation"].IsSequence()) {
      for (const auto &coord : object_node["manipulation"]) {
        manipulation.push_back(coord.as<float>());
      }
    }

    ObjectDescription object{};
    object.number = number;
    object.name = name;
    object.locations = locations;
    object.navigation = navigation;
    object.manipulation = manipulation;

    objects.push_back(object);
  }

  return objects;
}

std::vector<PersonDescription>
WorldLoader::parse_people(const YAML::Node &node) const {
  std::vector<PersonDescription> persons;

  for (const auto &person_node : node) {

    auto field = [&](const std::string &key, auto default_value) {
      using T = decltype(default_value);
      return person_node[key] ? person_node[key].as<T>() : default_value;
    };

    const short int number = field("number", static_cast<short int>(0));

    const std::string name = field("name", std::string("unknown"));

    std::vector<std::string> description;
    if (person_node["description"] && person_node["description"].IsSequence()) {
      for (const auto &desc : person_node["description"]) {
        description.push_back(desc.as<std::string>());
      }
    }

    const std::string drink = field("drink", std::string("unknown"));

    const std::string room = field("room", std::string("unknown"));

    std::vector<std::string> accessories;
    if (person_node["accessories"] && person_node["accessories"].IsSequence()) {
      for (const auto &acc : person_node["accessories"]) {
        accessories.push_back(acc.as<std::string>());
      }
    }

    std::vector<std::string> order;
    if (person_node["order"] && person_node["order"].IsSequence()) {
      for (const auto &ord : person_node["order"]) {
        order.push_back(ord.as<std::string>());
      }
    }

    std::vector<float> navigation;
    if (person_node["navigation"] && person_node["navigation"].IsSequence()) {
      for (const auto &coord : person_node["navigation"]) {
        navigation.push_back(coord.as<float>());
      }
    }

    PersonDescription person{};
    person.number = number;
    person.name = name;
    person.description = description;
    person.drink = drink;
    person.room = room;
    person.accessories = accessories;
    person.order = order;
    person.navigation = navigation;

    persons.push_back(person);
  }

  return persons;
}
