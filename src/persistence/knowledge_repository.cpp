#include "knowledge_manager/persistence/knowledge_repository.hpp"

#include <cstdint>
#include <fstream>
#include <sstream>

#include <SQLiteCpp/SQLiteCpp.h>

#include "knowledge_manager/domain/location_zone.hpp"
#include "knowledge_manager/domain/object_category.hpp"
#include "knowledge_manager/domain/object_ownership.hpp"
#include "knowledge_manager/domain/robot_hand.hpp"
#include "knowledge_manager/persistence/repository_exception.hpp"

namespace knowledge_manager::persistence
{

using domain::KnownLocation;
using domain::LocationZone;
using domain::ManipulableObject;
using domain::ObjectCategory;
using domain::ObjectOwnership;
using domain::Person;
using domain::PersonType;
using domain::Pose3D;
using domain::Robot;
using domain::RobotHand;

namespace
{

// -- RobotHand <-> TEXT ('left'/'right'), matching the
// held_by_robot_hand CHECK constraint in object_state. Kept private to
// the .cpp: no caller outside this file needs to know the on-disk
// text representation of a hand.
std::string handToText(RobotHand hand)
{
  return hand == RobotHand::LEFT ? "left" : "right";
}

RobotHand textToHand(const std::string & text)
{
  if (text == "left") {
    return RobotHand::LEFT;
  }
  if (text == "right") {
    return RobotHand::RIGHT;
  }
  throw RepositoryException(
      "Unrecognized held_by_robot_hand value in database: '" + text + "'");
}

// -- LocationZone <-> TEXT, matching the CHECK constraints on
// known_location.zone and object.usual_zone.
std::string zoneToText(LocationZone zone)
{
  switch (zone) {
    case LocationZone::KITCHEN:
      return "kitchen";
    case LocationZone::LIVING_ROOM:
      return "living_room";
    case LocationZone::BEDROOM:
      return "bedroom";
    case LocationZone::LAUNDRY:
      return "laundry";
  }
  throw RepositoryException("Unrecognized LocationZone value");
}

LocationZone textToZone(const std::string & text)
{
  if (text == "kitchen") {
    return LocationZone::KITCHEN;
  }
  if (text == "living_room") {
    return LocationZone::LIVING_ROOM;
  }
  if (text == "bedroom") {
    return LocationZone::BEDROOM;
  }
  if (text == "laundry") {
    return LocationZone::LAUNDRY;
  }
  throw RepositoryException("Unrecognized zone name in database: '" + text +
                            "'");
}

// -- PersonType <-> TEXT, matching person_category.name rows.
std::string personTypeToText(PersonType type)
{
  switch (type) {
    case PersonType::GUEST:
      return "guest";
    case PersonType::CUSTOMER:
      return "customer";
    case PersonType::HOST:
      return "host";
    case PersonType::USER:
      return "user";
  }
  throw RepositoryException("Unrecognized PersonType value");
}

PersonType textToPersonType(const std::string & text)
{
  if (text == "guest") {
    return PersonType::GUEST;
  }
  if (text == "customer") {
    return PersonType::CUSTOMER;
  }
  if (text == "host") {
    return PersonType::HOST;
  }
  if (text == "user") {
    return PersonType::USER;
  }
  throw RepositoryException("Unrecognized person category name in database: '" +
                            text + "'");
}

// -- Person.photo <-> BLOB in person.photo.
//
// Binds the given optional photo to a nullable BLOB parameter: NULL
// when absent, the raw bytes (copied by SQLiteCpp, SQLITE_TRANSIENT)
// otherwise. Kept private to this file, mirroring how RobotHand and
// PersonType text encodings are also kept as file-local helpers --
// callers of KnowledgeRepository never need to know the on-disk
// representation.
void bindOptionalPhoto(
  SQLite::Statement & stmt, int index,
  const std::optional<std::vector<std::byte>> & photo)
{
  if (photo.has_value()) {
    stmt.bind(index, photo->data(), static_cast<int>(photo->size()));
  } else {
    stmt.bind(index);
  }
}

// Reads a nullable BLOB column into an optional photo. Returns
// std::nullopt if the column is NULL. Copies the bytes out of the
// Column immediately, since Column::getBlob() returns a pointer only
// valid while the owning Statement is alive.
std::optional<std::vector<std::byte>>
readOptionalPhoto(const SQLite::Column & column)
{
  if (column.isNull()) {
    return std::nullopt;
  }
  const std::byte *data = static_cast<const std::byte *>(column.getBlob());
  return std::vector<std::byte>(data, data + column.getBytes());
}

// -- ObjectCategory <-> TEXT, matching ManipulableObject_category.name rows.
std::string objectCategoryToText(ObjectCategory category)
{
  switch (category) {
    case ObjectCategory::FRUITS:
      return "fruits";
    case ObjectCategory::TRASH:
      return "trash";
    case ObjectCategory::TABLEWARE:
      return "tableware";
    case ObjectCategory::CUTLERY:
      return "cutlery";
    case ObjectCategory::GROCERY:
      return "grocery";
    case ObjectCategory::DRINKS:
      return "drinks";
    case ObjectCategory::SNACKS:
      return "snacks";
    case ObjectCategory::BASKET:
      return "basket";
    case ObjectCategory::FOOD:
      return "food";
    case ObjectCategory::CLEANING_SUPPLY:
      return "cleaning_supply";
    case ObjectCategory::CONTAINERS:
      return "containers";
    case ObjectCategory::CONTAINER_DROP:
      return "container_drop";
    case ObjectCategory::ELECTRODOMESTICS:
      return "electrodomestics";
    case ObjectCategory::TOYS:
      return "toys";
    case ObjectCategory::FABRIC:
      return "fabric";
  }
  throw RepositoryException("Unrecognized ObjectCategory value");
}

ObjectCategory textToObjectCategory(const std::string & category)
{
  if (category == "fruits") {
    return ObjectCategory::FRUITS;
  }
  if (category == "trash") {
    return ObjectCategory::TRASH;
  }
  if (category == "tableware") {
    return ObjectCategory::TABLEWARE;
  }
  if (category == "cutlery") {
    return ObjectCategory::CUTLERY;
  }
  if (category == "grocery") {
    return ObjectCategory::GROCERY;
  }
  if (category == "drinks") {
    return ObjectCategory::DRINKS;
  }
  if (category == "snacks") {
    return ObjectCategory::SNACKS;
  }
  if (category == "basket") {
    return ObjectCategory::BASKET;
  }
  if (category == "food") {
    return ObjectCategory::FOOD;
  }
  if (category == "cleaning_supply") {
    return ObjectCategory::CLEANING_SUPPLY;
  }
  if (category == "containers") {
    return ObjectCategory::CONTAINERS;
  }
  if (category == "container_drop") {
    return ObjectCategory::CONTAINER_DROP;
  }
  if (category == "electrodomestics") {
    return ObjectCategory::ELECTRODOMESTICS;
  }
  if (category == "toys") {
    return ObjectCategory::TOYS;
  }
  if (category == "fabric") {
    return ObjectCategory::FABRIC;
  }
  throw RepositoryException("Unrecognized object category name in database: '" +
                            category + "'");
}

} // namespace

// ---------------------------------------------------------------------
// Construction / schema setup
// ---------------------------------------------------------------------

KnowledgeRepository::KnowledgeRepository(
  const std::string & dbPath,
  const std::string & schemaPath,
  Logger & logger)
: db_(), logger_(logger)
{
  try {
    db_ = std::make_unique<SQLite::Database>(dbPath, SQLite::OPEN_READWRITE |
                                                         SQLite::OPEN_CREATE);
    db_->exec("PRAGMA foreign_keys = ON;");
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("Failed to open database at '") + dbPath +
                  "': " + e.what());
    throw RepositoryException(std::string("Failed to open database: ") +
                              e.what());
  }

