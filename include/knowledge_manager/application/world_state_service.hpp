#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "knowledge_manager/application/result.hpp"
#include "knowledge_manager/domain/known_location.hpp"
#include "knowledge_manager/domain/location_zone.hpp"
#include "knowledge_manager/domain/manipulable_object.hpp"
#include "knowledge_manager/domain/object_category.hpp"
#include "knowledge_manager/domain/person.hpp"
#include "knowledge_manager/domain/person_type.hpp"
#include "knowledge_manager/domain/physical_description.hpp"
#include "knowledge_manager/domain/pose3d.hpp"
#include "knowledge_manager/domain/robot.hpp"
#include "knowledge_manager/domain/robot_hand.hpp"
#include "knowledge_manager/persistence/knowledge_repository.hpp"

namespace knowledge_manager::application
{

/// Facade over KnowledgeRepository: the one entry point through which
/// the rest of the package (ROSManager) reads and writes the world
/// state. It does two things the repository deliberately does not do:
///
///   1. It turns expected outcomes into Results. "Not found" (the
///      repository returns nullopt/empty), "position out of range" and
///      a reference to a person or location that does not exist are
///      business outcomes the caller must be told about; and every
///      RepositoryException (infrastructure failure) is caught here
///      and reported as a failed Result carrying its message. No
///      exception ever crosses this interface, so callers have a
///      single error channel (Clean Code: prefer one way to report
///      errors).
///   2. It answers positional queries (fruits_1, guest_2) on top of
///      the repository's first-registration order.
///
/// It does not hold business rules about the domain itself (e.g. arm
/// occupancy) -- those belong to consuming nodes such as the planner.
/// The rule "only one USER at a time" is enforced inside
/// KnowledgeRepository::savePerson, in the same transaction as the
/// save, so that it cannot be left half-applied; this class does not
/// repeat it.
///
/// References. Wherever a method takes a "reference" to an object or a
/// person, it accepts either the real name ("apple", "Ana") or a
/// positional alias "<kind>_<n>" ("fruits_1", "guest_2", "host_1"),
/// where <kind> is the lower-case ObjectCategory or PersonType name and
/// n is the 1-based position in first registration order, regardless
/// of current ownership. Positions are not stable across deletions
/// (see KnowledgeRepository::deleteObject). To keep a reference
/// unambiguous, no object or person may be named like an alias; saving
/// one with such a name fails.
class WorldStateService {
public:
  /// repository must outlive this service; it is not owned.
  explicit WorldStateService(persistence::KnowledgeRepository & repository);

  // -- Robot ---------------------------------------------------------
  Result<domain::Robot> getRobot();
  Result<void> updateRobot(const domain::Robot & robot);

  // -- ManipulableObject ---------------------------------------------
  Result<domain::ManipulableObject> getObjectByName(const std::string & name);
  Result<std::vector<domain::ManipulableObject>>
  getObjectsByCategory(domain::ObjectCategory category);
  Result<domain::ManipulableObject>
  getObjectByCategoryAndPosition(domain::ObjectCategory category, int position);

  /// Fails, without storing anything, when the object's ownership
  /// refers to a known location or a person that does not exist.
  Result<void> createOrUpdateObject(const domain::ManipulableObject & object);
  Result<void> deleteObject(const std::string & name);

  // -- Person --------------------------------------------------------
  Result<domain::Person> getPersonByName(const std::string & name);
  Result<domain::Person> getPersonByNameWithPhoto(const std::string & name);
  Result<std::vector<domain::Person>> getPersonsByType(domain::PersonType type);
  Result<std::vector<domain::Person>>
  getPersonsByTypeWithPhoto(domain::PersonType type);
  Result<domain::Person> getPersonByTypeAndPosition(
    domain::PersonType type,
    int position);
  Result<domain::Person>
  getPersonByTypeAndPositionWithPhoto(domain::PersonType type, int position);
  Result<void> createOrUpdatePerson(const domain::Person & person);
  Result<void> deletePerson(const std::string & name);

