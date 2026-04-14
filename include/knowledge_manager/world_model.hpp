/**
 * @file world_model.hpp
 * @brief In-memory world state — the single source of truth.
 *
 * WorldModel holds the complete runtime state of the robot's environment.
 * It has no knowledge of YAML, ROS2, or any persistence mechanism.
 * It speaks only JSON (via nlohmann::json) so any transport layer can
 * serialise and deserialise it without coupling to its internals.
 *
 * Responsibilities (SRP):
 *   - Store robot state (room, holding).
 *   - Store object locations.
 *   - Store known persons and their perceptual attributes.
 *   - Store read-only vocabulary (valid locations, object names, person names).
 *   - Serialise to JSON  / deserialise from JSON.
 *
 * Design notes:
 *   - DIP: callers depend on this class, not on any file format.
 *   - OCP: new entity types (e.g. RoomState) can be added without
 *     modifying existing methods.
 *
 * BioRobotics Laboratory, Facultad de Ingeniería, UNAM
 */

#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ── Value types
// ───────────────────────────────────────────────────────────────

/**
 * @brief Physical state of one object in the environment.
 */
struct ObjectState {
  std::string name;
  std::string room;
};

/**
 * @brief Perceptual attributes of one person observed by the robot.
 *
 * All fields default to "unknown" so partial updates are safe.
 */
struct PersonState {
  std::string name = "unknown";
  std::string room = "unknown";
  std::string gender = "unknown";
  std::string age_group = "unknown";
  std::string shirt_color = "unknown";
  std::string hair_color = "unknown";
  std::string hair_style = "unknown";
  std::string emotion = "unknown";
  std::string accessories = "unknown";
};

// ── WorldModel
// ────────────────────────────────────────────────────────────────

/**
 * @brief Authoritative in-memory model of the robot's world.
 *
 * Invariant: all mutating operations keep the model consistent.
 * Callers that need persistence must serialise with toJson() and
 * hand the result to a YamlPersistence instance — this class does
 * not write to disk.
 */
class WorldModel {
public:
  // ── Robot state ────────────────────────────────────────────────────────────

  void setRobotRoom(const std::string &room);
  void setRobotHolding(const std::string &object_name);

  std::string getRobotRoom() const;
  std::string getRobotHolding() const;

  // ── Object state ───────────────────────────────────────────────────────────

  void setObjectLocation(const std::string &name, const std::string &room);

  /// Returns empty optional when the object is not tracked.
  std::optional<std::string> getObjectLocation(const std::string &name) const;

  std::vector<ObjectState> getObjects() const;

  // ── Person state ───────────────────────────────────────────────────────────

  /**
   * @brief Insert or fully replace the state of a person.
   *
   * Use this when a complete observation is available.
   */
  void setPerson(const PersonState &person);

  /**
   * @brief Partially update one person's attributes from a JSON patch.
   *
   * Only fields present in the patch are updated; absent fields keep
   * their current value. Creates the person entry if it does not exist.
   *
   * @param name   Person identifier (must match PersonState::name).
   * @param patch  JSON object with only the fields to update.
   */
  void updatePerson(const std::string &name, const nlohmann::json &patch);

  std::optional<PersonState> getPerson(const std::string &name) const;
  std::vector<PersonState> getPersons() const;

  // ── Read-only vocabulary (loaded once from keys/) ─────────────────────────

  void setKnownLocations(const std::vector<std::string> &locations);
  void setKnownObjectNames(const std::vector<std::string> &names);
  void setKnownPersonNames(const std::vector<std::string> &names);

  bool isKnownLocation(const std::string &location) const;
  bool isKnownObjectName(const std::string &name) const;
  bool isKnownPersonName(const std::string &name) const;

  const std::unordered_set<std::string> &knownLocations() const;
  const std::unordered_set<std::string> &knownObjectNames() const;
  const std::unordered_set<std::string> &knownPersonNames() const;

  // ── Serialisation ──────────────────────────────────────────────────────────

  /**
   * @brief Serialise the mutable world state to a JSON string.
   *
   * Vocabulary sets are intentionally excluded: they are static
   * and do not need to be persisted in volatile/runtime.yaml.
   *
   * Schema:
   * @code
   * {
   *   "robot":   { "room": "...", "holding": "..." },
   *   "objects": [ { "name": "...", "room": "..." }, ... ],
   *   "persons": [ { "name": "...", "room": "...", ... }, ... ]
   * }
   * @endcode
   */
  std::string toJson() const;

  /**
   * @brief Apply a JSON patch to the mutable world state.
   *
   * Unknown keys are silently ignored so partial updates are safe.
   * Each present key overwrites only the corresponding portion of
   * the state.
   *
   * Throws nlohmann::json::parse_error on malformed input.
   */
  void updateFromJson(const std::string &json_str);

private:
  // Mutable state
  std::string robot_room_ = "unknown";
  std::string robot_holding_ = "nothing";

  std::unordered_map<std::string, std::string> object_locations_;
  std::unordered_map<std::string, PersonState> persons_;

  // Read-only vocabulary
  std::unordered_set<std::string> known_locations_;
  std::unordered_set<std::string> known_object_names_;
  std::unordered_set<std::string> known_person_names_;

  // ── Private helpers ────────────────────────────────────────────────────────

  static PersonState personFromJson(const nlohmann::json &j);
  static nlohmann::json personToJson(const PersonState &p);
};