  ensureSchema(schemaPath);
}

KnowledgeRepository::~KnowledgeRepository() = default;

void KnowledgeRepository::ensureSchema(const std::string & schemaPath)
{
  std::ifstream file(schemaPath);
  if (!file.is_open()) {
    logger_.error("Could not open schema file at '" + schemaPath + "'");
    throw RepositoryException("Could not open schema file at '" + schemaPath +
                              "'");
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  try {
    // schema.sql uses "CREATE TABLE IF NOT EXISTS" / "CREATE INDEX IF
    // NOT EXISTS" throughout, so running it again on an
    // already-initialized database (e.g. after a process restart) is
    // safe and a no-op. It does NOT migrate a database created with
    // an older schema version: such a file must be deleted once.
    db_->exec(buffer.str());
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("Failed to apply schema from '") + schemaPath +
                  "': " + e.what());
    throw RepositoryException(std::string("Failed to apply schema: ") +
                              e.what());
  }
}

// ---------------------------------------------------------------------
// Pose3D <-> pose_3d row helpers
//
// Pose3D is a value object with no identity of its own in the domain,
// so KnowledgeRepository owns the decision of how it maps to a row:
// the owning row (known_location, object or person) remembers which
// pose_3d row it uses, and a save updates that row in place
// (upsertPose) instead of inserting a new one every time. Each
// pose_3d row has exactly one owner, so updating in place never
// affects another entity, and no orphan rows accumulate. The domain
// still has no getId() on Pose3D: the id never leaves this file.
// ---------------------------------------------------------------------

namespace
{

std::int64_t insertPose(SQLite::Database & db, const Pose3D & pose)
{
  SQLite::Statement stmt(db, "INSERT INTO pose_3d (x, y, z, qx, qy, qz, qw) "
    "VALUES (?, ?, ?, ?, ?, ?, ?)");
  stmt.bind(1, pose.getX());
  stmt.bind(2, pose.getY());
  stmt.bind(3, pose.getZ());
  stmt.bind(4, pose.getQx());
  stmt.bind(5, pose.getQy());
  stmt.bind(6, pose.getQz());
  stmt.bind(7, pose.getQw());
  stmt.exec();
  return db.getLastInsertRowid();
}

// Returns a default-constructed Pose3D (0,0,0 / identity quaternion)
// if poseId is absent, per the agreed fallback for an orphaned
// pose_id left NULL by an ON DELETE SET NULL -- KnownLocation::getPose()
// is not optional in the domain, so this repository must always
// produce some Pose3D.
Pose3D loadPoseOrDefault(
  SQLite::Database & db,
  std::optional<std::int64_t> poseId)
{
  if (!poseId.has_value()) {
    return Pose3D();
  }

  SQLite::Statement stmt(
    db, "SELECT x, y, z, qx, qy, qz, qw FROM pose_3d WHERE id = ?");
  stmt.bind(1, *poseId);
  if (!stmt.executeStep()) {
    // Referenced id does not exist (should not normally happen given
    // FK constraints, but defensively treated the same as "no pose").
    return Pose3D();
  }

  Pose3D pose;
  pose.setPosition(static_cast<float>(stmt.getColumn(0).getDouble()),
                   static_cast<float>(stmt.getColumn(1).getDouble()),
                   static_cast<float>(stmt.getColumn(2).getDouble()));
  pose.setOrientation(static_cast<float>(stmt.getColumn(3).getDouble()),
                      static_cast<float>(stmt.getColumn(4).getDouble()),
                      static_cast<float>(stmt.getColumn(5).getDouble()),
                      static_cast<float>(stmt.getColumn(6).getDouble()));
  return pose;
}

std::optional<std::int64_t> optionalInt64(const SQLite::Column & column)
{
  if (column.isNull()) {
    return std::nullopt;
  }
  return column.getInt64();
}

// Returns true if a row with poseId existed and was updated.
bool updatePose(SQLite::Database & db, std::int64_t poseId, const Pose3D & pose)
{
  SQLite::Statement stmt(db, "UPDATE pose_3d SET x = ?, y = ?, z = ?, "
    "qx = ?, qy = ?, qz = ?, qw = ? WHERE id = ?");
  stmt.bind(1, pose.getX());
  stmt.bind(2, pose.getY());
  stmt.bind(3, pose.getZ());
  stmt.bind(4, pose.getQx());
  stmt.bind(5, pose.getQy());
  stmt.bind(6, pose.getQz());
  stmt.bind(7, pose.getQw());
  stmt.bind(8, poseId);
  return stmt.exec() > 0;
}

// Returns the id of the pose_3d row now holding `pose`: reuses
// `existingId` (updating that row in place) when the owner already has
// one, and inserts a new row otherwise. This is what keeps repeated
// saves of the same entity from leaving orphan pose_3d rows behind.
std::int64_t upsertPose(
  SQLite::Database & db,
  std::optional<std::int64_t> existingId,
  const Pose3D & pose)
{
  if (existingId.has_value() && updatePose(db, *existingId, pose)) {
    return *existingId;
  }
  return insertPose(db, pose);
}

// Deletes a pose_3d row that its owner no longer references. Must be
// called AFTER the owning row is deleted or repointed (the owner holds
// a FK to pose_3d).
void deletePoseIfPresent(
  SQLite::Database & db,
  std::optional<std::int64_t> poseId)
{
  if (!poseId.has_value()) {
    return;
  }
  SQLite::Statement stmt(db, "DELETE FROM pose_3d WHERE id = ?");
  stmt.bind(1, *poseId);
  stmt.exec();
}

// Builds a KnownLocation (name, pose, zone) from its known_location
// row, or std::nullopt if no location has that name. Shared by every
// place that has to turn a location name back into a KnownLocation
// (loadKnownLocationByName, the Person and Robot loaders), so the
// row-to-domain translation, now including the zone, lives in one
// place.
std::optional<KnownLocation> findKnownLocation(
  SQLite::Database & db,
  const std::string & name)
{
  SQLite::Statement stmt(
    db, "SELECT pose_id, zone FROM known_location WHERE name = ?");
  stmt.bind(1, name);
  if (!stmt.executeStep()) {
    return std::nullopt;
  }
  return KnownLocation(name,
                       loadPoseOrDefault(db, optionalInt64(stmt.getColumn(0))),
                       textToZone(stmt.getColumn(1).getString()));
}

// Drains a statement whose columns are (name, pose_id, zone) into a
// list of KnownLocation. Shared by loadAllKnownLocations and
// loadKnownLocationsByZone.
std::vector<KnownLocation> collectKnownLocations(
  SQLite::Database & db,
  SQLite::Statement & stmt)
{
  std::vector<KnownLocation> result;
  while (stmt.executeStep()) {
    result.emplace_back(stmt.getColumn(0).getString(),
                        loadPoseOrDefault(db, optionalInt64(stmt.getColumn(1))),
                        textToZone(stmt.getColumn(2).getString()));
  }
  return result;
}

} // namespace

