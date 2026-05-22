#include "loaders/world_loader.hpp"

#include <algorithm>

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
    const std::string key = node.first.as<std::string>();

    if (key == "robot") {
      state.robot = parse_robot(node.second);
    } else if (key == "known_locations") {
      state.known_locations = parse_known_locations(node.second);
    } else if (key == "objects") {
      state.objects = parse_objects(node.second);
    } else if (key == "people") {
      state.persons = parse_people(node.second);
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
    robot.location = is_known_location(room) ? room : "unknown";
  }
  if (node["left_hand"]) {
    const std::string item = node["left_hand"].as<std::string>();
    robot.left_hand = is_known_object(item) ? item : "unknown";
  }
  if (node["right_hand"]) {
    const std::string item = node["right_hand"].as<std::string>();
    robot.right_hand = is_known_object(item) ? item : "unknown";
  }

  return robot;
}

std::vector<std::string>
WorldLoader::parse_known_locations(const YAML::Node &node) const {
  std::vector<std::string> locations;

  for (const auto &loc : node) {
    const std::string value = loc.as<std::string>();
    if (is_known_location(value)) {
      locations.push_back(value);
    }
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

    if (!is_known_object(name)) {
      continue;
    }

    std::vector<std::string> locations;
    if (object_node["locations"] && object_node["locations"].IsSequence()) {
      for (const auto &loc : object_node["locations"]) {
        const std::string location = loc.as<std::string>();
        if (is_known_location(location)) {
          locations.push_back(location);
        }
      }
    }

    ObjectDescription object{};
    object.number = number;
    object.name = name;
    object.locations = locations;

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
    const std::string gender = field("gender", std::string("unknown"));
    const std::string age_group = field("age_group", std::string("unknown"));
    const std::string emotion = field("emotion", std::string("unknown"));
    const std::string hair_color = field("hair_color", std::string("unknown"));
    const std::string hair_style = field("hair_style", std::string("unknown"));
    const std::string shirt_color =
        field("shirt_color", std::string("unknown"));
    const std::string drink = field("drink", std::string("unknown"));
    const std::string room = field("room", std::string("unknown"));

    std::vector<std::string> accessories;
    if (person_node["accessories"] && person_node["accessories"].IsSequence()) {
      for (const auto &acc : person_node["accessories"]) {
        accessories.push_back(acc.as<std::string>());
      }
    }

    const bool valid = is_valid_field(name, persons_ref_) &&
                       is_valid_field(gender, persons_ref_) &&
                       is_valid_field(age_group, persons_ref_) &&
                       is_valid_field(emotion, persons_ref_) &&
                       is_valid_field(hair_color, persons_ref_) &&
                       is_valid_field(hair_style, persons_ref_) &&
                       is_valid_field(shirt_color, persons_ref_) &&
                       is_known_location(room) &&
                       std::all_of(accessories.begin(), accessories.end(),
                                   [&](const std::string &acc) {
                                     return is_valid_field(acc, persons_ref_);
                                   });

    if (!valid) {
      continue;
    }

    PersonDescription person{};
    person.number = number;
    person.name = name;
    person.gender = gender;
    person.age_group = age_group;
    person.emotion = emotion;
    person.hair_color = hair_color;
    person.hair_style = hair_style;
    person.shirt_color = shirt_color;
    person.drink = drink;
    person.room = room;
    person.accessories = accessories;

    persons.push_back(person);
  }

  return persons;
}

bool WorldLoader::is_known_location(const std::string &location) const {
  return location == "unknown" || is_valid_field(location, locations_ref_);
}

bool WorldLoader::is_known_object(const std::string &object) const {
  return object == "nothing" || is_valid_field(object, objects_ref_);
}

bool WorldLoader::is_valid_field(const std::string &value,
                                 const std::vector<std::string> &ref) const {
  return std::binary_search(ref.begin(), ref.end(), value);
}
