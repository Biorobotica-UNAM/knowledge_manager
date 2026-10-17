#pragma once

#include <string>
#include <vector>

#include "knowledge_manager/application/result.hpp"
#include "knowledge_manager/domain/known_location.hpp"
#include "knowledge_manager/domain/location_zone.hpp"
#include "knowledge_manager/domain/manipulable_object.hpp"
#include "knowledge_manager/domain/object_category.hpp"
#include "knowledge_manager/domain/person.hpp"
#include "knowledge_manager/domain/person_type.hpp"
#include "knowledge_manager/domain/robot.hpp"
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
/// Positions are 1-based and follow first registration order,
/// regardless of current ownership; they are not stable across
/// deletions (see KnowledgeRepository::deleteObject).
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

  /// Checks that the owner named by the object's ownership exists.
  /// Returns an empty string when it does, else the reason.
  std::string findOwnershipProblem(const domain::ManipulableObject & object);
};

} // namespace knowledge_manager::application