// ---------------------------------------------------------------------
// KnownLocation
// ---------------------------------------------------------------------

void KnowledgeRepository::saveKnownLocation(const KnownLocation & location)
{
  try {
    std::optional<std::int64_t> currentPoseId;
    {
      SQLite::Statement current(
        *db_, "SELECT pose_id FROM known_location WHERE name = ?");
      current.bind(1, location.getName());
      if (current.executeStep()) {
        currentPoseId = optionalInt64(current.getColumn(0));
      }
    }
    std::int64_t poseId = upsertPose(*db_, currentPoseId, location.getPose());

    SQLite::Statement stmt(*db_,
      "INSERT INTO known_location (name, pose_id, zone) "
      "VALUES (?, ?, ?) "
      "ON CONFLICT(name) DO UPDATE SET "
      "pose_id = excluded.pose_id, zone = excluded.zone");
    stmt.bind(1, location.getName());
    stmt.bind(2, poseId);
    stmt.bind(3, zoneToText(location.getZone()));
    stmt.exec();
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("saveKnownLocation failed: ") + e.what());
    throw RepositoryException(std::string("saveKnownLocation failed: ") +
                              e.what());
  }
}

std::optional<KnownLocation>
KnowledgeRepository::loadKnownLocationByName(const std::string & name)
{
  try {
    return findKnownLocation(*db_, name);
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("loadKnownLocationByName failed: ") + e.what());
    throw RepositoryException(std::string("loadKnownLocationByName failed: ") +
                              e.what());
  }
}

std::vector<KnownLocation> KnowledgeRepository::loadAllKnownLocations()
{
  try {
    SQLite::Statement stmt(*db_,
      "SELECT name, pose_id, zone FROM known_location");
    return collectKnownLocations(*db_, stmt);
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("loadAllKnownLocations failed: ") + e.what());
    throw RepositoryException(std::string("loadAllKnownLocations failed: ") +
                              e.what());
  }
}

std::vector<KnownLocation>
KnowledgeRepository::loadKnownLocationsByZone(LocationZone zone)
{
  try {
    SQLite::Statement stmt(
      *db_, "SELECT name, pose_id, zone FROM known_location WHERE zone = ?");
    stmt.bind(1, zoneToText(zone));
    return collectKnownLocations(*db_, stmt);
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("loadKnownLocationsByZone failed: ") + e.what());
    throw RepositoryException(std::string("loadKnownLocationsByZone failed: ") +
                              e.what());
  }
}

// ---------------------------------------------------------------------
// ManipulableObject / ObjectOwnership (exclusive-arc)
// ---------------------------------------------------------------------

namespace
{

// Resolves the object_category.id for a given category name,
// inserting the category row if it is not yet there. ObjectCategory
// is a fixed enumeration, so every name reaching this function is
// already valid by construction; this only needs an id to store the
// category against.
std::int64_t resolveObjectCategoryId(
  SQLite::Database & db,
  const std::string & categoryName)
{
  {
    SQLite::Statement select(db,
      "SELECT id FROM object_category WHERE name = ?");
    select.bind(1, categoryName);
    if (select.executeStep()) {
      return select.getColumn(0).getInt64();
    }
  }
  SQLite::Statement insert(db, "INSERT INTO object_category (name) VALUES (?)");
  insert.bind(1, categoryName);
  insert.exec();
  return db.getLastInsertRowid();
}

std::string objectCategoryNameById(
  SQLite::Database & db,
  std::int64_t categoryId)
{
  SQLite::Statement stmt(db, "SELECT name FROM object_category WHERE id = ?");
  stmt.bind(1, categoryId);
  if (!stmt.executeStep()) {
    throw RepositoryException(
        "object_category row referenced by object not found (id=" +
        std::to_string(categoryId) + ")");
  }
  return stmt.getColumn(0).getString();
}

// Writes the exclusive-arc row in object_state for the given
// (already-inserted) object id, based on the in-memory
// ObjectOwnership. At most one of the three columns is set, mirroring
// the CHECK constraint in schema.sql: an unknown ownership leaves all
// three NULL.
void writeObjectState(
  SQLite::Database & db, std::int64_t objectId,
  const ObjectOwnership & ownership)
{
  SQLite::Statement stmt(
    db,
    "INSERT INTO object_state (object_id, location_name, held_by_person_id, "
    "held_by_robot_hand) VALUES (?, ?, ?, ?) "
    "ON CONFLICT(object_id) DO UPDATE SET "
    "location_name = excluded.location_name, "
    "held_by_person_id = excluded.held_by_person_id, "
    "held_by_robot_hand = excluded.held_by_robot_hand");
  stmt.bind(1, objectId);

  if (ownership.isUnknown()) {
    stmt.bind(2); // NULL
    stmt.bind(3); // NULL
    stmt.bind(4); // NULL
  } else if (ownership.isAtLocation()) {
    stmt.bind(2, ownership.getLocationName());
    stmt.bind(3); // NULL
    stmt.bind(4); // NULL
  } else if (ownership.isHeldByPerson()) {
    // held_by_person_id is a FK to person.id, but ObjectOwnership
    // only carries a person *name* -- resolve it here.
    SQLite::Statement personLookup(db, "SELECT id FROM person WHERE name = ?");
    personLookup.bind(1, ownership.getPersonName());
    if (!personLookup.executeStep()) {
      throw RepositoryException("ObjectOwnership references unknown person '" +
                                ownership.getPersonName() +
                                "'; save that Person first");
    }
    stmt.bind(2); // NULL
    stmt.bind(3, personLookup.getColumn(0).getInt64());
    stmt.bind(4); // NULL
  } else {
    stmt.bind(2); // NULL
    stmt.bind(3); // NULL
    stmt.bind(4, handToText(ownership.getRobotHand()));
  }

  stmt.exec();
}

// Reconstructs an ObjectOwnership from an object_state row already
// positioned on the current row of stmt (columns: location_name,
// held_by_person_id, held_by_robot_hand, in that order), resolving
// held_by_person_id back to a person name. All three columns NULL
// means the ownership is unknown.
ObjectOwnership readObjectOwnership(
  SQLite::Database & db,
  SQLite::Statement & stmt, int locationCol,
  int personIdCol, int handCol)
{
  if (!stmt.getColumn(locationCol).isNull()) {
    return ObjectOwnership::atLocation(stmt.getColumn(locationCol).getString());
  }
  if (!stmt.getColumn(personIdCol).isNull()) {
    std::int64_t personId = stmt.getColumn(personIdCol).getInt64();
    SQLite::Statement nameLookup(db, "SELECT name FROM person WHERE id = ?");
    nameLookup.bind(1, personId);
    if (!nameLookup.executeStep()) {
      throw RepositoryException("object_state references missing person id=" +
                                std::to_string(personId));
    }
    return ObjectOwnership::heldByPerson(nameLookup.getColumn(0).getString());
  }
  if (!stmt.getColumn(handCol).isNull()) {
    return ObjectOwnership::heldByRobot(
        textToHand(stmt.getColumn(handCol).getString()));
  }
  return ObjectOwnership::unknown();
}

// Builds a full ManipulableObject from the already-read columns of an
// `object` row, joined with its object_state row. Shared by
// loadObjectByName, loadObjectsByCategory and loadPersonOrder to avoid
// duplicating the same assembly logic (Clean Code: don't repeat
// yourself across near-identical query paths).
ManipulableObject buildObjectFromRow(
  SQLite::Database & db,
  std::int64_t objectId,
  const std::string & name,
  const std::string & categoryName,
  const std::string & usualZoneName,
  std::optional<std::int64_t> navPoseId,
  std::optional<std::int64_t> graspPoseId)
{
  SQLite::Statement stateStmt(
    db, "SELECT location_name, held_by_person_id, held_by_robot_hand FROM "
    "object_state WHERE object_id = ?");
  stateStmt.bind(1, objectId);
  if (!stateStmt.executeStep()) {
    throw RepositoryException("object id=" + std::to_string(objectId) +
                              " has no matching object_state row");
  }
  ObjectOwnership ownership = readObjectOwnership(db, stateStmt, 0, 1, 2);

  // categoryName and usualZoneName come from TEXT columns, so they
  // must be translated back into the fixed ObjectCategory and
  // LocationZone enums before reaching the domain.
  ManipulableObject object(name, textToObjectCategory(categoryName),
    textToZone(usualZoneName), ownership);
  if (navPoseId.has_value()) {
    object.setNavigablePose(loadPoseOrDefault(db, navPoseId));
  }
  if (graspPoseId.has_value()) {
    object.setGraspPose(loadPoseOrDefault(db, graspPoseId));
  }
  return object;
}

} // namespace

