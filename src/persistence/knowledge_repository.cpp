#include "knowledge_manager/persistence/knowledge_repository.hpp"

#include <fstream>
#include <sstream>

#include <SQLiteCpp/SQLiteCpp.h>

#include "knowledge_manager/domain/object_ownership.hpp"
#include "knowledge_manager/domain/robot_hand.hpp"
#include "knowledge_manager/persistence/repository_exception.hpp"

namespace knowledge_manager::persistence
{

using domain::KnownLocation;
using domain::ManipulableObject;
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
  throw RepositoryException("Unrecognized person_category name in database: '" +
                            text + "'");
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
    // safe and a no-op.
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
// every save inserts a fresh pose_3d row and returns its id, rather
// than updating one in place. This mirrors how the domain has no
// getId() on Pose3D to update by.
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

} // namespace

// ---------------------------------------------------------------------
// KnownLocation
// ---------------------------------------------------------------------

void KnowledgeRepository::saveKnownLocation(const KnownLocation & location)
{
  try {
    std::int64_t poseId = insertPose(*db_, location.getPose());

    SQLite::Statement stmt(
      *db_, "INSERT INTO known_location (name, pose_id) VALUES (?, ?) "
      "ON CONFLICT(name) DO UPDATE SET pose_id = excluded.pose_id");
    stmt.bind(1, location.getName());
    stmt.bind(2, poseId);
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
    SQLite::Statement stmt(*db_,
      "SELECT pose_id FROM known_location WHERE name = ?");
    stmt.bind(1, name);
    if (!stmt.executeStep()) {
      return std::nullopt;
    }

    std::optional<std::int64_t> poseId;
    if (!stmt.getColumn(0).isNull()) {
      poseId = stmt.getColumn(0).getInt64();
    }

    return KnownLocation(name, loadPoseOrDefault(*db_, poseId));
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("loadKnownLocationByName failed: ") + e.what());
    throw RepositoryException(std::string("loadKnownLocationByName failed: ") +
                              e.what());
  }
}

std::vector<KnownLocation> KnowledgeRepository::loadAllKnownLocations()
{
  std::vector<KnownLocation> result;
  try {
    SQLite::Statement stmt(*db_, "SELECT name, pose_id FROM known_location");
    while (stmt.executeStep()) {
      std::string name = stmt.getColumn(0).getString();
      std::optional<std::int64_t> poseId;
      if (!stmt.getColumn(1).isNull()) {
        poseId = stmt.getColumn(1).getInt64();
      }
      result.emplace_back(name, loadPoseOrDefault(*db_, poseId));
    }
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("loadAllKnownLocations failed: ") + e.what());
    throw RepositoryException(std::string("loadAllKnownLocations failed: ") +
                              e.what());
  }
  return result;
}

// ---------------------------------------------------------------------
// ManipulableObject / ObjectOwnership (exclusive-arc)
// ---------------------------------------------------------------------

namespace
{

// Resolves the object_category.id for a given category name,
// inserting the category if it is not yet known. ObjectCategory
// (config package) is the source of truth for which category names
// are *valid*; this repository does not re-validate that here (see
// WorldStateService, which owns that check) -- it only needs an id to
// store the already-accepted category string against.
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
// ObjectOwnership. Exactly one of the three columns is set, mirroring
// the CHECK constraint in schema.sql and the domain-level guarantee
// ObjectOwnership already provides by construction.
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

