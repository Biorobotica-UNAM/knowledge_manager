#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <SQLiteCpp/SQLiteCpp.h>

#include "knowledge_manager/domain/known_location.hpp"
#include "knowledge_manager/domain/location_zone.hpp"
#include "knowledge_manager/domain/manipulable_object.hpp"
#include "knowledge_manager/domain/object_category.hpp"
#include "knowledge_manager/domain/object_ownership.hpp"
#include "knowledge_manager/domain/person.hpp"
#include "knowledge_manager/domain/robot.hpp"
#include "knowledge_manager/persistence/knowledge_repository.hpp"
#include "knowledge_manager/persistence/logger.hpp"
#include "knowledge_manager/persistence/repository_exception.hpp"

using namespace knowledge_manager::domain;
using namespace knowledge_manager::persistence;

namespace
{

// Minimal Logger for tests: writes to stdout/stderr like the
// production ConsoleLogger, kept local to this test file so the test
// suite does not depend on a specific logger implementation living
// elsewhere in the codebase.
class TestLogger : public Logger {
public:
  void error(const std::string & message) override
  {
    std::fprintf(stderr, "[ERROR] %s\n", message.c_str());
  }
  void warn(const std::string & message) override
  {
    std::fprintf(stderr, "[WARN] %s\n", message.c_str());
  }
  void info(const std::string & message) override
  {
    std::fprintf(stdout, "[INFO] %s\n", message.c_str());
  }
};

// Names of a list of objects/persons/locations, in the order given.
// Used to assert FIFO order with a single readable EXPECT_EQ.
template<typename T>
std::vector<std::string> namesOf(const std::vector<T> & items)
{
  std::vector<std::string> names;
  for (const auto & item : items) {
    names.push_back(item.getName());
  }
  return names;
}

// Same, but sorted: for queries whose order is not part of the contract
// (known locations by zone).
template<typename T>
std::vector<std::string> sortedNamesOf(const std::vector<T> & items)
{
  std::vector<std::string> names = namesOf(items);
  std::sort(names.begin(), names.end());
  return names;
}

Pose3D poseAt(float x, float y = 0.0f)
{
  return Pose3D(x, y, 0.0f, 0, 0, 0, 1);
}

// "kitchen" is a LocationZone, not a location name: specific places
// have specific names and belong to exactly one zone.
KnownLocation kitchenCounter(const Pose3D & pose = Pose3D())
{
  return KnownLocation("kitchen_counter", pose, LocationZone::KITCHEN);
}

KnownLocation sofa(const Pose3D & pose = Pose3D())
{
  return KnownLocation("sofa", pose, LocationZone::LIVING_ROOM);
}

ManipulableObject objectAtCounter(
  const std::string & name,
  ObjectCategory category)
{
  return ManipulableObject(name, category, LocationZone::KITCHEN,
                           ObjectOwnership::atLocation("kitchen_counter"));
}

ManipulableObject objectHeldByRobot(
  const std::string & name,
  ObjectCategory category, RobotHand hand)
{
  return ManipulableObject(name, category, LocationZone::KITCHEN,
                           ObjectOwnership::heldByRobot(hand));
}

const std::vector<std::byte> kHostPhoto{std::byte{0x01}, std::byte{0x02},
  std::byte{0x03}};

// gtest fixture: gives every TEST_F below a fresh database file and a
// fresh KnowledgeRepository, and cleans up afterward. This avoids
// repeating the same setup/teardown code in each test (Clean Code:
// don't repeat yourself), and keeps tests independent from each other
// -- one test's data never leaks into the next.
class KnowledgeRepositoryTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    dbPath_ = "/tmp/kb_gtest.db";
    std::remove(dbPath_.c_str());
    repo_ =
      std::make_unique<KnowledgeRepository>(dbPath_, schemaPath_, logger_);
  }

  void TearDown() override
  {
    repo_.reset(); // close the connection before removing the file
    std::remove(dbPath_.c_str());
  }

  // Simulates a process restart: closes the repository and opens a new
  // one against the same database file.
  void restart()
  {
    repo_.reset();
    repo_ =
      std::make_unique<KnowledgeRepository>(dbPath_, schemaPath_, logger_);
  }

  // Objects whose ownership is "at location" need that location to
  // exist first (foreign key).
  void saveCounter() {repo_->saveKnownLocation(kitchenCounter());}

  // A small but complete world, used by the resetRuntimeState tests:
  //   locations: kitchen_counter (KITCHEN), sofa (LIVING_ROOM)
  //   persons:   Ana (GUEST, with order [apple]), Host (HOST, with photo)
  //   objects:   apple (at the counter), towel (held by Ana, usual zone
  //              LAUNDRY), spoon (in the robot's right hand)
  //   robot:     starts at the sofa, currently at the counter
  // Every object, person and the robot carries poses, so that the reset
  // has something to clear.
  void populateWorld()
  {
    const KnownLocation counter = kitchenCounter(poseAt(1.0f, 2.0f));
    const KnownLocation couch = sofa(poseAt(3.0f, 4.0f));
    repo_->saveKnownLocation(counter);
    repo_->saveKnownLocation(couch);

    // Persons first: object ownership refers to them by name.
    PhysicalDescription anaLook;
    anaLook.setClothes("red shirt");
    Person ana("Ana", PersonType::GUEST, counter);
    ana.setPhysicalDescription(anaLook);
    ana.setNavigablePose(poseAt(10.0f));
    repo_->savePerson(ana);

    PhysicalDescription hostLook;
    hostLook.setClothes("blue jacket");
    Person host("Host", PersonType::HOST, couch);
    host.setPhysicalDescription(hostLook);
    host.setNavigablePose(poseAt(11.0f));
    host.setPhoto(kHostPhoto);
    repo_->savePerson(host);

    ManipulableObject apple = objectAtCounter("apple", ObjectCategory::FRUITS);
    apple.setNavigablePose(poseAt(20.0f));
    apple.setGraspPose(poseAt(21.0f));
    repo_->saveObject(apple);

    repo_->saveObject(ManipulableObject("towel", ObjectCategory::FABRIC,
                                        LocationZone::LAUNDRY,
                                        ObjectOwnership::heldByPerson("Ana")));

    ana.setOrder({apple});
    repo_->savePerson(ana);

    ManipulableObject spoon =
      objectHeldByRobot("spoon", ObjectCategory::CUTLERY, RobotHand::RIGHT);
    spoon.setNavigablePose(poseAt(30.0f));
    spoon.setGraspPose(poseAt(31.0f));
    Robot robot("Justina", couch);
    robot.setKnownLocation(counter);
    robot.setObjectInHand(RobotHand::RIGHT, spoon);
    repo_->saveRobot(robot);

    // Precondition of every reset test: the scenario really holds what
    // the reset is supposed to clear, so a test cannot pass trivially.
    EXPECT_EQ(countRows("pose_3d"), 8);
    EXPECT_EQ(countRows("person_description"), 2);
    EXPECT_EQ(countRows("person"), 2);
    EXPECT_EQ(countRows("person_order"), 1);
    EXPECT_EQ(countRows("object"), 3);
  }

  // -- Inspecting the database from outside the repository -----------
  //
  // The repository deliberately hides its tables, so the "no orphan
  // rows" tests open a second, read-only connection to the same file
  // and count rows directly. This is test-only: production code never
  // does this. All saves in the repository are committed by the time
  // they return, so this connection always sees the final state.

  std::int64_t scalar(const std::string & sql)
  {
    SQLite::Database db(dbPath_, SQLite::OPEN_READONLY);
    SQLite::Statement query(db, sql);
    query.executeStep();
    return query.getColumn(0).getInt64();
  }

  std::int64_t countRows(const std::string & table)
  {
    return scalar("SELECT COUNT(*) FROM " + table);
  }

  // pose_3d rows that no known_location, object or person points to.
  std::int64_t countOrphanPoses()
  {
    return scalar(
        "SELECT COUNT(*) FROM pose_3d WHERE id NOT IN ("
        " SELECT pose_id FROM known_location WHERE pose_id IS NOT NULL"
        " UNION SELECT nav_pose_id FROM object WHERE nav_pose_id IS NOT NULL"
        " UNION SELECT manip_pose_id FROM object WHERE manip_pose_id IS NOT "
        "NULL"
        " UNION SELECT nav_pose_id FROM person WHERE nav_pose_id IS NOT NULL)");
  }

  // person_description rows that no person points to.
  std::int64_t countOrphanDescriptions()
  {
    return scalar(
        "SELECT COUNT(*) FROM person_description WHERE id NOT IN ("
        " SELECT description_id FROM person WHERE description_id IS NOT NULL)");
  }

  // object_state rows with no owner at all (ownership unknown).
  std::int64_t countObjectsWithUnknownOwnership()
  {
    return scalar(
        "SELECT COUNT(*) FROM object_state WHERE location_name IS NULL"
        " AND held_by_person_id IS NULL AND held_by_robot_hand IS NULL");
  }

  // KNOWLEDGE_MANAGER_SCHEMA_PATH is injected by CMake (see
  // target_compile_definitions in CMakeLists.txt) as an absolute path
  // to schema.sql in the package source tree. A relative path here
  // would be fragile: ctest/colcon test invoke this binary from the
  // build directory, not from the package root, so "schema.sql" alone
  // would not resolve.
  std::string schemaPath_ = KNOWLEDGE_MANAGER_SCHEMA_PATH;
  std::string dbPath_;
  TestLogger logger_;
  std::unique_ptr<KnowledgeRepository> repo_;
};

} // namespace