namespace
{

// Core logic of saving a ManipulableObject, without opening its own
// transaction. Factored out so that saveRobot -- which needs to save
// the objects held in each hand as part of its own single transaction
// -- can call this directly instead of nesting a second
// SQLite::Transaction inside the first (SQLite/SQLiteCpp does not
// support nested transactions). KnowledgeRepository::saveObject wraps
// this in its own transaction for callers that save a single object
// on its own.
void saveObjectImpl(SQLite::Database & db, const ManipulableObject & object)
{
  std::int64_t categoryId =
    resolveObjectCategoryId(db, objectCategoryToText(object.getCategory()));

  // Reuse the pose rows this object already points to (if any), so
  // that re-saving an object (e.g. vision refining its pose) updates
  // them in place instead of leaving the previous rows orphaned.
  std::optional<std::int64_t> currentNavPoseId;
  std::optional<std::int64_t> currentGraspPoseId;
  {
    SQLite::Statement current(
      db, "SELECT nav_pose_id, manip_pose_id FROM object WHERE name = ?");
    current.bind(1, object.getName());
    if (current.executeStep()) {
      currentNavPoseId = optionalInt64(current.getColumn(0));
      currentGraspPoseId = optionalInt64(current.getColumn(1));
    }
  }

  std::optional<std::int64_t> navPoseId;
  if (object.getNavigablePose().has_value()) {
    navPoseId = upsertPose(db, currentNavPoseId, *object.getNavigablePose());
  }
  std::optional<std::int64_t> graspPoseId;
  if (object.getGraspPose().has_value()) {
    graspPoseId = upsertPose(db, currentGraspPoseId, *object.getGraspPose());
  }

  SQLite::Statement stmt(
    db,
    "INSERT INTO object (name, category_id, usual_zone, nav_pose_id, "
    "manip_pose_id) VALUES (?, ?, ?, ?, ?) "
    "ON CONFLICT(name) DO UPDATE SET "
    "category_id = excluded.category_id, "
    "usual_zone = excluded.usual_zone, "
    "nav_pose_id = COALESCE(excluded.nav_pose_id, object.nav_pose_id), "
    "manip_pose_id = COALESCE(excluded.manip_pose_id, object.manip_pose_id)");
  stmt.bind(1, object.getName());
  stmt.bind(2, categoryId);
  stmt.bind(3, zoneToText(object.getUsualZone()));
  if (navPoseId.has_value()) {
    stmt.bind(4, *navPoseId);
  } else {
    stmt.bind(4);
  }
  if (graspPoseId.has_value()) {
    stmt.bind(5, *graspPoseId);
  } else {
    stmt.bind(5);
  }
  stmt.exec();

  SQLite::Statement idLookup(db, "SELECT id FROM object WHERE name = ?");
  idLookup.bind(1, object.getName());
  idLookup.executeStep();
  std::int64_t objectId = idLookup.getColumn(0).getInt64();

  writeObjectState(db, objectId, object.getOwnership());
}

} // namespace

void KnowledgeRepository::saveObject(const ManipulableObject & object)
{
  try {
    SQLite::Transaction transaction(*db_);
    saveObjectImpl(*db_, object);
    transaction.commit();
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("saveObject failed for '") + object.getName() +
                  "': " + e.what());
    throw RepositoryException(std::string("saveObject failed: ") + e.what());
  }
}

std::optional<ManipulableObject>
KnowledgeRepository::loadObjectByName(const std::string & name)
{
  try {
    SQLite::Statement stmt(
      *db_, "SELECT id, category_id, usual_zone, "
      "nav_pose_id, manip_pose_id FROM object WHERE name = ?");
    stmt.bind(1, name);
    if (!stmt.executeStep()) {
      return std::nullopt;
    }

    std::int64_t objectId = stmt.getColumn(0).getInt64();
    std::string categoryName =
      objectCategoryNameById(*db_, stmt.getColumn(1).getInt64());
    std::string usualZoneName = stmt.getColumn(2).getString();
    std::optional<std::int64_t> navPoseId = optionalInt64(stmt.getColumn(3));
    std::optional<std::int64_t> graspPoseId = optionalInt64(stmt.getColumn(4));

    return buildObjectFromRow(*db_, objectId, name, categoryName, usualZoneName,
                              navPoseId, graspPoseId);
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("loadObjectByName failed: ") + e.what());
    throw RepositoryException(std::string("loadObjectByName failed: ") +
                              e.what());
  }
}