  if (ownership.isAtLocation()) {
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
// held_by_person_id back to a person name.
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
  // The CHECK constraint in schema.sql should make this unreachable;
  // treated as a data-integrity failure rather than silently
  // defaulted, since there is no sensible default ownership.
  throw RepositoryException(
      "object_state row has no ownership set (violates exclusive-arc)");
}

// Builds a full ManipulableObject from an `object` row already
// positioned on the current row of objStmt, joined with its
// object_state row. Shared by loadObjectByName and
// loadObjectsByCategory to avoid duplicating the same assembly logic
// (Clean Code: don't repeat yourself across near-identical query
// paths).
ManipulableObject buildObjectFromRow(
  SQLite::Database & db,
  std::int64_t objectId,
  const std::string & name,
  const std::string & categoryName,
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

  ManipulableObject object(name, categoryName, ownership);
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
  std::int64_t categoryId = resolveObjectCategoryId(db, object.getCategory());

  std::optional<std::int64_t> navPoseId;
  if (object.getNavigablePose().has_value()) {
    navPoseId = insertPose(db, *object.getNavigablePose());
  }
  std::optional<std::int64_t> graspPoseId;
  if (object.getGraspPose().has_value()) {
    graspPoseId = insertPose(db, *object.getGraspPose());
  }

  SQLite::Statement stmt(
    db,
    "INSERT INTO object (name, category_id, nav_pose_id, manip_pose_id) "
    "VALUES (?, ?, ?, ?) "
    "ON CONFLICT(name) DO UPDATE SET "
    "category_id = excluded.category_id, "
    "nav_pose_id = COALESCE(excluded.nav_pose_id, object.nav_pose_id), "
    "manip_pose_id = COALESCE(excluded.manip_pose_id, object.manip_pose_id)");
  stmt.bind(1, object.getName());
  stmt.bind(2, categoryId);
  if (navPoseId.has_value()) {
    stmt.bind(3, *navPoseId);
  } else {
    stmt.bind(3);
  }
  if (graspPoseId.has_value()) {
    stmt.bind(4, *graspPoseId);
  } else {
    stmt.bind(4);
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
    SQLite::Statement stmt(*db_, "SELECT id, category_id, nav_pose_id, "
      "manip_pose_id FROM object WHERE name = ?");
    stmt.bind(1, name);
    if (!stmt.executeStep()) {
      return std::nullopt;
    }

    std::int64_t objectId = stmt.getColumn(0).getInt64();
    std::string categoryName =
      objectCategoryNameById(*db_, stmt.getColumn(1).getInt64());
    std::optional<std::int64_t> navPoseId;
    if (!stmt.getColumn(2).isNull()) {
      navPoseId = stmt.getColumn(2).getInt64();
    }
    std::optional<std::int64_t> graspPoseId;
    if (!stmt.getColumn(3).isNull()) {
      graspPoseId = stmt.getColumn(3).getInt64();
    }

    return buildObjectFromRow(*db_, objectId, name, categoryName, navPoseId,
                              graspPoseId);
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("loadObjectByName failed: ") + e.what());
    throw RepositoryException(std::string("loadObjectByName failed: ") +
                              e.what());
  }
}

