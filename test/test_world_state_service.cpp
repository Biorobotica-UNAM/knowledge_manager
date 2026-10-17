#include <cstdio>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "knowledge_manager/application/world_state_service.hpp"
#include "knowledge_manager/domain/known_location.hpp"
#include "knowledge_manager/domain/location_zone.hpp"
#include "knowledge_manager/domain/manipulable_object.hpp"
#include "knowledge_manager/domain/object_ownership.hpp"
#include "knowledge_manager/domain/person.hpp"
#include "knowledge_manager/domain/robot.hpp"
#include "knowledge_manager/persistence/knowledge_repository.hpp"
#include "knowledge_manager/persistence/logger.hpp"

using namespace knowledge_manager::domain;
using namespace knowledge_manager::persistence;
using namespace knowledge_manager::application;

namespace
{

class SilentLogger : public Logger {
public:
  void error(const std::string &) override {}
  void warn(const std::string &) override {}
  void info(const std::string &) override {}
};

KnownLocation counter()
{
  return KnownLocation("kitchen_counter", Pose3D(), LocationZone::KITCHEN);
}

ManipulableObject fruitAtCounter(const std::string & name)
{
  return ManipulableObject(name, ObjectCategory::FRUITS, LocationZone::KITCHEN,
                           ObjectOwnership::atLocation("kitchen_counter"));
}

class WorldStateServiceTest : public ::testing::Test {
protected:
  void SetUp() override
  {
    dbPath_ = "/tmp/wss_gtest.db";
    std::remove(dbPath_.c_str());
    repo_ = std::make_unique<KnowledgeRepository>(
        dbPath_, KNOWLEDGE_MANAGER_SCHEMA_PATH, logger_);
    service_ = std::make_unique<WorldStateService>(*repo_);
  }
  void TearDown() override
  {
    service_.reset();
    repo_.reset();
    std::remove(dbPath_.c_str());
  }

  std::string dbPath_;
  SilentLogger logger_;
  std::unique_ptr<KnowledgeRepository> repo_;
  std::unique_ptr<WorldStateService> service_;
};

} // namespace

TEST_F(WorldStateServiceTest, GetRobot_NoneSaved_Fails) {
  auto result = service_->getRobot();
  EXPECT_TRUE(result.isFailure());
  EXPECT_FALSE(result.error().empty());
}

TEST_F(WorldStateServiceTest, UpdateRobot_ThenGetRobot_Succeeds) {
  ASSERT_TRUE(service_->updateRobot(Robot("Justina", counter())).isSuccess());
  auto result = service_->getRobot();
  ASSERT_TRUE(result.isSuccess());
  EXPECT_EQ(result.value().getName(), "Justina");
}

TEST_F(WorldStateServiceTest, GetObjectByName_Unknown_FailsWithName) {
  auto result = service_->getObjectByName("ghost");
  ASSERT_TRUE(result.isFailure());
  EXPECT_NE(result.error().find("ghost"), std::string::npos);
}

TEST_F(WorldStateServiceTest,
       CreateObject_AtMissingLocation_FailsAndStoresNothing) {
  auto result = service_->createOrUpdateObject(fruitAtCounter("apple"));
  ASSERT_TRUE(result.isFailure());
  EXPECT_NE(result.error().find("kitchen_counter"), std::string::npos);
  EXPECT_FALSE(repo_->loadObjectByName("apple").has_value());
}

TEST_F(WorldStateServiceTest, CreateObject_HeldByMissingPerson_Fails) {
  auto result = service_->createOrUpdateObject(ManipulableObject(
      "bag", ObjectCategory::CONTAINERS, LocationZone::BEDROOM,
      ObjectOwnership::heldByPerson("Nobody")));
  EXPECT_TRUE(result.isFailure());
}

TEST_F(WorldStateServiceTest, CreateObject_UnknownOwnership_NeedsNothingElse) {
  auto result = service_->createOrUpdateObject(
      ManipulableObject("apple", ObjectCategory::FRUITS, LocationZone::KITCHEN,
                        ObjectOwnership::unknown()));
  EXPECT_TRUE(result.isSuccess());
}

TEST_F(WorldStateServiceTest,
       InfrastructureFailure_BecomesFailedResult_NotException) {
  // Two objects in the same hand: the unique index makes the
  // repository throw; the service must report it as a Result.
  ASSERT_TRUE(service_
    ->createOrUpdateObject(ManipulableObject(
                      "pear", ObjectCategory::FRUITS, LocationZone::KITCHEN,
                      ObjectOwnership::heldByRobot(RobotHand::LEFT)))
    .isSuccess());

  Result<void> result = Result<void>::success();
  EXPECT_NO_THROW(result = service_->createOrUpdateObject(ManipulableObject(
                      "kiwi", ObjectCategory::FRUITS, LocationZone::KITCHEN,
                      ObjectOwnership::heldByRobot(RobotHand::LEFT))));
  EXPECT_TRUE(result.isFailure());
  EXPECT_FALSE(result.error().empty());
}

TEST_F(WorldStateServiceTest, ObjectByPosition_IsOneBasedAndFifo) {
  ASSERT_TRUE(service_->createOrUpdateKnownLocation(counter()).isSuccess());
  ASSERT_TRUE(
      service_->createOrUpdateObject(fruitAtCounter("pear")).isSuccess());
  ASSERT_TRUE(
      service_->createOrUpdateObject(fruitAtCounter("apple")).isSuccess());

  auto first =
    service_->getObjectByCategoryAndPosition(ObjectCategory::FRUITS, 1);
  auto second =
    service_->getObjectByCategoryAndPosition(ObjectCategory::FRUITS, 2);
  ASSERT_TRUE(first.isSuccess());
  ASSERT_TRUE(second.isSuccess());
  EXPECT_EQ(first.value().getName(), "pear");
  EXPECT_EQ(second.value().getName(), "apple");
}