std::vector<ManipulableObject>
KnowledgeRepository::loadObjectsByCategory(ObjectCategory category)
{
  std::vector<ManipulableObject> result;
  try {
    std::string categoryName = objectCategoryToText(category);

    // ORDER BY o.id is part of this method's contract: objects are
    // returned in order of first registration (FIFO), which
    // WorldStateService::getObjectByCategoryAndPosition relies on
    // (plans refer to objects as fruits_1, fruits_2, ...).
    // saveObjectImpl preserves the id on update (upsert), and the
    // startup reset never deletes objects, so this order is stable
    // across re-saves and restarts. Do not remove it.
    SQLite::Statement stmt(
      *db_, "SELECT o.id, o.name, o.usual_zone, o.nav_pose_id, "
      "o.manip_pose_id "
      "FROM object o JOIN object_category c ON o.category_id = c.id "
      "WHERE c.name = ? "
      "ORDER BY o.id ASC");
    stmt.bind(1, categoryName);
    while (stmt.executeStep()) {
      std::int64_t objectId = stmt.getColumn(0).getInt64();
      std::string name = stmt.getColumn(1).getString();
      std::string usualZoneName = stmt.getColumn(2).getString();
      std::optional<std::int64_t> navPoseId = optionalInt64(stmt.getColumn(3));
      std::optional<std::int64_t> graspPoseId =
        optionalInt64(stmt.getColumn(4));

      result.push_back(buildObjectFromRow(*db_, objectId, name, categoryName,
                                          usualZoneName, navPoseId,
                                          graspPoseId));
    }
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("loadObjectsByCategory failed: ") + e.what());
    throw RepositoryException(std::string("loadObjectsByCategory failed: ") +
                              e.what());
  }
  return result;
}

void KnowledgeRepository::deleteObject(const std::string & name)
{
  try {
    SQLite::Transaction transaction(*db_);

    std::optional<std::int64_t> navPoseId;
    std::optional<std::int64_t> graspPoseId;
    {
      SQLite::Statement poses(
        *db_, "SELECT nav_pose_id, manip_pose_id FROM object WHERE name = ?");
      poses.bind(1, name);
      if (poses.executeStep()) {
        navPoseId = optionalInt64(poses.getColumn(0));
        graspPoseId = optionalInt64(poses.getColumn(1));
      }
    }

    SQLite::Statement stmt(*db_, "DELETE FROM object WHERE name = ?");
    stmt.bind(1, name);
    stmt.exec();

    // The object owned these pose rows exclusively; remove them with it.
    deletePoseIfPresent(*db_, navPoseId);
    deletePoseIfPresent(*db_, graspPoseId);

    transaction.commit();
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("deleteObject failed: ") + e.what());
    throw RepositoryException(std::string("deleteObject failed: ") + e.what());
  }
}

// ---------------------------------------------------------------------
// Person
// ---------------------------------------------------------------------

namespace
{

std::int64_t resolvePersonCategoryId(SQLite::Database & db, PersonType type)
{
  std::string typeName = personTypeToText(type);
  {
    SQLite::Statement select(db,
      "SELECT id FROM person_category WHERE name = ?");
    select.bind(1, typeName);
    if (select.executeStep()) {
      return select.getColumn(0).getInt64();
    }
  }
  SQLite::Statement insert(db, "INSERT INTO person_category (name) VALUES (?)");
  insert.bind(1, typeName);
  insert.exec();
  return db.getLastInsertRowid();
}

std::int64_t
insertPersonDescription(
  SQLite::Database & db,
  const domain::PhysicalDescription & description)
{
  SQLite::Statement stmt(
    db, "INSERT INTO person_description (gender, clothes, age, hair, "
    "facial_expression, accessories_phrase) VALUES (?, ?, ?, ?, ?, ?)");
  stmt.bind(1, description.getGender());
  stmt.bind(2, description.getClothes());
  stmt.bind(3, description.getAge());
  stmt.bind(4, description.getHairType());
  stmt.bind(5, description.getFacialExpression());
  stmt.bind(6, description.getAccessories());
  stmt.exec();
  return db.getLastInsertRowid();
}

bool updatePersonDescription(
  SQLite::Database & db, std::int64_t descriptionId,
  const domain::PhysicalDescription & description)
{
  SQLite::Statement stmt(
    db,
    "UPDATE person_description SET gender = ?, clothes = ?, age = ?, "
    "hair = ?, facial_expression = ?, accessories_phrase = ? WHERE id = ?");
  stmt.bind(1, description.getGender());
  stmt.bind(2, description.getClothes());
  stmt.bind(3, description.getAge());
  stmt.bind(4, description.getHairType());
  stmt.bind(5, description.getFacialExpression());
  stmt.bind(6, description.getAccessories());
  stmt.bind(7, descriptionId);
  return stmt.exec() > 0;
}

// Same idea as upsertPose: reuse the person's existing description
// row when there is one, instead of inserting a new row on every save.
std::int64_t
upsertPersonDescription(
  SQLite::Database & db,
  std::optional<std::int64_t> existingId,
  const domain::PhysicalDescription & description)
{
  if (existingId.has_value() &&
    updatePersonDescription(db, *existingId, description))
  {
    return *existingId;
  }
  return insertPersonDescription(db, description);
}

domain::PhysicalDescription
loadPersonDescriptionOrDefault(
  SQLite::Database & db,
  std::optional<std::int64_t> descriptionId)
{
  domain::PhysicalDescription description;
  if (!descriptionId.has_value()) {
    return description;
  }

  SQLite::Statement stmt(
    db, "SELECT gender, clothes, age, hair, facial_expression, "
    "accessories_phrase FROM person_description WHERE id = ?");
  stmt.bind(1, *descriptionId);
  if (!stmt.executeStep()) {
    return description;
  }

  description.setGender(stmt.getColumn(0).getString());
  description.setClothes(stmt.getColumn(1).getString());
  description.setAge(stmt.getColumn(2).getString());
  description.setHairType(stmt.getColumn(3).getString());
  description.setFacialExpression(stmt.getColumn(4).getString());
  description.setAccessories(stmt.getColumn(5).getString());
  return description;
}

// Loads person_order rows for personId into a list of
// ManipulableObject, resolving each object by its FK to the object
// table (each referenced object is expected to already exist, per the
// FK constraint in schema.sql).
std::vector<ManipulableObject> loadPersonOrder(
  SQLite::Database & db,
  std::int64_t personId)
{
  std::vector<ManipulableObject> order;
  SQLite::Statement stmt(
    db, "SELECT o.id, o.name, o.category_id, o.usual_zone, o.nav_pose_id, "
    "o.manip_pose_id "
    "FROM person_order po JOIN object o ON po.object_id = o.id "
    "WHERE po.person_id = ? ORDER BY po.seq");
  stmt.bind(1, personId);
  while (stmt.executeStep()) {
    std::int64_t objectId = stmt.getColumn(0).getInt64();
    std::string name = stmt.getColumn(1).getString();
    std::string categoryName =
      objectCategoryNameById(db, stmt.getColumn(2).getInt64());
    std::string usualZoneName = stmt.getColumn(3).getString();
    std::optional<std::int64_t> navPoseId = optionalInt64(stmt.getColumn(4));
    std::optional<std::int64_t> graspPoseId = optionalInt64(stmt.getColumn(5));

    order.push_back(buildObjectFromRow(db, objectId, name, categoryName,
                                       usualZoneName, navPoseId, graspPoseId));
  }
  return order;
}

void writePersonOrder(
  SQLite::Database & db, std::int64_t personId,
  const std::vector<ManipulableObject> & order)
{
  SQLite::Statement clear(db, "DELETE FROM person_order WHERE person_id = ?");
  clear.bind(1, personId);
  clear.exec();

  int seq = 0;
  for (const auto & item : order) {
    SQLite::Statement idLookup(db, "SELECT id FROM object WHERE name = ?");
    idLookup.bind(1, item.getName());
    if (!idLookup.executeStep()) {
      throw RepositoryException("Person order references unknown object '" +
                                item.getName() +
                                "'; save that ManipulableObject first");
    }
    std::int64_t objectId = idLookup.getColumn(0).getInt64();

    SQLite::Statement insert(db, "INSERT INTO person_order (person_id, "
      "object_id, seq) VALUES (?, ?, ?)");
    insert.bind(1, personId);
    insert.bind(2, objectId);
    insert.bind(3, seq++);
    insert.exec();
  }
}

// includePhoto is a file-private implementation detail, not part of
// KnowledgeRepository's public interface: the public API expresses
// the with/without-photo distinction through two named methods
// (loadPersonByName / loadPersonByNameWithPhoto), not through a
// boolean argument callers would have to interpret at the call site.
Person buildPersonFromRow(
  SQLite::Database & db, std::int64_t personId,
  const std::string & name, PersonType type,
  const std::string & favoriteDrink,
  std::optional<std::int64_t> descriptionId,
  const std::optional<std::string> & locationName,
  std::optional<std::int64_t> navPoseId,
  bool includePhoto)
{
  KnownLocation location;
  if (locationName.has_value()) {
    std::optional<KnownLocation> found = findKnownLocation(db, *locationName);
    if (found.has_value()) {
      location = *found;
    }
  }

  Person person(name, type, location);
  person.setPhysicalDescription(
      loadPersonDescriptionOrDefault(db, descriptionId));
  person.setFavoriteDrink(favoriteDrink);
  if (navPoseId.has_value()) {
    person.setNavigablePose(loadPoseOrDefault(db, navPoseId));
  }
  person.setOrder(loadPersonOrder(db, personId));

  if (includePhoto) {
    SQLite::Statement photoStmt(db, "SELECT photo FROM person WHERE id = ?");
    photoStmt.bind(1, personId);
    if (photoStmt.executeStep()) {
      auto photo = readOptionalPhoto(photoStmt.getColumn(0));
      if (photo.has_value()) {
        person.setPhoto(*photo);
      }
    }
  }

  return person;
}

} // namespace