// -- KnownLocation --------------------------------------------------

TEST_F(KnowledgeRepositoryTest, SaveAndLoadKnownLocation) {
  repo_->saveKnownLocation(kitchenCounter(poseAt(1.0f, 2.0f)));

  auto loaded = repo_->loadKnownLocationByName("kitchen_counter");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_FLOAT_EQ(loaded->getPose().getX(), 1.0f);
  EXPECT_FLOAT_EQ(loaded->getPose().getY(), 2.0f);
  EXPECT_EQ(loaded->getZone(), LocationZone::KITCHEN);
}

TEST_F(KnowledgeRepositoryTest,
       LoadKnownLocationByName_NotFound_ReturnsNullopt) {
  EXPECT_EQ(repo_->loadKnownLocationByName("nonexistent"), std::nullopt);
}

TEST_F(KnowledgeRepositoryTest, SavedKnownLocation_RoundTripsEveryZone) {
  const std::vector<std::pair<std::string, LocationZone>> places{
    {"kitchen_counter", LocationZone::KITCHEN},
    {"sofa", LocationZone::LIVING_ROOM},
    {"bed", LocationZone::BEDROOM},
    {"washing_machine", LocationZone::LAUNDRY}};
  for (const auto &[name, zone] : places) {
    repo_->saveKnownLocation(KnownLocation(name, Pose3D(), zone));
  }

  for (const auto &[name, zone] : places) {
    auto loaded = repo_->loadKnownLocationByName(name);
    ASSERT_TRUE(loaded.has_value()) << name;
    EXPECT_EQ(loaded->getZone(), zone) << name;
  }
}

TEST_F(KnowledgeRepositoryTest, LoadKnownLocationsByZone_ReturnsOnlyThatZone) {
  repo_->saveKnownLocation(
      KnownLocation("kitchen_counter", Pose3D(), LocationZone::KITCHEN));
  repo_->saveKnownLocation(
      KnownLocation("fridge", Pose3D(), LocationZone::KITCHEN));
  repo_->saveKnownLocation(sofa());
  repo_->saveKnownLocation(
      KnownLocation("bed", Pose3D(), LocationZone::BEDROOM));

  EXPECT_EQ(
      sortedNamesOf(repo_->loadKnownLocationsByZone(LocationZone::KITCHEN)),
    (std::vector<std::string>{"fridge", "kitchen_counter"}));
  EXPECT_EQ(
      sortedNamesOf(repo_->loadKnownLocationsByZone(LocationZone::LIVING_ROOM)),
    (std::vector<std::string>{"sofa"}));
}

TEST_F(KnowledgeRepositoryTest,
       LoadKnownLocationsByZone_ZoneWithoutLocations_ReturnsEmptyVector) {
  saveCounter();
  EXPECT_TRUE(repo_->loadKnownLocationsByZone(LocationZone::LAUNDRY).empty());
}

TEST_F(KnowledgeRepositoryTest, LoadAllKnownLocations_ReturnsEveryZone) {
  saveCounter();
  repo_->saveKnownLocation(sofa());

  EXPECT_EQ(sortedNamesOf(repo_->loadAllKnownLocations()),
    (std::vector<std::string>{"kitchen_counter", "sofa"}));
}

TEST_F(KnowledgeRepositoryTest, ReSavingKnownLocation_CanMoveItToAnotherZone) {
  repo_->saveKnownLocation(
      KnownLocation("table", Pose3D(), LocationZone::KITCHEN));
  repo_->saveKnownLocation(
      KnownLocation("table", Pose3D(), LocationZone::LIVING_ROOM));

  auto loaded = repo_->loadKnownLocationByName("table");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->getZone(), LocationZone::LIVING_ROOM);
  EXPECT_TRUE(repo_->loadKnownLocationsByZone(LocationZone::KITCHEN).empty());
}

