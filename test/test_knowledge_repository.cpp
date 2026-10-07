#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <SQLiteCpp/SQLiteCpp.h>

#include "knowledge_manager/domain/known_location.hpp"
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

// Names of a list of objects/persons, in the order given. Used to
// assert FIFO order with a single readable EXPECT_EQ.
template<typename T>
std::vector<std::string> namesOf(const std::vector<T> & items)
{
  std::vector<std::string> names;
  for (const auto & item : items) {
    names.push_back(item.getName());
  }
  return names;
}

ManipulableObject objectAtKitchen(
  const std::string & name,
  ObjectCategory category)
{
  return ManipulableObject(name, category,
                           ObjectOwnership::atLocation("kitchen"));
}

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

  // Objects used in ownership "at location" need their location to
  // exist first (see SavedObjectAtLocation_RoundTripsOwnership).
  void saveKitchen()
  {
    repo_->saveKnownLocation(KnownLocation("kitchen", Pose3D()));
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
  Pose3D kitchenPose(1.0f, 2.0f, 0.0f, 0, 0, 0, 1);
  KnownLocation kitchen("kitchen", kitchenPose);
  repo_->saveKnownLocation(kitchen);

  auto loaded = repo_->loadKnownLocationByName("kitchen");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_FLOAT_EQ(loaded->getPose().getX(), 1.0f);
  EXPECT_FLOAT_EQ(loaded->getPose().getY(), 2.0f);
}

TEST_F(KnowledgeRepositoryTest,
       LoadKnownLocationByName_NotFound_ReturnsNullopt) {
  EXPECT_EQ(repo_->loadKnownLocationByName("nonexistent"), std::nullopt);
}

// -- ManipulableObject / ObjectOwnership exclusive-arc ---------------

TEST_F(KnowledgeRepositoryTest, SavedObjectAtLocation_RoundTripsOwnership) {
  KnownLocation kitchen("kitchen", Pose3D());
  repo_->saveKnownLocation(kitchen);

  ManipulableObject apple("apple", ObjectCategory::FRUITS,
    ObjectOwnership::atLocation("kitchen"));
  repo_->saveObject(apple);

  auto loaded = repo_->loadObjectByName("apple");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_TRUE(loaded->getOwnership().isAtLocation());
  EXPECT_EQ(loaded->getOwnership().getLocationName(), "kitchen");
  EXPECT_EQ(loaded->getCategory(), ObjectCategory::FRUITS);
}

TEST_F(KnowledgeRepositoryTest, SavedObjectHeldByRobot_RoundTripsOwnership) {
  ManipulableObject spoon("spoon", ObjectCategory::CUTLERY,
    ObjectOwnership::heldByRobot(RobotHand::RIGHT));
  repo_->saveObject(spoon);

  auto loaded = repo_->loadObjectByName("spoon");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_TRUE(loaded->getOwnership().isHeldByRobot());
  EXPECT_EQ(loaded->getOwnership().getRobotHand(), RobotHand::RIGHT);
}

TEST_F(KnowledgeRepositoryTest, SavedObjectHeldByPerson_RoundTripsOwnership) {
  Person ana("Ana", PersonType::GUEST, KnownLocation("kitchen", Pose3D()));
  repo_->savePerson(ana);

  ManipulableObject backpack("backpack", ObjectCategory::CONTAINERS,
    ObjectOwnership::heldByPerson("Ana"));
  repo_->saveObject(backpack);

  auto loaded = repo_->loadObjectByName("backpack");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_TRUE(loaded->getOwnership().isHeldByPerson());
  EXPECT_EQ(loaded->getOwnership().getPersonName(), "Ana");
}