void KnowledgeRepository::savePerson(const Person & person)
{
  try {
    SQLite::Transaction transaction(*db_);

    // Enforce "only one Person may hold type USER at a time" at the
    // point of writing a USER: any previously-assigned USER is
    // demoted back to GUEST first. Uniqueness itself is an
    // application-level rule owned by WorldStateService per the
    // design document; this repository only needs to make the write
    // consistent with whatever WorldStateService has already decided.
    if (person.getPersonType() == PersonType::USER) {
      std::int64_t guestCategoryId =
        resolvePersonCategoryId(*db_, PersonType::GUEST);
      std::int64_t userCategoryId =
        resolvePersonCategoryId(*db_, PersonType::USER);
      SQLite::Statement demote(*db_, "UPDATE person SET category_id = ? "
        "WHERE category_id = ? AND name != ?");
      demote.bind(1, guestCategoryId);
      demote.bind(2, userCategoryId);
      demote.bind(3, person.getName());
      demote.exec();
    }

    std::int64_t categoryId =
      resolvePersonCategoryId(*db_, person.getPersonType());
    // Reuse the description and nav-pose rows this person already
    // owns (if any) instead of inserting new ones on every save.
    std::optional<std::int64_t> currentDescriptionId;
    std::optional<std::int64_t> currentNavPoseId;
    {
      SQLite::Statement current(
        *db_,
        "SELECT description_id, nav_pose_id FROM person WHERE name = ?");
      current.bind(1, person.getName());
      if (current.executeStep()) {
        currentDescriptionId = optionalInt64(current.getColumn(0));
        currentNavPoseId = optionalInt64(current.getColumn(1));
      }
    }

    std::int64_t descriptionId = upsertPersonDescription(
        *db_, currentDescriptionId, person.getPhysicalDescription());

    std::optional<std::int64_t> navPoseId;
    if (person.getNavigablePose().has_value()) {
      navPoseId =
        upsertPose(*db_, currentNavPoseId, *person.getNavigablePose());
    }

    std::string locationName = person.getKnownLocation().getName();
    bool hasLocation = !locationName.empty();
    if (hasLocation) {
      saveKnownLocation(person.getKnownLocation());
    }

    // photo uses the same COALESCE-on-conflict strategy as nav_pose_id:
    // a Person loaded via the default (no-photo) retrieval and saved
    // back after an unrelated change (e.g. a new location) carries
    // getPhoto() == std::nullopt, and must NOT wipe out a
    // previously-stored photo it simply never loaded. Only a Person
    // that explicitly carries a photo (via setPhoto, or loaded via
    // the WithPhoto retrieval) overwrites the stored value.
    SQLite::Statement stmt(
      *db_,
      "INSERT INTO person (name, category_id, description_id, "
      "favorite_drink, location_name, nav_pose_id, photo) "
      "VALUES (?, ?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(name) DO UPDATE SET "
      "category_id = excluded.category_id, "
      "description_id = excluded.description_id, "
      "favorite_drink = excluded.favorite_drink, "
      "location_name = excluded.location_name, "
      "nav_pose_id = COALESCE(excluded.nav_pose_id, person.nav_pose_id), "
      "photo = COALESCE(excluded.photo, person.photo)");
    stmt.bind(1, person.getName());
    stmt.bind(2, categoryId);
    stmt.bind(3, descriptionId);
    stmt.bind(4, person.getFavoriteDrink());
    if (hasLocation) {
      stmt.bind(5, locationName);
    } else {
      stmt.bind(5);
    }
    if (navPoseId.has_value()) {
      stmt.bind(6, *navPoseId);
    } else {
      stmt.bind(6);
    }
    bindOptionalPhoto(stmt, 7, person.getPhoto());
    stmt.exec();

    SQLite::Statement idLookup(*db_, "SELECT id FROM person WHERE name = ?");
    idLookup.bind(1, person.getName());
    idLookup.executeStep();
    std::int64_t personId = idLookup.getColumn(0).getInt64();

    writePersonOrder(*db_, personId, person.getOrder());

    transaction.commit();
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("savePerson failed for '") + person.getName() +
                  "': " + e.what());
    throw RepositoryException(std::string("savePerson failed: ") + e.what());
  }
}