// -- ManipulableObject / ObjectOwnership exclusive-arc ---------------

TEST_F(KnowledgeRepositoryTest, SavedObjectAtLocation_RoundTripsOwnership) {
  saveCounter();
  repo_->saveObject(objectAtCounter("apple", ObjectCategory::FRUITS));

  auto loaded = repo_->loadObjectByName("apple");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_TRUE(loaded->getOwnership().isAtLocation());
  EXPECT_EQ(loaded->getOwnership().getLocationName(), "kitchen_counter");
  EXPECT_EQ(loaded->getCategory(), ObjectCategory::FRUITS);
  EXPECT_EQ(loaded->getUsualZone(), LocationZone::KITCHEN);
}

TEST_F(KnowledgeRepositoryTest, SavedObjectHeldByRobot_RoundTripsOwnership) {
  repo_->saveObject(
      objectHeldByRobot("spoon", ObjectCategory::CUTLERY, RobotHand::RIGHT));

  auto loaded = repo_->loadObjectByName("spoon");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_TRUE(loaded->getOwnership().isHeldByRobot());
  EXPECT_EQ(loaded->getOwnership().getRobotHand(), RobotHand::RIGHT);
}

TEST_F(KnowledgeRepositoryTest, SavedObjectHeldByPerson_RoundTripsOwnership) {
  repo_->savePerson(Person("Ana", PersonType::GUEST, kitchenCounter()));

  repo_->saveObject(ManipulableObject("backpack", ObjectCategory::CONTAINERS,
                                      LocationZone::BEDROOM,
                                      ObjectOwnership::heldByPerson("Ana")));

  auto loaded = repo_->loadObjectByName("backpack");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_TRUE(loaded->getOwnership().isHeldByPerson());
  EXPECT_EQ(loaded->getOwnership().getPersonName(), "Ana");
}

TEST_F(KnowledgeRepositoryTest,
       SavedObjectWithUnknownOwnership_RoundTripsAsUnknown) {
  // Knowing an object (name, category, usual zone) does not require
  // knowing where it is: no location needs to exist for this to work.
  repo_->saveObject(ManipulableObject("apple", ObjectCategory::FRUITS,
                                      LocationZone::KITCHEN,
                                      ObjectOwnership::unknown()));

  auto loaded = repo_->loadObjectByName("apple");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_TRUE(loaded->getOwnership().isUnknown());
  EXPECT_FALSE(loaded->getOwnership().isAtLocation());
  EXPECT_FALSE(loaded->getOwnership().isHeldByPerson());
  EXPECT_FALSE(loaded->getOwnership().isHeldByRobot());
}

TEST_F(KnowledgeRepositoryTest, SavedObject_RoundTripsUsualZone) {
  repo_->saveObject(ManipulableObject("towel", ObjectCategory::FABRIC,
                                      LocationZone::LAUNDRY,
                                      ObjectOwnership::unknown()));

  auto loaded = repo_->loadObjectByName("towel");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->getUsualZone(), LocationZone::LAUNDRY);
}

TEST_F(KnowledgeRepositoryTest, ReSavingObject_UpdatesItsUsualZone) {
  repo_->saveObject(ManipulableObject("towel", ObjectCategory::FABRIC,
                                      LocationZone::LAUNDRY,
                                      ObjectOwnership::unknown()));
  repo_->saveObject(ManipulableObject("towel", ObjectCategory::FABRIC,
                                      LocationZone::BEDROOM,
                                      ObjectOwnership::unknown()));

  auto loaded = repo_->loadObjectByName("towel");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->getUsualZone(), LocationZone::BEDROOM);
}

TEST_F(KnowledgeRepositoryTest,
       ChangingOwnershipToUnknown_ForgetsThePreviousOwner) {
  saveCounter();
  repo_->saveObject(objectAtCounter("apple", ObjectCategory::FRUITS));

  repo_->saveObject(ManipulableObject("apple", ObjectCategory::FRUITS,
                                      LocationZone::KITCHEN,
                                      ObjectOwnership::unknown()));

  auto loaded = repo_->loadObjectByName("apple");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_TRUE(loaded->getOwnership().isUnknown());
  EXPECT_EQ(countObjectsWithUnknownOwnership(), 1);
}