std::vector<ManipulableObject>
KnowledgeRepository::loadObjectsByCategory(const std::string & category)
{
  std::vector<ManipulableObject> result;
  try {
    SQLite::Statement stmt(
      *db_, "SELECT o.id, o.name, o.nav_pose_id, o.manip_pose_id "
      "FROM object o JOIN object_category c ON o.category_id = c.id "
      "WHERE c.name = ?");
    stmt.bind(1, category);
    while (stmt.executeStep()) {
      std::int64_t objectId = stmt.getColumn(0).getInt64();
      std::string name = stmt.getColumn(1).getString();
      std::optional<std::int64_t> navPoseId;
      if (!stmt.getColumn(2).isNull()) {
        navPoseId = stmt.getColumn(2).getInt64();
      }
      std::optional<std::int64_t> graspPoseId;
      if (!stmt.getColumn(3).isNull()) {
        graspPoseId = stmt.getColumn(3).getInt64();
      }

      result.push_back(buildObjectFromRow(*db_, objectId, name, category,
                                          navPoseId, graspPoseId));
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
    SQLite::Statement stmt(*db_, "DELETE FROM object WHERE name = ?");
    stmt.bind(1, name);
    stmt.exec();
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
    db, "SELECT o.id, o.name, o.category_id, o.nav_pose_id, o.manip_pose_id "
    "FROM person_order po JOIN object o ON po.object_id = o.id "
    "WHERE po.person_id = ? ORDER BY po.seq");
  stmt.bind(1, personId);
  while (stmt.executeStep()) {
    std::int64_t objectId = stmt.getColumn(0).getInt64();
    std::string name = stmt.getColumn(1).getString();
    std::string categoryName =
      objectCategoryNameById(db, stmt.getColumn(2).getInt64());
    std::optional<std::int64_t> navPoseId;
    if (!stmt.getColumn(3).isNull()) {
      navPoseId = stmt.getColumn(3).getInt64();
    }
    std::optional<std::int64_t> graspPoseId;
    if (!stmt.getColumn(4).isNull()) {
      graspPoseId = stmt.getColumn(4).getInt64();
    }

    order.push_back(buildObjectFromRow(db, objectId, name, categoryName,
                                       navPoseId, graspPoseId));
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

Person buildPersonFromRow(
  SQLite::Database & db, std::int64_t personId,
  const std::string & name, PersonType type,
  const std::string & favoriteDrink,
  std::optional<std::int64_t> descriptionId,
  const std::optional<std::string> & locationName,
  std::optional<std::int64_t> navPoseId)
{
  KnownLocation location;
  if (locationName.has_value()) {
    SQLite::Statement locStmt(
      db, "SELECT pose_id FROM known_location WHERE name = ?");
    locStmt.bind(1, *locationName);
    std::optional<std::int64_t> poseId;
    if (locStmt.executeStep() && !locStmt.getColumn(0).isNull()) {
      poseId = locStmt.getColumn(0).getInt64();
    }
    location = KnownLocation(*locationName, loadPoseOrDefault(db, poseId));
  }

  Person person(name, type, location);
  person.setPhysicalDescription(
      loadPersonDescriptionOrDefault(db, descriptionId));
  person.setFavoriteDrink(favoriteDrink);
  if (navPoseId.has_value()) {
    person.setNavigablePose(loadPoseOrDefault(db, navPoseId));
  }
  person.setOrder(loadPersonOrder(db, personId));
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
    std::int64_t descriptionId =
      insertPersonDescription(*db_, person.getPhysicalDescription());

    std::optional<std::int64_t> navPoseId;
    if (person.getNavigablePose().has_value()) {
      navPoseId = insertPose(*db_, *person.getNavigablePose());
    }

    std::string locationName = person.getKnownLocation().getName();
    bool hasLocation = !locationName.empty();
    if (hasLocation) {
      saveKnownLocation(person.getKnownLocation());
    }

    SQLite::Statement stmt(
      *db_,
      "INSERT INTO person (name, category_id, description_id, "
      "favorite_drink, location_name, nav_pose_id) VALUES (?, ?, ?, ?, ?, ?) "
      "ON CONFLICT(name) DO UPDATE SET "
      "category_id = excluded.category_id, "
      "description_id = excluded.description_id, "
      "favorite_drink = excluded.favorite_drink, "
      "location_name = excluded.location_name, "
      "nav_pose_id = COALESCE(excluded.nav_pose_id, person.nav_pose_id)");
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

std::optional<Person>
KnowledgeRepository::loadPersonByName(const std::string & name)
{
  try {
    SQLite::Statement stmt(
      *db_, "SELECT id, category_id, favorite_drink, description_id, "
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
      *db_, "SELECT name FROM person_category WHERE id = ?");
    categoryLookup.bind(1, categoryId);
    categoryLookup.executeStep();
    PersonType type = textToPersonType(categoryLookup.getColumn(0).getString());

    return buildPersonFromRow(*db_, personId, name, type, favoriteDrink,
                              descriptionId, locationName, navPoseId);
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("loadPersonByName failed: ") + e.what());
    throw RepositoryException(std::string("loadPersonByName failed: ") +
                              e.what());
  }
}

std::vector<Person> KnowledgeRepository::loadPersonsByType(PersonType type)
{
  std::vector<Person> result;
  try {
    SQLite::Statement stmt(
      *db_, "SELECT p.id, p.name, p.favorite_drink, p.description_id, "
      "p.location_name, p.nav_pose_id "
      "FROM person p JOIN person_category c ON p.category_id = c.id "
      "WHERE c.name = ?");
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

      result.push_back(buildPersonFromRow(*db_, personId, name, type,
                                          favoriteDrink, descriptionId,
                                          locationName, navPoseId));
    }
  } catch (const SQLite::Exception & e) {
    logger_.error(std::string("loadPersonsByType failed: ") + e.what());
    throw RepositoryException(std::string("loadPersonsByType failed: ") +
                              e.what());
  }
  return result;
}

void KnowledgeRepository::deletePerson(const std::string & name)
{
  try {
    SQLite::Statement stmt(*db_, "DELETE FROM person WHERE name = ?");
    stmt.bind(1, name);
    stmt.exec();
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

    saveKnownLocation(robot.getKnownLocation());

    SQLite::Statement stmt(
      *db_, "INSERT INTO robot (id, name, location_name) VALUES (1, ?, ?) "
      "ON CONFLICT(id) DO UPDATE SET "
      "name = excluded.name, location_name = excluded.location_name");
    stmt.bind(1, robot.getName());
    stmt.bind(2, robot.getKnownLocation().getName());
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

Robot KnowledgeRepository::loadRobot()
{
  try {
    SQLite::Statement stmt(
      *db_, "SELECT name, location_name FROM robot WHERE id = 1");
    if (!stmt.executeStep()) {
      // No robot persisted yet: return a default-constructed Robot,
      // mirroring the same "no data yet -> default value object"
      // stance used for Pose3D, rather than throwing for what is a
      // normal state on a completely fresh database.
      return Robot();
    }

    std::string name = stmt.getColumn(0).getString();
    std::optional<std::string> locationName;
    if (!stmt.getColumn(1).isNull()) {
      locationName = stmt.getColumn(1).getString();
    }

    KnownLocation location;
    if (locationName.has_value()) {
      auto loaded = loadKnownLocationByName(*locationName);
      if (loaded.has_value()) {
        location = *loaded;
      }
    }

    Robot robot(name, location);

    // Resolve which object, if any, each hand holds by scanning
    // object_state for held_by_robot_hand entries.
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

} // namespace knowledge_manager::persistence