  // -- References: name or positional alias ----------------------------
  Result<domain::ManipulableObject> resolveObject(const std::string & reference);
  Result<domain::Person> resolvePerson(const std::string & reference);
  Result<domain::Person> resolvePersonWithPhoto(const std::string & reference);

  // -- Knowledge: what the robot knows across restarts -------------------
  /// Registers an object by what it is: name, category and usual zone.
  /// A new object starts with unknown ownership and no poses. If the
  /// object already exists only its category and usual zone change; its
  /// run state is kept.
  Result<void> registerObject(
    const std::string & name,
    domain::ObjectCategory category,
    domain::LocationZone usualZone);

  /// Registers a person at a known location. If the person already
  /// exists only the type and the location change.
  Result<void> registerPerson(
    const std::string & name, domain::PersonType type,
    const std::string & locationName);

  // -- Run state of objects ---------------------------------------------
  // Each of these states one fact about an object. Reading the object,
  // changing it and saving it happen here, in one place, so that callers
  // never do that round trip themselves.
  Result<void> setObjectAtLocation(
    const std::string & objectReference,
    const std::string & locationName);
  Result<void> setObjectHeldByPerson(
    const std::string & objectReference,
    const std::string & personReference);
  /// Fails when that hand already holds a different object.
  Result<void> setObjectHeldByRobot(
    const std::string & objectReference,
    domain::RobotHand hand);
  Result<void> forgetObjectOwnership(const std::string & objectReference);
  Result<void> setObjectNavigablePose(
    const std::string & objectReference,
    const domain::Pose3D & pose);
  Result<void> setObjectGraspPose(
    const std::string & objectReference,
    const domain::Pose3D & pose);

  // -- Run state of the robot --------------------------------------------
  Result<void> setRobotLocation(const std::string & locationName);

  // -- Run state of persons ----------------------------------------------
  Result<void> setPersonType(
    const std::string & personReference,
    domain::PersonType type);
  Result<void> setPersonLocation(
    const std::string & personReference,
    const std::string & locationName);
  Result<void> setPersonNavigablePose(
    const std::string & personReference,
    const domain::Pose3D & pose);
  Result<void>
  setPersonDescription(
    const std::string & personReference,
    const domain::PhysicalDescription & description);
  Result<void> setPersonPhoto(
    const std::string & personReference,
    const std::vector<std::byte> & photo);
  Result<void> setPersonFavoriteDrink(
    const std::string & personReference,
    const std::string & drink);
  /// objectReferences are resolved in order; if one fails nothing changes.
  Result<void> setPersonOrder(
    const std::string & personReference,
    const std::vector<std::string> & objectReferences);

  // -- KnownLocation ---------------------------------------------------
  Result<domain::KnownLocation> getKnownLocationByName(const std::string & name);
  Result<std::vector<domain::KnownLocation>> getAllKnownLocations();
  /// A zone without locations is a successful, empty result.
  Result<std::vector<domain::KnownLocation>>
  getKnownLocationsByZone(domain::LocationZone zone);
  Result<void>
  createOrUpdateKnownLocation(const domain::KnownLocation & location);

  // -- Run state -------------------------------------------------------
  Result<void> resetRuntimeState();

private:
  persistence::KnowledgeRepository & repository_;

  /// Returns an empty string when the object can be saved, else why not:
  /// its name looks like an alias, or its ownership refers to a known
  /// location or person that does not exist, or to a hand that already
  /// holds a different object.
  std::string findObjectProblem(const domain::ManipulableObject & object);

  /// Same for persons: only the name rule applies.
  std::string findPersonProblem(const domain::Person & person);

  Result<void>
  changeObject(
    const std::string & reference,
    const std::function<void(domain::ManipulableObject &)> & change);
  /// Unlike changeObject, the change returns the new Person, because
  /// Person has no setters for its type or location.
  Result<void> changePerson(
    const std::string & reference,
    const std::function<domain::Person(const domain::Person &)> & change);

  /// A copy of person with another type and location (Person has no
  /// setter for its type).
  static domain::Person rebuiltPerson(
    const domain::Person & person,
    domain::PersonType type,
    const domain::KnownLocation & location);
};

} // namespace knowledge_manager::application