namespace
{

// Shared implementation for loadPersonByName / loadPersonByNameWithPhoto.
// The two differ only in includePhoto, forwarded to buildPersonFromRow;
// the query itself never selects the photo column, since photo bytes
// are fetched separately (see buildPersonFromRow) only when actually
// requested.
std::optional<Person> loadPersonByNameImpl(
  SQLite::Database & db,
  const std::string & name,
  bool includePhoto)
{
  SQLite::Statement stmt(
    db, "SELECT id, category_id, favorite_drink, description_id, "
    "location_name, nav_pose_id FROM person WHERE name = ?");
  stmt.bind(1, name);
  if (!stmt.executeStep()) {
    return std::nullopt;
  }

  std::int64_t personId = stmt.getColumn(0).getInt64();
  std::int64_t categoryId = stmt.getColumn(1).getInt64();
  std::string favoriteDrink = stmt.getColumn(2).getString();
  std::optional<std::int64_t> descriptionId;
  if (!stmt.getColumn(3).isNull()) {
    descriptionId = stmt.getColumn(3).getInt64();
  }
  std::optional<std::string> locationName;
  if (!stmt.getColumn(4).isNull()) {
    locationName = stmt.getColumn(4).getString();
  }
  std::optional<std::int64_t> navPoseId;
  if (!stmt.getColumn(5).isNull()) {
    navPoseId = stmt.getColumn(5).getInt64();
  }

  SQLite::Statement categoryLookup(
    db, "SELECT name FROM person_category WHERE id = ?");
  categoryLookup.bind(1, categoryId);
  categoryLookup.executeStep();
  PersonType type = textToPersonType(categoryLookup.getColumn(0).getString());

  return buildPersonFromRow(db, personId, name, type, favoriteDrink,
                            descriptionId, locationName, navPoseId,
                            includePhoto);
}

} // namespace

std::optional<Person>
KnowledgeRepository::loadPersonByName(const std::string & name)
{
  try {
    return loadPersonByNameImpl(*db_, name, /*includePhoto=*/false);
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("loadPersonByName failed: ") + e.what());
    throw RepositoryException(std::string("loadPersonByName failed: ") +
                              e.what());
  }
}

std::optional<Person>
KnowledgeRepository::loadPersonByNameWithPhoto(const std::string & name)
{
  try {
    return loadPersonByNameImpl(*db_, name, /*includePhoto=*/true);
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("loadPersonByNameWithPhoto failed: ") + e.what());
    throw RepositoryException(
        std::string("loadPersonByNameWithPhoto failed: ") + e.what());
  }
}

namespace
{

std::vector<Person> loadPersonsByTypeImpl(
  SQLite::Database & db, PersonType type,
  bool includePhoto)
{
  std::vector<Person> result;
  // ORDER BY p.id is part of the contract: persons are returned in
  // order of first registration (FIFO). savePerson preserves the id on
  // update (upsert), so the order is stable across re-saves. Note the
  // position is by first registration, not by when the person acquired
  // the current type (e.g. a USER demoted to GUEST keeps their place).
  SQLite::Statement stmt(
    db, "SELECT p.id, p.name, p.favorite_drink, p.description_id, "
    "p.location_name, p.nav_pose_id "
    "FROM person p JOIN person_category c ON p.category_id = c.id "
    "WHERE c.name = ? "
    "ORDER BY p.id ASC");
  stmt.bind(1, personTypeToText(type));
  while (stmt.executeStep()) {
    std::int64_t personId = stmt.getColumn(0).getInt64();
    std::string name = stmt.getColumn(1).getString();
    std::string favoriteDrink = stmt.getColumn(2).getString();
    std::optional<std::int64_t> descriptionId;
    if (!stmt.getColumn(3).isNull()) {
      descriptionId = stmt.getColumn(3).getInt64();
    }
    std::optional<std::string> locationName;
    if (!stmt.getColumn(4).isNull()) {
      locationName = stmt.getColumn(4).getString();
    }
    std::optional<std::int64_t> navPoseId;
    if (!stmt.getColumn(5).isNull()) {
      navPoseId = stmt.getColumn(5).getInt64();
    }

    result.push_back(buildPersonFromRow(db, personId, name, type, favoriteDrink,
                                        descriptionId, locationName, navPoseId,
                                        includePhoto));
  }
  return result;
}

} // namespace

std::vector<Person> KnowledgeRepository::loadPersonsByType(PersonType type)
{
  try {
    return loadPersonsByTypeImpl(*db_, type, /*includePhoto=*/false);
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("loadPersonsByType failed: ") + e.what());
    throw RepositoryException(std::string("loadPersonsByType failed: ") +
                              e.what());
  }
}

std::vector<Person>
KnowledgeRepository::loadPersonsByTypeWithPhoto(PersonType type)
{
  try {
    return loadPersonsByTypeImpl(*db_, type, /*includePhoto=*/true);
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("loadPersonsByTypeWithPhoto failed: ") +
                  e.what());
    throw RepositoryException(
        std::string("loadPersonsByTypeWithPhoto failed: ") + e.what());
  }
}

void KnowledgeRepository::deletePerson(const std::string & name)
{
  try {
    SQLite::Transaction transaction(*db_);

    std::optional<std::int64_t> descriptionId;
    std::optional<std::int64_t> navPoseId;
    {
      SQLite::Statement owned(
        *db_,
        "SELECT description_id, nav_pose_id FROM person WHERE name = ?");
      owned.bind(1, name);
      if (owned.executeStep()) {
        descriptionId = optionalInt64(owned.getColumn(0));
        navPoseId = optionalInt64(owned.getColumn(1));
      }
    }

    // Objects this person was holding need no explicit handling here:
    // object_state.held_by_person_id is ON DELETE SET NULL, and "no
    // owner column populated" is the valid UNKNOWN state, so they
    // become unknown instead of pointing at a missing person.
    SQLite::Statement stmt(*db_, "DELETE FROM person WHERE name = ?");
    stmt.bind(1, name);
    stmt.exec();

    // The person owned these rows exclusively; remove them with it.
    // Their KnownLocation is shared and is deliberately left alone.
    deletePoseIfPresent(*db_, navPoseId);
    if (descriptionId.has_value()) {
      SQLite::Statement dropDescription(
        *db_, "DELETE FROM person_description WHERE id = ?");
      dropDescription.bind(1, *descriptionId);
      dropDescription.exec();
    }

    transaction.commit();
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("deletePerson failed: ") + e.what());
    throw RepositoryException(std::string("deletePerson failed: ") + e.what());
  }
}