TEST_F(KnowledgeRepositoryTest, LoadObjectsByCategory_ReturnsOnlyMatching) {
  KnownLocation kitchen("kitchen", Pose3D());
  repo_->saveKnownLocation(kitchen);
  repo_->saveObject(ManipulableObject("apple", ObjectCategory::FRUITS,
                                      ObjectOwnership::atLocation("kitchen")));
  repo_->saveObject(ManipulableObject("banana", ObjectCategory::FRUITS,
                                      ObjectOwnership::atLocation("kitchen")));
  repo_->saveObject(
      ManipulableObject("spoon", ObjectCategory::CUTLERY,
                        ObjectOwnership::heldByRobot(RobotHand::LEFT)));

  auto fruits = repo_->loadObjectsByCategory(ObjectCategory::FRUITS);
  EXPECT_EQ(fruits.size(), 2u);
}

TEST_F(KnowledgeRepositoryTest, DeleteObject_RemovesIt) {
  repo_->saveObject(
      ManipulableObject("banana", ObjectCategory::FRUITS,
                        ObjectOwnership::heldByRobot(RobotHand::LEFT)));
  repo_->deleteObject("banana");
  EXPECT_EQ(repo_->loadObjectByName("banana"), std::nullopt);
}

TEST_F(KnowledgeRepositoryTest,
       SaveObject_UnknownPersonOwner_ThrowsRepositoryException) {
  ManipulableObject ghostOwned(
    "napkin", ObjectCategory::TRASH,
    ObjectOwnership::heldByPerson("NobodyByThisName"));
  EXPECT_THROW(repo_->saveObject(ghostOwned), RepositoryException);
}

// -- Object FIFO order (fruits_1, fruits_2, ... aliases) -------------
//
// loadObjectsByCategory returns objects in order of first
// registration. WorldStateService::getObjectByCategoryAndPosition and
// the plans' positional aliases depend on this.