TEST_F(KnowledgeRepositoryTest, LoadObjectsByCategory_ReturnsOnlyMatching) {
  saveCounter();
  repo_->saveObject(objectAtCounter("apple", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtCounter("banana", ObjectCategory::FRUITS));
  repo_->saveObject(
      objectHeldByRobot("spoon", ObjectCategory::CUTLERY, RobotHand::LEFT));

  auto fruits = repo_->loadObjectsByCategory(ObjectCategory::FRUITS);
  EXPECT_EQ(fruits.size(), 2u);
}

TEST_F(KnowledgeRepositoryTest, DeleteObject_RemovesIt) {
  repo_->saveObject(
      objectHeldByRobot("banana", ObjectCategory::FRUITS, RobotHand::LEFT));
  repo_->deleteObject("banana");
  EXPECT_EQ(repo_->loadObjectByName("banana"), std::nullopt);
}

TEST_F(KnowledgeRepositoryTest,
       SaveObject_UnknownPersonOwner_ThrowsRepositoryException) {
  ManipulableObject ghostOwned(
    "napkin", ObjectCategory::TRASH, LocationZone::KITCHEN,
    ObjectOwnership::heldByPerson("NobodyByThisName"));
  EXPECT_THROW(repo_->saveObject(ghostOwned), RepositoryException);
}

// -- A hand holds at most one object ---------------------------------

TEST_F(KnowledgeRepositoryTest,
       SaveObject_SecondObjectInTheSameHand_ThrowsAndStoresNothing) {
  repo_->saveObject(
      objectHeldByRobot("pear", ObjectCategory::FRUITS, RobotHand::LEFT));

  EXPECT_THROW(repo_->saveObject(objectHeldByRobot(
                   "kiwi", ObjectCategory::FRUITS, RobotHand::LEFT)),
               RepositoryException);

  // The failed save was rolled back as a whole: the second object does
  // not exist, and the first one still holds the hand.
  EXPECT_EQ(repo_->loadObjectByName("kiwi"), std::nullopt);
  auto pear = repo_->loadObjectByName("pear");
  ASSERT_TRUE(pear.has_value());
  EXPECT_TRUE(pear->getOwnership().isHeldByRobot());
}

TEST_F(KnowledgeRepositoryTest, ObjectsInDifferentHands_AreAccepted) {
  repo_->saveObject(
      objectHeldByRobot("pear", ObjectCategory::FRUITS, RobotHand::LEFT));
  EXPECT_NO_THROW(repo_->saveObject(
      objectHeldByRobot("kiwi", ObjectCategory::FRUITS, RobotHand::RIGHT)));
}

TEST_F(KnowledgeRepositoryTest, ReleasedHand_CanHoldAnotherObject) {
  saveCounter();
  repo_->saveObject(
      objectHeldByRobot("pear", ObjectCategory::FRUITS, RobotHand::LEFT));

  // The robot puts the pear down: the left hand is free again.
  repo_->saveObject(objectAtCounter("pear", ObjectCategory::FRUITS));

  EXPECT_NO_THROW(repo_->saveObject(
      objectHeldByRobot("kiwi", ObjectCategory::FRUITS, RobotHand::LEFT)));
}

// -- Object FIFO order (fruits_1, fruits_2, ... aliases) -------------
//
// loadObjectsByCategory returns objects in order of first
// registration. WorldStateService::getObjectByCategoryAndPosition and
// the plans' positional aliases depend on this.

TEST_F(KnowledgeRepositoryTest,
       LoadObjectsByCategory_ReturnsFirstRegistrationOrder_NotAlphabetical) {
  saveCounter();
  // Deliberately not alphabetical, so a name-ordered result would fail.
  repo_->saveObject(objectAtCounter("pear", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtCounter("apple", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtCounter("mango", ObjectCategory::FRUITS));

  auto fruits = repo_->loadObjectsByCategory(ObjectCategory::FRUITS);
  EXPECT_EQ(namesOf(fruits),
    (std::vector<std::string>{"pear", "apple", "mango"}));
}

TEST_F(KnowledgeRepositoryTest, LoadObjectsByCategory_OrderIsPerCategory) {
  saveCounter();
  repo_->saveObject(objectAtCounter("pear", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtCounter("fork", ObjectCategory::CUTLERY));
  repo_->saveObject(objectAtCounter("apple", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtCounter("knife", ObjectCategory::CUTLERY));

  EXPECT_EQ(namesOf(repo_->loadObjectsByCategory(ObjectCategory::FRUITS)),
    (std::vector<std::string>{"pear", "apple"}));
  EXPECT_EQ(namesOf(repo_->loadObjectsByCategory(ObjectCategory::CUTLERY)),
    (std::vector<std::string>{"fork", "knife"}));
}

TEST_F(KnowledgeRepositoryTest, ReSavingObject_KeepsItsOriginalPosition) {
  saveCounter();
  ManipulableObject pear = objectAtCounter("pear", ObjectCategory::FRUITS);
  repo_->saveObject(pear);
  repo_->saveObject(objectAtCounter("apple", ObjectCategory::FRUITS));

  // Vision refines pear's pose: the same object is saved again. It
  // must not move to the end of the queue (saveObject is an upsert,
  // not delete-and-reinsert).
  pear.setNavigablePose(poseAt(1.0f, 2.0f));
  repo_->saveObject(pear);
  pear.setNavigablePose(poseAt(3.0f, 4.0f));
  repo_->saveObject(pear);

  auto fruits = repo_->loadObjectsByCategory(ObjectCategory::FRUITS);
  EXPECT_EQ(namesOf(fruits), (std::vector<std::string>{"pear", "apple"}));
}

TEST_F(KnowledgeRepositoryTest,
       ObjectPosition_IsIndependentOfCurrentOwnership) {
  saveCounter();
  repo_->saveObject(objectAtCounter("pear", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtCounter("apple", ObjectCategory::FRUITS));

  // The robot picks up pear: it must still be fruits_1.
  repo_->saveObject(
      objectHeldByRobot("pear", ObjectCategory::FRUITS, RobotHand::LEFT));

  auto fruits = repo_->loadObjectsByCategory(ObjectCategory::FRUITS);
  ASSERT_EQ(fruits.size(), 2u);
  EXPECT_EQ(fruits[0].getName(), "pear");
  EXPECT_TRUE(fruits[0].getOwnership().isHeldByRobot());
  EXPECT_EQ(fruits[1].getName(), "apple");
}

TEST_F(KnowledgeRepositoryTest, DeleteObject_ShiftsPositionsOfLaterObjects) {
  // Documents the restriction stated on deleteObject: deleting an
  // object shifts the later ones up. Plans must not delete objects
  // while they refer to them by position.
  saveCounter();
  repo_->saveObject(objectAtCounter("pear", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtCounter("apple", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtCounter("mango", ObjectCategory::FRUITS));

  repo_->deleteObject("pear");

  EXPECT_EQ(namesOf(repo_->loadObjectsByCategory(ObjectCategory::FRUITS)),
    (std::vector<std::string>{"apple", "mango"}));

  // A newly registered object goes to the end of the queue.
  repo_->saveObject(objectAtCounter("kiwi", ObjectCategory::FRUITS));
  EXPECT_EQ(namesOf(repo_->loadObjectsByCategory(ObjectCategory::FRUITS)),
    (std::vector<std::string>{"apple", "mango", "kiwi"}));
}

// -- Object poses: no orphan rows ------------------------------------

TEST_F(KnowledgeRepositoryTest,
       ReSavingObjectWithNewPoses_UpdatesInPlace_NoOrphanPoses) {
  saveCounter();
  ManipulableObject apple = objectAtCounter("apple", ObjectCategory::FRUITS);
  apple.setNavigablePose(poseAt(1.0f));
  apple.setGraspPose(poseAt(2.0f));
  repo_->saveObject(apple);

  const std::int64_t posesAfterFirstSave = countRows("pose_3d");

  for (int i = 1; i <= 5; ++i) {
    apple.setNavigablePose(poseAt(10.0f + i));
    apple.setGraspPose(poseAt(20.0f + i));
    repo_->saveObject(apple);
  }

  EXPECT_EQ(countRows("pose_3d"), posesAfterFirstSave);
  EXPECT_EQ(countOrphanPoses(), 0);

  auto loaded = repo_->loadObjectByName("apple");
  ASSERT_TRUE(loaded.has_value());
  ASSERT_TRUE(loaded->getNavigablePose().has_value());
  ASSERT_TRUE(loaded->getGraspPose().has_value());
  EXPECT_FLOAT_EQ(loaded->getNavigablePose()->getX(), 15.0f);
  EXPECT_FLOAT_EQ(loaded->getGraspPose()->getX(), 25.0f);
}

TEST_F(KnowledgeRepositoryTest,
       SavingObjectWithoutPose_KeepsPreviouslyStoredPose) {
  saveCounter();
  ManipulableObject apple = objectAtCounter("apple", ObjectCategory::FRUITS);
  apple.setNavigablePose(poseAt(7.0f));
  repo_->saveObject(apple);

  // Saved again by someone who never loaded the pose (e.g. an
  // ownership change): the stored pose must not be wiped.
  repo_->saveObject(objectAtCounter("apple", ObjectCategory::FRUITS));

  auto loaded = repo_->loadObjectByName("apple");
  ASSERT_TRUE(loaded.has_value());
  ASSERT_TRUE(loaded->getNavigablePose().has_value());
  EXPECT_FLOAT_EQ(loaded->getNavigablePose()->getX(), 7.0f);
  EXPECT_EQ(countOrphanPoses(), 0);
}

TEST_F(KnowledgeRepositoryTest, DeleteObject_RemovesThePosesItOwned) {
  saveCounter();
  const std::int64_t baseline = countRows("pose_3d");

  ManipulableObject apple = objectAtCounter("apple", ObjectCategory::FRUITS);
  apple.setNavigablePose(poseAt(1.0f));
  apple.setGraspPose(poseAt(2.0f));
  repo_->saveObject(apple);
  ASSERT_EQ(countRows("pose_3d"), baseline + 2);

  repo_->deleteObject("apple");

  EXPECT_EQ(countRows("pose_3d"), baseline);
  EXPECT_EQ(countOrphanPoses(), 0);
}

// -- KnownLocation poses: no orphan rows -----------------------------

TEST_F(KnowledgeRepositoryTest,
       ReSavingKnownLocation_UpdatesPoseInPlace_NoOrphanPoses) {
  repo_->saveKnownLocation(kitchenCounter(poseAt(1.0f, 1.0f)));
  const std::int64_t posesAfterFirstSave = countRows("pose_3d");

  repo_->saveKnownLocation(kitchenCounter(poseAt(9.0f, 9.0f)));

  EXPECT_EQ(countRows("pose_3d"), posesAfterFirstSave);
  EXPECT_EQ(countOrphanPoses(), 0);

  auto loaded = repo_->loadKnownLocationByName("kitchen_counter");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_FLOAT_EQ(loaded->getPose().getX(), 9.0f);
}

// -- Person ------------------------------------------------------------

TEST_F(KnowledgeRepositoryTest, SavedPerson_RoundTripsFieldsAndOrder) {
  const KnownLocation counter = kitchenCounter();
  repo_->saveKnownLocation(counter);
  repo_->saveObject(objectAtCounter("apple", ObjectCategory::FRUITS));
  auto apple = repo_->loadObjectByName("apple");
  ASSERT_TRUE(apple.has_value());

  Person guest("Ana", PersonType::GUEST, counter);
  guest.setFavoriteDrink("coke");
  guest.setOrder({*apple});
  repo_->savePerson(guest);

  auto loaded = repo_->loadPersonByName("Ana");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->getFavoriteDrink(), "coke");
  ASSERT_EQ(loaded->getOrder().size(), 1u);
  EXPECT_EQ(loaded->getOrder()[0].getName(), "apple");
  EXPECT_EQ(loaded->getKnownLocation().getName(), "kitchen_counter");
  EXPECT_EQ(loaded->getKnownLocation().getZone(), LocationZone::KITCHEN);
}

TEST_F(KnowledgeRepositoryTest,
       OnlyOneUserAtATime_PreviousUserIsDemotedToGuest) {
  const KnownLocation counter = kitchenCounter();
  repo_->savePerson(Person("Bob", PersonType::USER, counter));
  repo_->savePerson(Person("Carla", PersonType::USER, counter));

  auto users = repo_->loadPersonsByType(PersonType::USER);
  ASSERT_EQ(users.size(), 1u);
  EXPECT_EQ(users[0].getName(), "Carla");

  auto bob = repo_->loadPersonByName("Bob");
  ASSERT_TRUE(bob.has_value());
  EXPECT_EQ(bob->getPersonType(), PersonType::GUEST);
}

TEST_F(KnowledgeRepositoryTest, DeletePerson_RemovesThem) {
  repo_->savePerson(Person("Temp", PersonType::GUEST, kitchenCounter()));
  repo_->deletePerson("Temp");
  EXPECT_EQ(repo_->loadPersonByName("Temp"), std::nullopt);
}

TEST_F(KnowledgeRepositoryTest,
       DeletePerson_ObjectsTheyHeldBecomeUnknown_AndAreNotDeleted) {
  repo_->savePerson(Person("Ana", PersonType::GUEST, kitchenCounter()));
  repo_->saveObject(ManipulableObject("backpack", ObjectCategory::CONTAINERS,
                                      LocationZone::BEDROOM,
                                      ObjectOwnership::heldByPerson("Ana")));

  repo_->deletePerson("Ana");

  auto backpack = repo_->loadObjectByName("backpack");
  ASSERT_TRUE(backpack.has_value());
  EXPECT_TRUE(backpack->getOwnership().isUnknown());
  // What the object is, and where it is usually found, is kept.
  EXPECT_EQ(backpack->getUsualZone(), LocationZone::BEDROOM);
}

// -- Person FIFO order -------------------------------------------------

TEST_F(KnowledgeRepositoryTest,
       LoadPersonsByType_ReturnsFirstRegistrationOrder_NotAlphabetical) {
  const KnownLocation counter = kitchenCounter();
  repo_->savePerson(Person("Zoe", PersonType::GUEST, counter));
  repo_->savePerson(Person("Ana", PersonType::GUEST, counter));
  repo_->savePerson(Person("Mia", PersonType::GUEST, counter));

  EXPECT_EQ(namesOf(repo_->loadPersonsByType(PersonType::GUEST)),
    (std::vector<std::string>{"Zoe", "Ana", "Mia"}));
  EXPECT_EQ(namesOf(repo_->loadPersonsByTypeWithPhoto(PersonType::GUEST)),
    (std::vector<std::string>{"Zoe", "Ana", "Mia"}));
}

TEST_F(KnowledgeRepositoryTest, ReSavingPerson_KeepsItsOriginalPosition) {
  const KnownLocation counter = kitchenCounter();
  repo_->savePerson(Person("Zoe", PersonType::GUEST, counter));
  Person ana("Ana", PersonType::GUEST, counter);
  repo_->savePerson(ana);
  repo_->savePerson(Person("Mia", PersonType::GUEST, counter));

  ana.setFavoriteDrink("tea");
  repo_->savePerson(ana);
  ana.setFavoriteDrink("juice");
  repo_->savePerson(ana);

  EXPECT_EQ(namesOf(repo_->loadPersonsByType(PersonType::GUEST)),
    (std::vector<std::string>{"Zoe", "Ana", "Mia"}));
}

TEST_F(KnowledgeRepositoryTest,
       DemotedUser_KeepsItsRegistrationPositionAmongGuests) {
  const KnownLocation counter = kitchenCounter();
  repo_->savePerson(Person("Bob", PersonType::USER, counter));
  repo_->savePerson(Person("Ana", PersonType::GUEST, counter));
  repo_->savePerson(Person("Carla", PersonType::GUEST, counter));

  // Dan becomes the USER; Bob is demoted to GUEST. Bob was registered
  // first, so he is the first guest, not the last.
  repo_->savePerson(Person("Dan", PersonType::USER, counter));

  EXPECT_EQ(namesOf(repo_->loadPersonsByType(PersonType::GUEST)),
    (std::vector<std::string>{"Bob", "Ana", "Carla"}));
}

// -- Person: no orphan rows --------------------------------------------

TEST_F(KnowledgeRepositoryTest,
       ReSavingPerson_UpdatesDescriptionAndPoseInPlace_NoOrphans) {
  PhysicalDescription description;
  description.setClothes("red shirt");

  Person ana("Ana", PersonType::GUEST, kitchenCounter(poseAt(1.0f, 1.0f)));
  ana.setPhysicalDescription(description);
  ana.setNavigablePose(poseAt(2.0f));
  repo_->savePerson(ana);

  const std::int64_t posesAfterFirstSave = countRows("pose_3d");
  ASSERT_EQ(countRows("person_description"), 1);

  for (int i = 1; i <= 5; ++i) {
    description.setClothes("shirt " + std::to_string(i));
    ana.setPhysicalDescription(description);
    ana.setNavigablePose(poseAt(10.0f + i));
    repo_->savePerson(ana);
  }

  EXPECT_EQ(countRows("person_description"), 1);
  EXPECT_EQ(countRows("pose_3d"), posesAfterFirstSave);
  EXPECT_EQ(countOrphanDescriptions(), 0);
  EXPECT_EQ(countOrphanPoses(), 0);

  auto loaded = repo_->loadPersonByName("Ana");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->getPhysicalDescription().getClothes(), "shirt 5");
  ASSERT_TRUE(loaded->getNavigablePose().has_value());
  EXPECT_FLOAT_EQ(loaded->getNavigablePose()->getX(), 15.0f);
}

TEST_F(KnowledgeRepositoryTest,
       DeletePerson_RemovesOwnedRows_ButKeepsTheSharedKnownLocation) {
  Person ana("Ana", PersonType::GUEST, kitchenCounter(poseAt(7.0f, 8.0f)));
  ana.setNavigablePose(poseAt(2.0f));
  repo_->savePerson(ana);

  repo_->deletePerson("Ana");

  EXPECT_EQ(countRows("person_description"), 0);
  EXPECT_EQ(countOrphanPoses(), 0);
  // Only the kitchen counter's own pose remains.
  EXPECT_EQ(countRows("pose_3d"), 1);

  auto counter = repo_->loadKnownLocationByName("kitchen_counter");
  ASSERT_TRUE(counter.has_value());
  EXPECT_FLOAT_EQ(counter->getPose().getX(), 7.0f);
}

// -- Person.photo ------------------------------------------------------

TEST_F(KnowledgeRepositoryTest,
       SavedPersonWithPhoto_LoadByNameWithPhoto_RoundTripsBytes) {
  std::vector<std::byte> photo{std::byte{0xDE}, std::byte{0xAD},
    std::byte{0xBE}, std::byte{0xEF}};

  Person ana("Ana", PersonType::GUEST, kitchenCounter());
  ana.setPhoto(photo);
  repo_->savePerson(ana);

  auto loaded = repo_->loadPersonByNameWithPhoto("Ana");
  ASSERT_TRUE(loaded.has_value());
  ASSERT_TRUE(loaded->getPhoto().has_value());
  EXPECT_EQ(*loaded->getPhoto(), photo);
}

TEST_F(KnowledgeRepositoryTest,
       SavedPersonWithoutPhoto_LoadByNameWithPhoto_ReturnsNullopt) {
  Person bob("Bob", PersonType::GUEST, kitchenCounter());
  repo_->savePerson(bob);

  auto loaded = repo_->loadPersonByNameWithPhoto("Bob");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_FALSE(loaded->getPhoto().has_value());
}

TEST_F(KnowledgeRepositoryTest,
       LoadPersonByName_Default_NeverReturnsPhotoEvenIfStored) {
  std::vector<std::byte> photo{std::byte{0x01}, std::byte{0x02}};

  Person carla("Carla", PersonType::GUEST, kitchenCounter());
  carla.setPhoto(photo);
  repo_->savePerson(carla);

  // The default retrieval must leave getPhoto() empty regardless of
  // whether a photo is actually stored -- callers that need to know
  // must use loadPersonByNameWithPhoto instead (see
  // KnowledgeRepository, contract 2, and Person::getPhoto()'s note on
  // "not loaded" vs. "doesn't exist" being indistinguishable here by
  // design).
  auto loaded = repo_->loadPersonByName("Carla");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_FALSE(loaded->getPhoto().has_value());
}

TEST_F(KnowledgeRepositoryTest,
       SavingPersonLoadedWithoutPhoto_DoesNotWipeStoredPhoto) {
  std::vector<std::byte> photo{std::byte{0xAA}, std::byte{0xBB}};

  Person dave("Dave", PersonType::GUEST, kitchenCounter());
  dave.setPhoto(photo);
  repo_->savePerson(dave);

  // Simulate a caller that only needed to update an unrelated field
  // (favorite drink): it loads the default (no-photo) form, changes
  // that field, and saves it back. getPhoto() is std::nullopt on this
  // in-memory Person -- not because the photo was deleted, but
  // because it was never loaded. savePerson must not interpret that
  // as "clear the photo" (see the COALESCE strategy documented on
  // KnowledgeRepository::savePerson / schema.sql's person.photo).
  auto loadedDefault = repo_->loadPersonByName("Dave");
  ASSERT_TRUE(loadedDefault.has_value());
  ASSERT_FALSE(loadedDefault->getPhoto().has_value());

  loadedDefault->setFavoriteDrink("orange juice");
  repo_->savePerson(*loadedDefault);

  auto loadedWithPhoto = repo_->loadPersonByNameWithPhoto("Dave");
  ASSERT_TRUE(loadedWithPhoto.has_value());
  EXPECT_EQ(loadedWithPhoto->getFavoriteDrink(), "orange juice");
  ASSERT_TRUE(loadedWithPhoto->getPhoto().has_value());
  EXPECT_EQ(*loadedWithPhoto->getPhoto(), photo);
}

TEST_F(KnowledgeRepositoryTest,
       LoadPersonsByTypeWithPhoto_ReturnsPhotosForMatchingType) {
  std::vector<std::byte> photo{std::byte{0x7A}};
  const KnownLocation counter = kitchenCounter();

  Person guestWithPhoto("Eve", PersonType::GUEST, counter);
  guestWithPhoto.setPhoto(photo);
  repo_->savePerson(guestWithPhoto);
  repo_->savePerson(Person("Frank", PersonType::GUEST, counter));

  auto defaultResults = repo_->loadPersonsByType(PersonType::GUEST);
  for (const auto & person : defaultResults) {
    EXPECT_FALSE(person.getPhoto().has_value());
  }

  auto withPhotoResults = repo_->loadPersonsByTypeWithPhoto(PersonType::GUEST);
  ASSERT_EQ(withPhotoResults.size(), 2u);
  for (const auto & person : withPhotoResults) {
    if (person.getName() == "Eve") {
      ASSERT_TRUE(person.getPhoto().has_value());
      EXPECT_EQ(*person.getPhoto(), photo);
    } else {
      EXPECT_FALSE(person.getPhoto().has_value());
    }
  }
}

// -- Robot --------------------------------------------------------------

TEST_F(KnowledgeRepositoryTest, SavedRobot_RoundTripsStateAndHeldObjects) {
  ManipulableObject spoon =
    objectHeldByRobot("spoon", ObjectCategory::CUTLERY, RobotHand::RIGHT);

  Robot robot("Justina", kitchenCounter());
  robot.setObjectInHand(RobotHand::RIGHT, spoon);
  repo_->saveRobot(robot);

  auto loaded = repo_->loadRobot();
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->getName(), "Justina");
  EXPECT_EQ(loaded->getKnownLocation().getName(), "kitchen_counter");

  auto rightHand = loaded->getObjectInHand(RobotHand::RIGHT);
  ASSERT_TRUE(rightHand.has_value());
  EXPECT_EQ(rightHand->getName(), "spoon");
  EXPECT_FALSE(loaded->getObjectInHand(RobotHand::LEFT).has_value());
}

TEST_F(KnowledgeRepositoryTest,
       SavedRobot_RoundTripsStartAndCurrentLocationSeparately) {
  Robot robot("Justina", sofa());
  robot.setKnownLocation(kitchenCounter());
  repo_->saveRobot(robot);

  auto loaded = repo_->loadRobot();
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->getStartLocation().getName(), "sofa");
  EXPECT_EQ(loaded->getStartLocation().getZone(), LocationZone::LIVING_ROOM);
  EXPECT_EQ(loaded->getKnownLocation().getName(), "kitchen_counter");
  EXPECT_EQ(loaded->getKnownLocation().getZone(), LocationZone::KITCHEN);
}

TEST_F(KnowledgeRepositoryTest, LoadRobot_NoRobotSavedYet_ReturnsNullopt) {
  EXPECT_EQ(repo_->loadRobot(), std::nullopt);
}

TEST_F(KnowledgeRepositoryTest, SavingRobotRepeatedly_DoesNotGrowPoseTable) {
  // saveRobot re-saves the robot's locations and every held object, so
  // repeated updateRobot calls used to insert new pose rows each time
  // even when nothing had changed.
  ManipulableObject spoon =
    objectHeldByRobot("spoon", ObjectCategory::CUTLERY, RobotHand::RIGHT);
  spoon.setNavigablePose(poseAt(1.0f));
  spoon.setGraspPose(poseAt(2.0f));

  Robot robot("Justina", kitchenCounter(poseAt(5.0f, 5.0f)));
  robot.setObjectInHand(RobotHand::RIGHT, spoon);
  repo_->saveRobot(robot);

  const std::int64_t posesAfterFirstSave = countRows("pose_3d");

  for (int i = 0; i < 10; ++i) {
    repo_->saveRobot(robot);
  }

  EXPECT_EQ(countRows("pose_3d"), posesAfterFirstSave);
  EXPECT_EQ(countOrphanPoses(), 0);
}

// -- Process restart / schema idempotency --------------------------

TEST_F(KnowledgeRepositoryTest, EnsureSchema_IsIdempotentAcrossRestarts) {
  const KnownLocation counter = kitchenCounter();
  Robot robot("Justina", counter);
  repo_->saveRobot(robot);
  repo_->savePerson(Person("Ana", PersonType::GUEST, counter));

  // Simulate a process restart: close this repository and open a new
  // one against the same database file. This must not fail (schema.sql
  // uses CREATE TABLE IF NOT EXISTS), and previously-saved data must
  // still be there.
  restart();

  auto loadedRobot = repo_->loadRobot();
  ASSERT_TRUE(loadedRobot.has_value());
  EXPECT_EQ(loadedRobot->getName(), "Justina");

  auto loadedAna = repo_->loadPersonByName("Ana");
  EXPECT_TRUE(loadedAna.has_value());
}

TEST_F(KnowledgeRepositoryTest, FifoOrder_SurvivesProcessRestart) {
  saveCounter();
  repo_->saveObject(objectAtCounter("pear", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtCounter("apple", ObjectCategory::FRUITS));

  restart();

  EXPECT_EQ(namesOf(repo_->loadObjectsByCategory(ObjectCategory::FRUITS)),
    (std::vector<std::string>{"pear", "apple"}));
}

// -- resetRuntimeState: knowledge stays, run state is forgotten -------

TEST_F(KnowledgeRepositoryTest, ResetRuntimeState_KeepsKnownLocations) {
  populateWorld();

  repo_->resetRuntimeState();

  EXPECT_EQ(sortedNamesOf(repo_->loadAllKnownLocations()),
    (std::vector<std::string>{"kitchen_counter", "sofa"}));

  auto counter = repo_->loadKnownLocationByName("kitchen_counter");
  ASSERT_TRUE(counter.has_value());
  EXPECT_EQ(counter->getZone(), LocationZone::KITCHEN);
  EXPECT_FLOAT_EQ(counter->getPose().getX(), 1.0f);
  EXPECT_FLOAT_EQ(counter->getPose().getY(), 2.0f);

  auto couch = repo_->loadKnownLocationByName("sofa");
  ASSERT_TRUE(couch.has_value());
  EXPECT_EQ(couch->getZone(), LocationZone::LIVING_ROOM);
  EXPECT_FLOAT_EQ(couch->getPose().getX(), 3.0f);
}

TEST_F(KnowledgeRepositoryTest,
       ResetRuntimeState_KeepsWhatObjectsAre_ButForgetsWhereTheyAre) {
  populateWorld();

  repo_->resetRuntimeState();

  struct Expected
  {
    std::string name;
    ObjectCategory category;
    LocationZone usualZone;
  };
  const std::vector<Expected> objects{
    {"apple", ObjectCategory::FRUITS, LocationZone::KITCHEN},
    {"towel", ObjectCategory::FABRIC, LocationZone::LAUNDRY},
    {"spoon", ObjectCategory::CUTLERY, LocationZone::KITCHEN}};

  for (const auto & expected : objects) {
    auto loaded = repo_->loadObjectByName(expected.name);
    ASSERT_TRUE(loaded.has_value()) << expected.name;
    EXPECT_EQ(loaded->getCategory(), expected.category) << expected.name;
    EXPECT_EQ(loaded->getUsualZone(), expected.usualZone) << expected.name;
    EXPECT_TRUE(loaded->getOwnership().isUnknown()) << expected.name;
    EXPECT_FALSE(loaded->getNavigablePose().has_value()) << expected.name;
    EXPECT_FALSE(loaded->getGraspPose().has_value()) << expected.name;
  }
  EXPECT_EQ(countObjectsWithUnknownOwnership(), 3);
}

TEST_F(KnowledgeRepositoryTest, ResetRuntimeState_LeavesNoOrphanRows) {
  populateWorld();

  repo_->resetRuntimeState();

  EXPECT_EQ(countOrphanPoses(), 0);
  EXPECT_EQ(countOrphanDescriptions(), 0);
  // Only the two known locations keep a pose.
  EXPECT_EQ(countRows("pose_3d"), 2);
  // Only the host keeps a description; Ana's order went with her.
  EXPECT_EQ(countRows("person_description"), 1);
  EXPECT_EQ(countRows("person_order"), 0);
  EXPECT_EQ(countRows("object"), 3);
}

TEST_F(KnowledgeRepositoryTest,
       ResetRuntimeState_SendsRobotBackToItsStartLocation_WithFreeHands) {
  populateWorld();
  ASSERT_EQ(repo_->loadRobot()->getKnownLocation().getName(),
            "kitchen_counter");
  ASSERT_TRUE(
      repo_->loadRobot()->getObjectInHand(RobotHand::RIGHT).has_value());

  repo_->resetRuntimeState();

  auto robot = repo_->loadRobot();
  ASSERT_TRUE(robot.has_value());
  EXPECT_EQ(robot->getName(), "Justina");
  EXPECT_EQ(robot->getStartLocation().getName(), "sofa");
  EXPECT_EQ(robot->getKnownLocation().getName(), "sofa");
  EXPECT_EQ(robot->getKnownLocation().getZone(), LocationZone::LIVING_ROOM);
  EXPECT_FALSE(robot->getObjectInHand(RobotHand::LEFT).has_value());
  EXPECT_FALSE(robot->getObjectInHand(RobotHand::RIGHT).has_value());
}

TEST_F(KnowledgeRepositoryTest, ResetRuntimeState_FreesTheHandsForNewObjects) {
  populateWorld();

  repo_->resetRuntimeState();

  EXPECT_NO_THROW(repo_->saveObject(
      objectHeldByRobot("fork", ObjectCategory::CUTLERY, RobotHand::RIGHT)));
}

TEST_F(KnowledgeRepositoryTest,
       ResetRuntimeState_DeletesNonHostPersons_KeepsHostWithoutItsPose) {
  populateWorld();

  repo_->resetRuntimeState();

  EXPECT_EQ(repo_->loadPersonByName("Ana"), std::nullopt);
  EXPECT_TRUE(repo_->loadPersonsByType(PersonType::GUEST).empty());

  auto host = repo_->loadPersonByNameWithPhoto("Host");
  ASSERT_TRUE(host.has_value());
  EXPECT_EQ(host->getPersonType(), PersonType::HOST);
  EXPECT_EQ(host->getPhysicalDescription().getClothes(), "blue jacket");
  EXPECT_EQ(host->getKnownLocation().getName(), "sofa");
  ASSERT_TRUE(host->getPhoto().has_value());
  EXPECT_EQ(*host->getPhoto(), kHostPhoto);
  // Where the host is right now is unknown at the start of a run.
  EXPECT_FALSE(host->getNavigablePose().has_value());
}

TEST_F(KnowledgeRepositoryTest,
       ResetRuntimeState_KeepsObjectRegistrationOrder) {
  saveCounter();
  repo_->saveObject(objectAtCounter("pear", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtCounter("apple", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtCounter("mango", ObjectCategory::FRUITS));

  repo_->resetRuntimeState();

  // Plans refer to objects as fruits_1, fruits_2, ...: the reset must
  // not change which object is which.
  EXPECT_EQ(namesOf(repo_->loadObjectsByCategory(ObjectCategory::FRUITS)),
    (std::vector<std::string>{"pear", "apple", "mango"}));
}

TEST_F(KnowledgeRepositoryTest, ResetRuntimeState_OnEmptyDatabase_DoesNothing) {
  EXPECT_NO_THROW(repo_->resetRuntimeState());

  EXPECT_EQ(repo_->loadRobot(), std::nullopt);
  EXPECT_EQ(countRows("object"), 0);
  EXPECT_EQ(countRows("person"), 0);
}

TEST_F(KnowledgeRepositoryTest, ResetRuntimeState_CalledTwice_GivesSameState) {
  populateWorld();

  repo_->resetRuntimeState();
  const std::int64_t poses = countRows("pose_3d");
  const std::int64_t descriptions = countRows("person_description");
  const std::int64_t persons = countRows("person");
  const std::int64_t objects = countRows("object");

  EXPECT_NO_THROW(repo_->resetRuntimeState());

  EXPECT_EQ(countRows("pose_3d"), poses);
  EXPECT_EQ(countRows("person_description"), descriptions);
  EXPECT_EQ(countRows("person"), persons);
  EXPECT_EQ(countRows("object"), objects);
  EXPECT_TRUE(repo_->loadPersonByName("Host").has_value());
}

TEST_F(KnowledgeRepositoryTest,
       ResetRuntimeState_AfterRestart_KnowledgeIsThere) {
  // The real startup flow: the process dies, a new one opens the same
  // database and resets the run state before serving anything.
  populateWorld();

  restart();
  repo_->resetRuntimeState();

  EXPECT_EQ(repo_->loadAllKnownLocations().size(), 2u);
  EXPECT_TRUE(repo_->loadObjectByName("apple").has_value());
  EXPECT_TRUE(repo_->loadPersonByName("Host").has_value());
  EXPECT_EQ(repo_->loadPersonByName("Ana"), std::nullopt);
  auto robot = repo_->loadRobot();
  ASSERT_TRUE(robot.has_value());
  EXPECT_EQ(robot->getKnownLocation().getName(), "sofa");
}