TEST_F(WorldStateServiceTest, ObjectByPosition_OutOfRange_Fails) {
  ASSERT_TRUE(service_->createOrUpdateKnownLocation(counter()).isSuccess());
  ASSERT_TRUE(
      service_->createOrUpdateObject(fruitAtCounter("pear")).isSuccess());

  EXPECT_TRUE(
      service_->getObjectByCategoryAndPosition(ObjectCategory::FRUITS, 0)
    .isFailure());
  EXPECT_TRUE(
      service_->getObjectByCategoryAndPosition(ObjectCategory::FRUITS, -3)
    .isFailure());
  EXPECT_TRUE(
      service_->getObjectByCategoryAndPosition(ObjectCategory::FRUITS, 2)
    .isFailure());
  EXPECT_TRUE(
      service_->getObjectByCategoryAndPosition(ObjectCategory::CUTLERY, 1)
    .isFailure());
}

TEST_F(WorldStateServiceTest, DeleteObject_Unknown_FailsAndExistingSucceeds) {
  EXPECT_TRUE(service_->deleteObject("ghost").isFailure());

  ASSERT_TRUE(service_
    ->createOrUpdateObject(ManipulableObject(
                      "apple", ObjectCategory::FRUITS, LocationZone::KITCHEN,
                      ObjectOwnership::unknown()))
    .isSuccess());
  EXPECT_TRUE(service_->deleteObject("apple").isSuccess());
  EXPECT_TRUE(service_->getObjectByName("apple").isFailure());
}

TEST_F(WorldStateServiceTest, Persons_ByNameTypeAndPosition) {
  ASSERT_TRUE(
      service_
    ->createOrUpdatePerson(Person("Zoe", PersonType::GUEST, counter()))
    .isSuccess());
  ASSERT_TRUE(
      service_
    ->createOrUpdatePerson(Person("Ana", PersonType::GUEST, counter()))
    .isSuccess());

  EXPECT_TRUE(service_->getPersonByName("Zoe").isSuccess());
  EXPECT_TRUE(service_->getPersonByName("Nobody").isFailure());
  EXPECT_EQ(service_->getPersonsByType(PersonType::GUEST).value().size(), 2u);
  EXPECT_EQ(service_->getPersonByTypeAndPosition(PersonType::GUEST, 2)
    .value()
    .getName(),
            "Ana");
  EXPECT_TRUE(
      service_->getPersonByTypeAndPosition(PersonType::GUEST, 3).isFailure());
  EXPECT_EQ(service_->getPersonByTypeAndPositionWithPhoto(PersonType::GUEST, 1)
    .value()
    .getName(),
            "Zoe");
}

TEST_F(WorldStateServiceTest, OnlyOneUser_IsEnforcedThroughTheService) {
  ASSERT_TRUE(
      service_->createOrUpdatePerson(Person("Bob", PersonType::USER, counter()))
    .isSuccess());
  ASSERT_TRUE(
      service_
    ->createOrUpdatePerson(Person("Carla", PersonType::USER, counter()))
    .isSuccess());

  auto users = service_->getPersonsByType(PersonType::USER);
  ASSERT_TRUE(users.isSuccess());
  ASSERT_EQ(users.value().size(), 1u);
  EXPECT_EQ(users.value()[0].getName(), "Carla");
}

TEST_F(WorldStateServiceTest, DeletePerson_Unknown_Fails) {
  EXPECT_TRUE(service_->deletePerson("Nobody").isFailure());
  ASSERT_TRUE(
      service_
    ->createOrUpdatePerson(Person("Ana", PersonType::GUEST, counter()))
    .isSuccess());
  EXPECT_TRUE(service_->deletePerson("Ana").isSuccess());
}

TEST_F(WorldStateServiceTest, KnownLocations_ByNameAllAndZone) {
  ASSERT_TRUE(service_->createOrUpdateKnownLocation(counter()).isSuccess());
  ASSERT_TRUE(service_
    ->createOrUpdateKnownLocation(KnownLocation(
                      "sofa", Pose3D(), LocationZone::LIVING_ROOM))
    .isSuccess());

  EXPECT_EQ(service_->getKnownLocationByName("sofa").value().getZone(),
            LocationZone::LIVING_ROOM);
  EXPECT_TRUE(service_->getKnownLocationByName("nowhere").isFailure());
  EXPECT_EQ(service_->getAllKnownLocations().value().size(), 2u);
  EXPECT_EQ(
      service_->getKnownLocationsByZone(LocationZone::KITCHEN).value().size(),
      1u);
}

TEST_F(WorldStateServiceTest,
       KnownLocationsByZone_EmptyZone_IsSuccessNotError) {
  auto result = service_->getKnownLocationsByZone(LocationZone::LAUNDRY);
  ASSERT_TRUE(result.isSuccess());
  EXPECT_TRUE(result.value().empty());
}

TEST_F(WorldStateServiceTest,
       ResetRuntimeState_ClearsRunStateAndReportsSuccess) {
  ASSERT_TRUE(service_->createOrUpdateKnownLocation(counter()).isSuccess());
  ASSERT_TRUE(
      service_->createOrUpdateObject(fruitAtCounter("apple")).isSuccess());
  ASSERT_TRUE(
      service_
    ->createOrUpdatePerson(Person("Ana", PersonType::GUEST, counter()))
    .isSuccess());

  EXPECT_TRUE(service_->resetRuntimeState().isSuccess());

  EXPECT_TRUE(
      service_->getObjectByName("apple").value().getOwnership().isUnknown());
  EXPECT_TRUE(service_->getPersonByName("Ana").isFailure());
}