TEST_F(KnowledgeRepositoryTest,
       LoadObjectsByCategory_ReturnsFirstRegistrationOrder_NotAlphabetical) {
  saveKitchen();
  // Deliberately not alphabetical, so a name-ordered result would fail.
  repo_->saveObject(objectAtKitchen("pear", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtKitchen("apple", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtKitchen("mango", ObjectCategory::FRUITS));

  auto fruits = repo_->loadObjectsByCategory(ObjectCategory::FRUITS);
  EXPECT_EQ(namesOf(fruits),
    (std::vector<std::string>{"pear", "apple", "mango"}));
}

TEST_F(KnowledgeRepositoryTest, LoadObjectsByCategory_OrderIsPerCategory) {
  saveKitchen();
  repo_->saveObject(objectAtKitchen("pear", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtKitchen("fork", ObjectCategory::CUTLERY));
  repo_->saveObject(objectAtKitchen("apple", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtKitchen("knife", ObjectCategory::CUTLERY));

  EXPECT_EQ(namesOf(repo_->loadObjectsByCategory(ObjectCategory::FRUITS)),
    (std::vector<std::string>{"pear", "apple"}));
  EXPECT_EQ(namesOf(repo_->loadObjectsByCategory(ObjectCategory::CUTLERY)),
    (std::vector<std::string>{"fork", "knife"}));
}

TEST_F(KnowledgeRepositoryTest, ReSavingObject_KeepsItsOriginalPosition) {
  saveKitchen();
  ManipulableObject pear = objectAtKitchen("pear", ObjectCategory::FRUITS);
  repo_->saveObject(pear);
  repo_->saveObject(objectAtKitchen("apple", ObjectCategory::FRUITS));

  // Vision refines pear's pose: the same object is saved again. It
  // must not move to the end of the queue (saveObject is an upsert,
  // not delete-and-reinsert).
  pear.setNavigablePose(Pose3D(1.0f, 2.0f, 0.0f, 0, 0, 0, 1));
  repo_->saveObject(pear);
  pear.setNavigablePose(Pose3D(3.0f, 4.0f, 0.0f, 0, 0, 0, 1));
  repo_->saveObject(pear);

  auto fruits = repo_->loadObjectsByCategory(ObjectCategory::FRUITS);
  EXPECT_EQ(namesOf(fruits), (std::vector<std::string>{"pear", "apple"}));
}

TEST_F(KnowledgeRepositoryTest,
       ObjectPosition_IsIndependentOfCurrentOwnership) {
  saveKitchen();
  repo_->saveObject(objectAtKitchen("pear", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtKitchen("apple", ObjectCategory::FRUITS));

  // The robot picks up pear: it must still be fruits_1.
  repo_->saveObject(
      ManipulableObject("pear", ObjectCategory::FRUITS,
                        ObjectOwnership::heldByRobot(RobotHand::LEFT)));

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
  saveKitchen();
  repo_->saveObject(objectAtKitchen("pear", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtKitchen("apple", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtKitchen("mango", ObjectCategory::FRUITS));

  repo_->deleteObject("pear");

  EXPECT_EQ(namesOf(repo_->loadObjectsByCategory(ObjectCategory::FRUITS)),
    (std::vector<std::string>{"apple", "mango"}));

  // A newly registered object goes to the end of the queue.
  repo_->saveObject(objectAtKitchen("kiwi", ObjectCategory::FRUITS));
  EXPECT_EQ(namesOf(repo_->loadObjectsByCategory(ObjectCategory::FRUITS)),
    (std::vector<std::string>{"apple", "mango", "kiwi"}));
}

// -- Object poses: no orphan rows ------------------------------------

TEST_F(KnowledgeRepositoryTest,
       ReSavingObjectWithNewPoses_UpdatesInPlace_NoOrphanPoses) {
  saveKitchen();
  ManipulableObject apple = objectAtKitchen("apple", ObjectCategory::FRUITS);
  apple.setNavigablePose(Pose3D(1.0f, 0.0f, 0.0f, 0, 0, 0, 1));
  apple.setGraspPose(Pose3D(2.0f, 0.0f, 0.0f, 0, 0, 0, 1));
  repo_->saveObject(apple);

  const std::int64_t posesAfterFirstSave = countRows("pose_3d");

  for (int i = 1; i <= 5; ++i) {
    apple.setNavigablePose(Pose3D(10.0f + i, 0.0f, 0.0f, 0, 0, 0, 1));
    apple.setGraspPose(Pose3D(20.0f + i, 0.0f, 0.0f, 0, 0, 0, 1));
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
  saveKitchen();
  ManipulableObject apple = objectAtKitchen("apple", ObjectCategory::FRUITS);
  apple.setNavigablePose(Pose3D(7.0f, 0.0f, 0.0f, 0, 0, 0, 1));
  repo_->saveObject(apple);

  // Saved again by someone who never loaded the pose (e.g. an
  // ownership change): the stored pose must not be wiped.
  repo_->saveObject(objectAtKitchen("apple", ObjectCategory::FRUITS));

  auto loaded = repo_->loadObjectByName("apple");
  ASSERT_TRUE(loaded.has_value());
  ASSERT_TRUE(loaded->getNavigablePose().has_value());
  EXPECT_FLOAT_EQ(loaded->getNavigablePose()->getX(), 7.0f);
  EXPECT_EQ(countOrphanPoses(), 0);
}

TEST_F(KnowledgeRepositoryTest, DeleteObject_RemovesThePosesItOwned) {
  saveKitchen();
  const std::int64_t baseline = countRows("pose_3d");

  ManipulableObject apple = objectAtKitchen("apple", ObjectCategory::FRUITS);
  apple.setNavigablePose(Pose3D(1.0f, 0.0f, 0.0f, 0, 0, 0, 1));
  apple.setGraspPose(Pose3D(2.0f, 0.0f, 0.0f, 0, 0, 0, 1));
  repo_->saveObject(apple);
  ASSERT_EQ(countRows("pose_3d"), baseline + 2);

  repo_->deleteObject("apple");

  EXPECT_EQ(countRows("pose_3d"), baseline);
  EXPECT_EQ(countOrphanPoses(), 0);
}

// -- KnownLocation poses: no orphan rows -----------------------------

TEST_F(KnowledgeRepositoryTest,
       ReSavingKnownLocation_UpdatesPoseInPlace_NoOrphanPoses) {
  repo_->saveKnownLocation(
      KnownLocation("kitchen", Pose3D(1.0f, 1.0f, 0.0f, 0, 0, 0, 1)));
  const std::int64_t posesAfterFirstSave = countRows("pose_3d");

  repo_->saveKnownLocation(
      KnownLocation("kitchen", Pose3D(9.0f, 9.0f, 0.0f, 0, 0, 0, 1)));

  EXPECT_EQ(countRows("pose_3d"), posesAfterFirstSave);
  EXPECT_EQ(countOrphanPoses(), 0);

  auto loaded = repo_->loadKnownLocationByName("kitchen");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_FLOAT_EQ(loaded->getPose().getX(), 9.0f);
}

// -- Person ------------------------------------------------------------

TEST_F(KnowledgeRepositoryTest, SavedPerson_RoundTripsFieldsAndOrder) {
  KnownLocation kitchen("kitchen", Pose3D());
  repo_->saveKnownLocation(kitchen);
  repo_->saveObject(ManipulableObject("apple", ObjectCategory::FRUITS,
                                      ObjectOwnership::atLocation("kitchen")));
  auto apple = repo_->loadObjectByName("apple");
  ASSERT_TRUE(apple.has_value());

  Person guest("Ana", PersonType::GUEST, kitchen);
  guest.setFavoriteDrink("coke");
  guest.setOrder({*apple});
  repo_->savePerson(guest);

  auto loaded = repo_->loadPersonByName("Ana");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->getFavoriteDrink(), "coke");
  ASSERT_EQ(loaded->getOrder().size(), 1u);
  EXPECT_EQ(loaded->getOrder()[0].getName(), "apple");
}

TEST_F(KnowledgeRepositoryTest,
       OnlyOneUserAtATime_PreviousUserIsDemotedToGuest) {
  KnownLocation kitchen("kitchen", Pose3D());
  repo_->savePerson(Person("Bob", PersonType::USER, kitchen));
  repo_->savePerson(Person("Carla", PersonType::USER, kitchen));

  auto users = repo_->loadPersonsByType(PersonType::USER);
  ASSERT_EQ(users.size(), 1u);
  EXPECT_EQ(users[0].getName(), "Carla");

  auto bob = repo_->loadPersonByName("Bob");
  ASSERT_TRUE(bob.has_value());
  EXPECT_EQ(bob->getPersonType(), PersonType::GUEST);
}

TEST_F(KnowledgeRepositoryTest, DeletePerson_RemovesThem) {
  repo_->savePerson(
      Person("Temp", PersonType::GUEST, KnownLocation("kitchen", Pose3D())));
  repo_->deletePerson("Temp");
  EXPECT_EQ(repo_->loadPersonByName("Temp"), std::nullopt);
}

// -- Person FIFO order -------------------------------------------------

TEST_F(KnowledgeRepositoryTest,
       LoadPersonsByType_ReturnsFirstRegistrationOrder_NotAlphabetical) {
  KnownLocation kitchen("kitchen", Pose3D());
  repo_->savePerson(Person("Zoe", PersonType::GUEST, kitchen));
  repo_->savePerson(Person("Ana", PersonType::GUEST, kitchen));
  repo_->savePerson(Person("Mia", PersonType::GUEST, kitchen));

  EXPECT_EQ(namesOf(repo_->loadPersonsByType(PersonType::GUEST)),
    (std::vector<std::string>{"Zoe", "Ana", "Mia"}));
  EXPECT_EQ(namesOf(repo_->loadPersonsByTypeWithPhoto(PersonType::GUEST)),
    (std::vector<std::string>{"Zoe", "Ana", "Mia"}));
}

TEST_F(KnowledgeRepositoryTest, ReSavingPerson_KeepsItsOriginalPosition) {
  KnownLocation kitchen("kitchen", Pose3D());
  repo_->savePerson(Person("Zoe", PersonType::GUEST, kitchen));
  Person ana("Ana", PersonType::GUEST, kitchen);
  repo_->savePerson(ana);
  repo_->savePerson(Person("Mia", PersonType::GUEST, kitchen));

  ana.setFavoriteDrink("tea");
  repo_->savePerson(ana);
  ana.setFavoriteDrink("juice");
  repo_->savePerson(ana);

  EXPECT_EQ(namesOf(repo_->loadPersonsByType(PersonType::GUEST)),
    (std::vector<std::string>{"Zoe", "Ana", "Mia"}));
}

TEST_F(KnowledgeRepositoryTest,
       DemotedUser_KeepsItsRegistrationPositionAmongGuests) {
  KnownLocation kitchen("kitchen", Pose3D());
  repo_->savePerson(Person("Bob", PersonType::USER, kitchen));
  repo_->savePerson(Person("Ana", PersonType::GUEST, kitchen));
  repo_->savePerson(Person("Carla", PersonType::GUEST, kitchen));

  // Dan becomes the USER; Bob is demoted to GUEST. Bob was registered
  // first, so he is the first guest, not the last.
  repo_->savePerson(Person("Dan", PersonType::USER, kitchen));

  EXPECT_EQ(namesOf(repo_->loadPersonsByType(PersonType::GUEST)),
    (std::vector<std::string>{"Bob", "Ana", "Carla"}));
}

// -- Person: no orphan rows --------------------------------------------

TEST_F(KnowledgeRepositoryTest,
       ReSavingPerson_UpdatesDescriptionAndPoseInPlace_NoOrphans) {
  PhysicalDescription description;
  description.setClothes("red shirt");

  Person ana("Ana", PersonType::GUEST,
    KnownLocation("kitchen", Pose3D(1.0f, 1.0f, 0.0f, 0, 0, 0, 1)));
  ana.setPhysicalDescription(description);
  ana.setNavigablePose(Pose3D(2.0f, 0.0f, 0.0f, 0, 0, 0, 1));
  repo_->savePerson(ana);

  const std::int64_t posesAfterFirstSave = countRows("pose_3d");
  ASSERT_EQ(countRows("person_description"), 1);

  for (int i = 1; i <= 5; ++i) {
    description.setClothes("shirt " + std::to_string(i));
    ana.setPhysicalDescription(description);
    ana.setNavigablePose(Pose3D(10.0f + i, 0.0f, 0.0f, 0, 0, 0, 1));
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
  Person ana("Ana", PersonType::GUEST,
    KnownLocation("kitchen", Pose3D(7.0f, 8.0f, 0.0f, 0, 0, 0, 1)));
  ana.setNavigablePose(Pose3D(2.0f, 0.0f, 0.0f, 0, 0, 0, 1));
  repo_->savePerson(ana);

  repo_->deletePerson("Ana");

  EXPECT_EQ(countRows("person_description"), 0);
  EXPECT_EQ(countOrphanPoses(), 0);
  // Only the kitchen's own pose remains.
  EXPECT_EQ(countRows("pose_3d"), 1);

  auto kitchen = repo_->loadKnownLocationByName("kitchen");
  ASSERT_TRUE(kitchen.has_value());
  EXPECT_FLOAT_EQ(kitchen->getPose().getX(), 7.0f);
}

// -- Person.photo ------------------------------------------------------

TEST_F(KnowledgeRepositoryTest,
       SavedPersonWithPhoto_LoadByNameWithPhoto_RoundTripsBytes) {
  std::vector<std::byte> photo{std::byte{0xDE}, std::byte{0xAD},
    std::byte{0xBE}, std::byte{0xEF}};

  Person ana("Ana", PersonType::GUEST, KnownLocation("kitchen", Pose3D()));
  ana.setPhoto(photo);
  repo_->savePerson(ana);

  auto loaded = repo_->loadPersonByNameWithPhoto("Ana");
  ASSERT_TRUE(loaded.has_value());
  ASSERT_TRUE(loaded->getPhoto().has_value());
  EXPECT_EQ(*loaded->getPhoto(), photo);
}

TEST_F(KnowledgeRepositoryTest,
       SavedPersonWithoutPhoto_LoadByNameWithPhoto_ReturnsNullopt) {
  Person bob("Bob", PersonType::GUEST, KnownLocation("kitchen", Pose3D()));
  repo_->savePerson(bob);

  auto loaded = repo_->loadPersonByNameWithPhoto("Bob");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_FALSE(loaded->getPhoto().has_value());
}

TEST_F(KnowledgeRepositoryTest,
       LoadPersonByName_Default_NeverReturnsPhotoEvenIfStored) {
  std::vector<std::byte> photo{std::byte{0x01}, std::byte{0x02}};

  Person carla("Carla", PersonType::GUEST, KnownLocation("kitchen", Pose3D()));
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

  Person dave("Dave", PersonType::GUEST, KnownLocation("kitchen", Pose3D()));
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
  KnownLocation kitchen("kitchen", Pose3D());

  Person guestWithPhoto("Eve", PersonType::GUEST, kitchen);
  guestWithPhoto.setPhoto(photo);
  repo_->savePerson(guestWithPhoto);
  repo_->savePerson(Person("Frank", PersonType::GUEST, kitchen));

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
  KnownLocation kitchen("kitchen", Pose3D());
  ManipulableObject spoon("spoon", ObjectCategory::CUTLERY,
    ObjectOwnership::heldByRobot(RobotHand::RIGHT));

  Robot robot("Justina", kitchen);
  robot.setObjectInHand(RobotHand::RIGHT, spoon);
  repo_->saveRobot(robot);

  Robot loaded = repo_->loadRobot();
  EXPECT_EQ(loaded.getName(), "Justina");
  EXPECT_EQ(loaded.getKnownLocation().getName(), "kitchen");

  auto rightHand = loaded.getObjectInHand(RobotHand::RIGHT);
  ASSERT_TRUE(rightHand.has_value());
  EXPECT_EQ(rightHand->getName(), "spoon");
  EXPECT_FALSE(loaded.getObjectInHand(RobotHand::LEFT).has_value());
}

TEST_F(KnowledgeRepositoryTest, LoadRobot_NoRobotSavedYet_ReturnsDefaultRobot) {
  Robot loaded = repo_->loadRobot();
  EXPECT_EQ(loaded.getName(), "");
}

TEST_F(KnowledgeRepositoryTest, SavingRobotRepeatedly_DoesNotGrowPoseTable) {
  // saveRobot re-saves the robot's location and every held object, so
  // repeated updateRobot calls used to insert new pose rows each time
  // even when nothing had changed.
  ManipulableObject spoon("spoon", ObjectCategory::CUTLERY,
    ObjectOwnership::heldByRobot(RobotHand::RIGHT));
  spoon.setNavigablePose(Pose3D(1.0f, 0.0f, 0.0f, 0, 0, 0, 1));
  spoon.setGraspPose(Pose3D(2.0f, 0.0f, 0.0f, 0, 0, 0, 1));

  Robot robot("Justina",
    KnownLocation("kitchen", Pose3D(5.0f, 5.0f, 0.0f, 0, 0, 0, 1)));
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
  KnownLocation kitchen("kitchen", Pose3D());
  Robot robot("Justina", kitchen);
  repo_->saveRobot(robot);
  repo_->savePerson(Person("Ana", PersonType::GUEST, kitchen));

  // Simulate a process restart: close this repository and open a new
  // one against the same database file. This must not fail (schema.sql
  // uses CREATE TABLE IF NOT EXISTS), and previously-saved data must
  // still be there.
  repo_.reset();
  KnowledgeRepository restarted(dbPath_, schemaPath_, logger_);

  Robot loadedRobot = restarted.loadRobot();
  EXPECT_EQ(loadedRobot.getName(), "Justina");

  auto loadedAna = restarted.loadPersonByName("Ana");
  EXPECT_TRUE(loadedAna.has_value());
}

TEST_F(KnowledgeRepositoryTest, FifoOrder_SurvivesProcessRestart) {
  saveKitchen();
  repo_->saveObject(objectAtKitchen("pear", ObjectCategory::FRUITS));
  repo_->saveObject(objectAtKitchen("apple", ObjectCategory::FRUITS));

  repo_.reset();
  KnowledgeRepository restarted(dbPath_, schemaPath_, logger_);

  EXPECT_EQ(namesOf(restarted.loadObjectsByCategory(ObjectCategory::FRUITS)),
    (std::vector<std::string>{"pear", "apple"}));
}
