/**
 * @file world_model.cpp
 * @brief Implementation of WorldModel.
 *
 * BioRobotics Laboratory, Facultad de Ingeniería, UNAM
 */

#include "knowledge_manager/world_model.hpp"

#include <optional>
#include <stdexcept>

using json = nlohmann::json;

// ── Robot state
// ───────────────────────────────────────────────────────────────

void WorldModel::setRobotRoom(const std::string &room) { robot_room_ = room; }

void WorldModel::setRobotHolding(const std::string &object_name) {
  robot_holding_ = object_name;
}

std::string WorldModel::getRobotRoom() const { return robot_room_; }

std::string WorldModel::getRobotHolding() const { return robot_holding_; }

// ── Object state
// ──────────────────────────────────────────────────────────────

void WorldModel::setObjectLocation(const std::string &name,
                                   const std::string &room) {
  object_locations_[name] = room;
}

std::optional<std::string>
WorldModel::getObjectLocation(const std::string &name) const {
  auto it = object_locations_.find(name);
  if (it == object_locations_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::vector<ObjectState> WorldModel::getObjects() const {
  std::vector<ObjectState> result;
  result.reserve(object_locations_.size());
  for (const auto &[name, room] : object_locations_) {
    result.push_back({name, room});
  }
  return result;
}

// ── Person state
// ──────────────────────────────────────────────────────────────

void WorldModel::setPerson(const PersonState &person) {
  persons_[person.name] = person;
}

void WorldModel::updatePerson(const std::string &name, const json &patch) {
  PersonState &p = persons_[name]; // default-constructs if absent
  p.name = name;

  if (patch.contains("room"))
    p.room = patch["room"];
  if (patch.contains("gender"))
    p.gender = patch["gender"];
  if (patch.contains("age_group"))
    p.age_group = patch["age_group"];
  if (patch.contains("shirt_color"))
    p.shirt_color = patch["shirt_color"];
  if (patch.contains("hair_color"))
    p.hair_color = patch["hair_color"];
  if (patch.contains("hair_style"))
    p.hair_style = patch["hair_style"];
  if (patch.contains("emotion"))
    p.emotion = patch["emotion"];
  if (patch.contains("accessories"))
    p.accessories = patch["accessories"];
}

std::optional<PersonState>
WorldModel::getPerson(const std::string &name) const {
  auto it = persons_.find(name);
  if (it == persons_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::vector<PersonState> WorldModel::getPersons() const {
  std::vector<PersonState> result;
  result.reserve(persons_.size());
  for (const auto &[_, person] : persons_) {
    result.push_back(person);
  }
  return result;
}

// ── Read-only vocabulary
// ──────────────────────────────────────────────────────

void WorldModel::setKnownLocations(const std::vector<std::string> &locations) {
  known_locations_ = {locations.begin(), locations.end()};
}

void WorldModel::setKnownObjectNames(const std::vector<std::string> &names) {
  known_object_names_ = {names.begin(), names.end()};
}

void WorldModel::setKnownPersonNames(const std::vector<std::string> &names) {
  known_person_names_ = {names.begin(), names.end()};
}

bool WorldModel::isKnownLocation(const std::string &location) const {
  return known_locations_.count(location) > 0;
}

bool WorldModel::isKnownObjectName(const std::string &name) const {
  return known_object_names_.count(name) > 0;
}

bool WorldModel::isKnownPersonName(const std::string &name) const {
  return known_person_names_.count(name) > 0;
}

const std::unordered_set<std::string> &WorldModel::knownLocations() const {
  return known_locations_;
}

const std::unordered_set<std::string> &WorldModel::knownObjectNames() const {
  return known_object_names_;
}

const std::unordered_set<std::string> &WorldModel::knownPersonNames() const {
  return known_person_names_;
}

// ── Serialisation
// ─────────────────────────────────────────────────────────────

std::string WorldModel::toJson() const {
  json j;

  j["robot"]["room"] = robot_room_;
  j["robot"]["holding"] = robot_holding_;

  j["objects"] = json::array();
  for (const auto &[name, room] : object_locations_) {
    j["objects"].push_back({{"name", name}, {"room", room}});
  }

  j["persons"] = json::array();
  for (const auto &[_, person] : persons_) {
    j["persons"].push_back(personToJson(person));
  }

  return j.dump(2);
}

void WorldModel::updateFromJson(const std::string &json_str) {
  const json j = json::parse(json_str); // throws on malformed input

  if (j.contains("robot")) {
    const auto &r = j["robot"];
    if (r.contains("room"))
      robot_room_ = r["room"];
    if (r.contains("holding"))
      robot_holding_ = r["holding"];
  }

  // "object" → single-object patch (e.g. from a manipulation event)
  if (j.contains("object")) {
    const auto &o = j["object"];
    const std::string name = o.value("name", "");
    const std::string room = o.value("room", "");
    if (!name.empty()) {
      object_locations_[name] = room;
    }
  }

  // "objects" → full replacement of the object table
  if (j.contains("objects") && j["objects"].is_array()) {
    for (const auto &o : j["objects"]) {
      const std::string name = o.value("name", "");
      const std::string room = o.value("room", "");
      if (!name.empty()) {
        object_locations_[name] = room;
      }
    }
  }

  // "person" → single-person patch (e.g. from HRI)
  if (j.contains("person")) {
    const auto &p = j["person"];
    const std::string name = p.value("name", "");
    if (!name.empty()) {
      updatePerson(name, p);
    }
  }

  // "persons" → full replacement of the persons table
  if (j.contains("persons") && j["persons"].is_array()) {
    for (const auto &p : j["persons"]) {
      const std::string name = p.value("name", "");
      if (!name.empty()) {
        persons_[name] = personFromJson(p);
      }
    }
  }
}

// ── Private helpers
// ───────────────────────────────────────────────────────────

PersonState WorldModel::personFromJson(const json &j) {
  PersonState p;
  p.name = j.value("name", "unknown");
  p.room = j.value("room", "unknown");
  p.gender = j.value("gender", "unknown");
  p.age_group = j.value("age_group", "unknown");
  p.shirt_color = j.value("shirt_color", "unknown");
  p.hair_color = j.value("hair_color", "unknown");
  p.hair_style = j.value("hair_style", "unknown");
  p.emotion = j.value("emotion", "unknown");
  p.accessories = j.value("accessories", "unknown");
  return p;
}

nlohmann::json WorldModel::personToJson(const PersonState &p) {
  return {
      {"name", p.name},
      {"room", p.room},
      {"gender", p.gender},
      {"age_group", p.age_group},
      {"shirt_color", p.shirt_color},
      {"hair_color", p.hair_color},
      {"hair_style", p.hair_style},
      {"emotion", p.emotion},
      {"accessories", p.accessories},
  };
}
