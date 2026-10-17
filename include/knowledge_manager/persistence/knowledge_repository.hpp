#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "knowledge_manager/domain/known_location.hpp"
#include "knowledge_manager/domain/location_zone.hpp"
#include "knowledge_manager/domain/manipulable_object.hpp"
#include "knowledge_manager/domain/object_category.hpp"
#include "knowledge_manager/domain/person.hpp"
#include "knowledge_manager/domain/person_type.hpp"
#include "knowledge_manager/domain/robot.hpp"
#include "knowledge_manager/persistence/logger.hpp"

// Forward-declared instead of #included: SQLiteCpp/sqlite3 details must
// never leak into this header. Callers of KnowledgeRepository should be
// able to compile without ever knowing SQLite is involved -- this is
// the "deep module, narrow interface" principle (A Philosophy of
// Software Design): the interface below is small; all the complexity
// of SQL, joins, and exclusive-arc reconciliation lives in the .cpp.
namespace SQLite
{
class Database;
}

namespace knowledge_manager::persistence
{

/// Gives the illusion of an in-memory collection of Robot,
/// ManipulableObject, Person and KnownLocation instances, while hiding
/// that they are actually persisted in a SQLite relational database. It
/// is the sole point of translation between the in-memory domain
/// classes and their relational representation, and the sole component
/// that knows the schema, foreign keys, deletion order and
/// transactions.
///
/// What survives a process restart is knowledge: KnownLocation entries
/// (with their zone), each object's name, category and usual zone, the
/// robot's name and start location, and persons of type HOST.
/// Everything that is run state (object ownership and poses, the
/// robot's current location and hands, non-host persons) is reset at
/// startup through a single operation, resetRuntimeState.
///
/// Error-handling strategy (see class-level notes on Logger and
/// RepositoryException):
///   - "Not found" is an expected outcome, represented by returning
///     a null object (std::optional::nullopt, or an empty vector for
///     plural queries) -- never an exception.
///   - Infrastructure failures (constraint violations, I/O errors, a
///     corrupted database) are unexpected from the caller's point of
///     view and are reported by throwing RepositoryException, after
///     first recording the failure via the injected Logger.
///   - This class does NOT return a Result type. Result is owned by
///     WorldStateService, the layer above, which is where expected,
///     recoverable business outcomes ("invalid category", "object not
///     found") belong. Introducing Result here too would duplicate
///     the same concept across two layers.
class KnowledgeRepository {
public:
  /// Opens (creating if necessary) the SQLite database at dbPath, and
  /// ensures the schema exists by executing the .sql file found at
  /// schemaPath (idempotent: safe to run against an already-
  /// initialized database, via "CREATE TABLE IF NOT EXISTS"). A
  /// database file created with an older schema version is not
  /// migrated; it must be deleted once. logger is used to record
  /// infrastructure-level events; it is expected to outlive this
  /// repository.
  KnowledgeRepository(
    const std::string & dbPath, const std::string & schemaPath,
    Logger & logger);

  // Not copyable: a KnowledgeRepository owns a single underlying
  // database connection, which should not be silently duplicated.
  KnowledgeRepository(const KnowledgeRepository &) = delete;
  KnowledgeRepository & operator=(const KnowledgeRepository &) = delete;

  ~KnowledgeRepository();

  // -- Robot ---------------------------------------------------------

  /// Persists the robot's current state to the database, including its
  /// start location, replacing any previously stored state (there is
  /// only ever one robot). Whichever hand holds an object, if any, is
  /// reflected in that object's own ObjectOwnership when it is
  /// separately loaded.
  void saveRobot(const domain::Robot & robot);

  /// Reconstructs and returns the robot's state from the database,
  /// including resolving which ManipulableObject, if any, each hand
  /// currently holds. Returns std::nullopt if no robot has been stored
  /// yet: there is no meaningful "empty" Robot to return.
  std::optional<domain::Robot> loadRobot();

  // -- ManipulableObject ----------------------------------------------

  /// Inserts or updates a manipulable object in the database,
  /// including its usual zone and its current ObjectOwnership
  /// (unknown, location, person, or robot hand) as the object's
  /// exclusive-arc state.
  /// If an object with the same name already exists it is updated in
  /// place (upsert, never delete-and-reinsert) and keeps its original
  /// registration position; a new object is registered at the end of
  /// the order. A null pose on the argument keeps the stored pose.
  void saveObject(const domain::ManipulableObject & object);

