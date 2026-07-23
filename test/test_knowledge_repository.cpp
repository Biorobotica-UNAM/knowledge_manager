#include <cstdio>

#include <gtest/gtest.h>

#include "knowledge_manager/domain/known_location.hpp"
#include "knowledge_manager/domain/manipulable_object.hpp"
#include "knowledge_manager/domain/object_ownership.hpp"
#include "knowledge_manager/domain/person.hpp"
#include "knowledge_manager/domain/robot.hpp"
#include "knowledge_manager/persistence/knowledge_repository.hpp"
#include "knowledge_manager/persistence/logger.hpp"
#include "knowledge_manager/persistence/repository_exception.hpp"

using namespace knowledge_manager::domain;
using namespace knowledge_manager::persistence;

namespace {

// Minimal Logger for tests: writes to stdout/stderr like the
// production ConsoleLogger, kept local to this test file so the test
// suite does not depend on a specific logger implementation living
// elsewhere in the codebase.
class TestLogger : public Logger {
public:
  void error(const std::string &message) override {
    std::fprintf(stderr, "[ERROR] %s\n", message.c_str());
  }
  void warn(const std::string &message) override {
    std::fprintf(stderr, "[WARN] %s\n", message.c_str());
  }
  void info(const std::string &message) override {
    std::fprintf(stdout, "[INFO] %s\n", message.c_str());
  }
};

// gtest fixture: gives every TEST_F below a fresh database file and a
// fresh KnowledgeRepository, and cleans up afterward. This avoids
// repeating the same setup/teardown code in each test (Clean Code:
// don't repeat yourself), and keeps tests independent from each other
// -- one test's data never leaks into the next.
class KnowledgeRepositoryTest : public ::testing::Test {
protected:
  void SetUp() override {
    dbPath_ = "/tmp/kb_gtest.db";
    std::remove(dbPath_.c_str());
    repo_ =
        std::make_unique<KnowledgeRepository>(dbPath_, schemaPath_, logger_);
  }

  void TearDown() override {
    repo_.reset(); // close the connection before removing the file
    std::remove(dbPath_.c_str());
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

  ManipulableObject apple("apple", "fruit",
                          ObjectOwnership::atLocation("kitchen"));
  repo_->saveObject(apple);

  auto loaded = repo_->loadObjectByName("apple");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_TRUE(loaded->getOwnership().isAtLocation());
  EXPECT_EQ(loaded->getOwnership().getLocationName(), "kitchen");
  EXPECT_EQ(loaded->getCategory(), "fruit");
}

TEST_F(KnowledgeRepositoryTest, SavedObjectHeldByRobot_RoundTripsOwnership) {
  ManipulableObject spoon("spoon", "cutlery",
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

  ManipulableObject book("book", "misc", ObjectOwnership::heldByPerson("Ana"));
  repo_->saveObject(book);

  auto loaded = repo_->loadObjectByName("book");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_TRUE(loaded->getOwnership().isHeldByPerson());
  EXPECT_EQ(loaded->getOwnership().getPersonName(), "Ana");
}

TEST_F(KnowledgeRepositoryTest, LoadObjectsByCategory_ReturnsOnlyMatching) {
  KnownLocation kitchen("kitchen", Pose3D());
  repo_->saveKnownLocation(kitchen);
  repo_->saveObject(ManipulableObject("apple", "fruit",
                                      ObjectOwnership::atLocation("kitchen")));
  repo_->saveObject(ManipulableObject("banana", "fruit",
                                      ObjectOwnership::atLocation("kitchen")));
  repo_->saveObject(ManipulableObject(
      "spoon", "cutlery", ObjectOwnership::heldByRobot(RobotHand::LEFT)));

  auto fruits = repo_->loadObjectsByCategory("fruit");
  EXPECT_EQ(fruits.size(), 2u);
}

TEST_F(KnowledgeRepositoryTest, DeleteObject_RemovesIt) {
  repo_->saveObject(ManipulableObject(
      "banana", "fruit", ObjectOwnership::heldByRobot(RobotHand::LEFT)));
  repo_->deleteObject("banana");
  EXPECT_EQ(repo_->loadObjectByName("banana"), std::nullopt);
}

TEST_F(KnowledgeRepositoryTest,
       SaveObject_UnknownPersonOwner_ThrowsRepositoryException) {
  ManipulableObject ghostOwned(
      "ghost_item", "misc", ObjectOwnership::heldByPerson("NobodyByThisName"));
  EXPECT_THROW(repo_->saveObject(ghostOwned), RepositoryException);
}

// -- Person -----------------------------------------------------------

TEST_F(KnowledgeRepositoryTest, SavedPerson_RoundTripsFieldsAndOrder) {
  KnownLocation kitchen("kitchen", Pose3D());
  repo_->saveKnownLocation(kitchen);
  repo_->saveObject(ManipulableObject("apple", "fruit",
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

// -- Robot --------------------------------------------------------------

TEST_F(KnowledgeRepositoryTest, SavedRobot_RoundTripsStateAndHeldObjects) {
  KnownLocation kitchen("kitchen", Pose3D());
  ManipulableObject spoon("spoon", "cutlery",
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