// ---------------------------------------------------------------------
// Robot (singleton row, id = 1)
// ---------------------------------------------------------------------

void KnowledgeRepository::saveRobot(const Robot & robot)
{
  try {
    SQLite::Transaction transaction(*db_);

    saveKnownLocation(robot.getStartLocation());
    saveKnownLocation(robot.getKnownLocation());

    SQLite::Statement stmt(
      *db_,
      "INSERT INTO robot (id, name, start_location_name, location_name) "
      "VALUES (1, ?, ?, ?) "
      "ON CONFLICT(id) DO UPDATE SET "
      "name = excluded.name, "
      "start_location_name = excluded.start_location_name, "
      "location_name = excluded.location_name");
    stmt.bind(1, robot.getName());
    stmt.bind(2, robot.getStartLocation().getName());
    stmt.bind(3, robot.getKnownLocation().getName());
    stmt.exec();

    // Hands: reflected in each held object's own object_state row,
    // per the design document ("Whichever hand holds an object, if
    // any, is reflected in that object's own ObjectOwnership when it
    // is separately loaded"). Persist each held object here so the
    // robot's hand state and the object's ownership never disagree.
    for (RobotHand hand : {RobotHand::LEFT, RobotHand::RIGHT}) {
      std::optional<ManipulableObject> held = robot.getObjectInHand(hand);
      if (held.has_value()) {
        ManipulableObject heldWithOwnership = *held;
        heldWithOwnership.setOwnership(ObjectOwnership::heldByRobot(hand));
        saveObjectImpl(*db_, heldWithOwnership);
      }
    }

    transaction.commit();
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("saveRobot failed: ") + e.what());
    throw RepositoryException(std::string("saveRobot failed: ") + e.what());
  }
}

std::optional<Robot> KnowledgeRepository::loadRobot()
{
  try {
    SQLite::Statement stmt(*db_,
      "SELECT name, start_location_name, location_name "
      "FROM robot WHERE id = 1");
    if (!stmt.executeStep()) {
      // No robot persisted yet: there is no meaningful "empty" Robot
      // (a Robot always has a name and a start location), so "not
      // found" is reported as std::nullopt, like every other load.
      return std::nullopt;
    }

    std::string name = stmt.getColumn(0).getString();
    std::string startLocationName = stmt.getColumn(1).getString();
    std::string currentLocationName = stmt.getColumn(2).getString();

    // Both columns are NOT NULL with ON DELETE RESTRICT, so a missing
    // location row can only mean a corrupted database.
    std::optional<KnownLocation> startLocation =
      findKnownLocation(*db_, startLocationName);
    std::optional<KnownLocation> currentLocation =
      findKnownLocation(*db_, currentLocationName);
    if (!startLocation.has_value() || !currentLocation.has_value()) {
      logger_.error("loadRobot: robot references a missing known location");
      throw RepositoryException("robot references a missing known location ('" +
                                startLocationName + "' / '" +
                                currentLocationName + "')");
    }

    Robot robot(name, *startLocation);
    robot.setKnownLocation(*currentLocation);

    // Resolve which object, if any, each hand holds by scanning
    // object_state for held_by_robot_hand entries. The unique index
    // on held_by_robot_hand guarantees at most one row per hand.
    for (RobotHand hand : {RobotHand::LEFT, RobotHand::RIGHT}) {
      SQLite::Statement handStmt(*db_, "SELECT o.name FROM object_state os "
        "JOIN object o ON os.object_id = o.id "
        "WHERE os.held_by_robot_hand = ?");
      handStmt.bind(1, handToText(hand));
      if (handStmt.executeStep()) {
        std::string heldObjectName = handStmt.getColumn(0).getString();
        auto heldObject = loadObjectByName(heldObjectName);
        if (heldObject.has_value()) {
          robot.setObjectInHand(hand, *heldObject);
        }
      }
    }

    return robot;
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("loadRobot failed: ") + e.what());
    throw RepositoryException(std::string("loadRobot failed: ") + e.what());
  }
}

// ---------------------------------------------------------------------
// Run-state reset
// ---------------------------------------------------------------------

void KnowledgeRepository::resetRuntimeState()
{
  try {
    SQLite::Transaction transaction(*db_);

    // 1. Objects: every ownership becomes unknown (all three owner
    //    columns NULL). Doing this first means no object refers to a
    //    person or to a robot hand by the time persons are removed.
    //    This also frees both of the robot's hands, since hands are
    //    derived from object_state.
    db_->exec("UPDATE object_state SET location_name = NULL, "
              "held_by_person_id = NULL, held_by_robot_hand = NULL");

    // 2. Objects: remove the navigable and grasp pose rows. The
    //    objects owned them exclusively, and object.nav_pose_id /
    //    manip_pose_id are ON DELETE SET NULL, so the columns clear
    //    themselves. The objects themselves are NOT deleted.
    db_->exec("DELETE FROM pose_3d WHERE id IN ("
              "SELECT nav_pose_id FROM object WHERE nav_pose_id IS NOT NULL "
              "UNION SELECT manip_pose_id FROM object WHERE manip_pose_id IS "
              "NOT NULL)");

    // 3. Persons: remove every navigable pose row (hosts included: a
    //    host's pose is unknown at the start of a run; person.nav_pose_id
    //    is ON DELETE SET NULL).
    db_->exec("DELETE FROM pose_3d WHERE id IN ("
              "SELECT nav_pose_id FROM person WHERE nav_pose_id IS NOT NULL)");

    // 4. Persons: delete every person except HOST, together with the
    //    description rows they owned. person_order rows go with them
    //    (ON DELETE CASCADE). KnownLocation rows are left alone.
    //    "NOT IN (subquery)" also behaves correctly when no HOST
    //    category row exists yet (then every person is deleted).
    const std::string hostName = personTypeToText(PersonType::HOST);

    SQLite::Statement dropDescriptions(
      *db_,
      "DELETE FROM person_description WHERE id IN ("
      "SELECT description_id FROM person WHERE description_id IS NOT NULL "
      "AND category_id NOT IN "
      "(SELECT id FROM person_category WHERE name = ?))");
    dropDescriptions.bind(1, hostName);
    dropDescriptions.exec();

    SQLite::Statement dropPersons(
      *db_, "DELETE FROM person WHERE category_id NOT IN "
      "(SELECT id FROM person_category WHERE name = ?)");
    dropPersons.bind(1, hostName);
    dropPersons.exec();

    // 5. Robot: the current location goes back to the start location.
    //    If no robot is stored, this updates zero rows.
    db_->exec("UPDATE robot SET location_name = start_location_name "
              "WHERE id = 1");

    transaction.commit();
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("resetRuntimeState failed: ") + e.what());
    throw RepositoryException(std::string("resetRuntimeState failed: ") +
                              e.what());
  }
}

} // namespace knowledge_manager::persistence