  /// Retrieves a manipulable object by name, reconstructing its usual
  /// zone and its current ObjectOwnership from the stored state.
  /// Returns std::nullopt if no object exists with that name.
  std::optional<domain::ManipulableObject>
  loadObjectByName(const std::string & name);

  /// Retrieves all manipulable objects belonging to the given
  /// category, each with its usual zone and current ObjectOwnership
  /// resolved. Objects are returned in order of first registration
  /// (FIFO), regardless of their current ObjectOwnership. This order
  /// is part of the contract:
  /// WorldStateService::getObjectByCategoryAndPosition depends on it.
  std::vector<domain::ManipulableObject>
  loadObjectsByCategory(domain::ObjectCategory category);

  /// Removes a manipulable object (and the pose rows it owned) from
  /// the database by name. Deleting an object shifts the position of
  /// every object registered after it within its category. Must not be
  /// called while a running plan refers to objects by position
  /// (e.g. fruits_1).
  void deleteObject(const std::string & name);

  // -- Person ----------------------------------------------------------

  /// Inserts or updates a person in the database, including their
  /// photo (as a BLOB) when person.getPhoto() holds a value.
  void savePerson(const domain::Person & person);

  /// Retrieves a person by name, with getPhoto() left std::nullopt
  /// regardless of whether a photo is stored. This is the default
  /// retrieval, intended for callers that only need identity, type,
  /// description, or location data, avoiding the cost of reading the
  /// photo BLOB. Returns std::nullopt if no person exists with that
  /// name.
  std::optional<domain::Person> loadPersonByName(const std::string & name);

  /// Retrieves a person by name, including their photo if one is
  /// stored. Returns std::nullopt if no person exists with that name.
  std::optional<domain::Person>
  loadPersonByNameWithPhoto(const std::string & name);

  /// Retrieves all persons of the given type (e.g. all guests), with
  /// getPhoto() left std::nullopt on each, in order of first
  /// registration (FIFO). Default retrieval; see loadPersonByName.
  std::vector<domain::Person> loadPersonsByType(domain::PersonType type);

  /// Retrieves all persons of the given type, including each
  /// person's photo if one is stored, in order of first registration
  /// (FIFO).
  std::vector<domain::Person>
  loadPersonsByTypeWithPhoto(domain::PersonType type);

  /// Removes a person (and the description and pose rows they owned)
  /// from the database by name. Shifts the position of persons
  /// registered after them. Any object that person was holding becomes
  /// ObjectOwnership::unknown() (done by the schema's ON DELETE SET
  /// NULL on object_state), so no object keeps referring to a person
  /// that no longer exists.
  void deletePerson(const std::string & name);

  // -- KnownLocation -----------------------------------------------

  /// Inserts or updates a known location in the database, including
  /// its zone.
  void saveKnownLocation(const domain::KnownLocation & location);

  /// Retrieves a known location by name. Returns std::nullopt if none
  /// exists with that name.
  std::optional<domain::KnownLocation>
  loadKnownLocationByName(const std::string & name);

  /// Retrieves every known location currently stored.
  std::vector<domain::KnownLocation> loadAllKnownLocations();

  /// Retrieves every known location belonging to the given zone.
  /// Returns an empty vector if the zone has none.
  std::vector<domain::KnownLocation>
  loadKnownLocationsByZone(domain::LocationZone zone);

  // -- Run-state reset ---------------------------------------------

  /// Resets, in a single transaction, everything that is unknown at
  /// the beginning of a run, and keeps everything that is knowledge:
  ///   - Robot: its current location goes back to its start location
  ///     and both hands are free. If no robot is stored, nothing is
  ///     done.
  ///   - Persons: every person except those of type HOST is deleted
  ///     (with the description and pose rows they owned). Surviving
  ///     hosts keep their name, description and photo; their
  ///     navigable pose is cleared.
  ///   - Objects: none is deleted. Each keeps its name, category and
  ///     usual zone; its ownership becomes unknown and its navigable
  ///     and grasp poses are removed.
  ///   - KnownLocation entries are never touched.
  /// On any failure the transaction is rolled back, leaving the
  /// database exactly as it was, and RepositoryException is thrown.
  /// The order in which things are cleared (ownership before persons)
  /// is an internal detail of this method.
  void resetRuntimeState();

private:
  std::unique_ptr<SQLite::Database> db_;
  Logger & logger_;

  void ensureSchema(const std::string & schemaPath);
};

} // namespace knowledge_manager::persistence
