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

// -- References: name or positional alias -------------------------------

TEST_F(WorldStateServiceTest, ResolveObject_AcceptsNameAndAlias) {
  ASSERT_TRUE(service_
    ->registerObject("pear", ObjectCategory::FRUITS,
                                   LocationZone::KITCHEN)
    .isSuccess());
  ASSERT_TRUE(service_
    ->registerObject("apple", ObjectCategory::FRUITS,
                                   LocationZone::KITCHEN)
    .isSuccess());
  ASSERT_TRUE(service_
    ->registerObject("towel", ObjectCategory::FABRIC,
                                   LocationZone::LAUNDRY)
    .isSuccess());

  EXPECT_EQ(service_->resolveObject("apple").value().getName(), "apple");
  EXPECT_EQ(service_->resolveObject("fruits_1").value().getName(), "pear");
  EXPECT_EQ(service_->resolveObject("fruits_2").value().getName(), "apple");
  EXPECT_EQ(service_->resolveObject("fabric_1").value().getName(), "towel");
  EXPECT_TRUE(service_->resolveObject("fruits_3").isFailure());
  EXPECT_TRUE(service_->resolveObject("cutlery_1").isFailure());
}

TEST_F(WorldStateServiceTest, ResolveObject_AliasOfMultiWordCategory) {
  ASSERT_TRUE(service_
    ->registerObject("spray", ObjectCategory::CLEANING_SUPPLY,
                                   LocationZone::LAUNDRY)
    .isSuccess());
  EXPECT_EQ(service_->resolveObject("cleaning_supply_1").value().getName(),
            "spray");
}

TEST_F(WorldStateServiceTest,
       ResolveObject_UnderscoreNameThatIsNotAnAlias_IsAPlainName) {
  ASSERT_TRUE(service_
    ->registerObject("red_apple_2", ObjectCategory::FRUITS,
                                   LocationZone::KITCHEN)
    .isSuccess());
  EXPECT_EQ(service_->resolveObject("red_apple_2").value().getName(),
            "red_apple_2");
}

TEST_F(WorldStateServiceTest, ResolvePerson_AcceptsNameAndAlias) {
  ASSERT_TRUE(service_->createOrUpdateKnownLocation(counter()).isSuccess());
  ASSERT_TRUE(
      service_->registerPerson("Zoe", PersonType::GUEST, "kitchen_counter")
    .isSuccess());
  ASSERT_TRUE(
      service_->registerPerson("Ana", PersonType::GUEST, "kitchen_counter")
    .isSuccess());
  ASSERT_TRUE(
      service_->registerPerson("Hugo", PersonType::HOST, "kitchen_counter")
    .isSuccess());

  EXPECT_EQ(service_->resolvePerson("guest_2").value().getName(), "Ana");
  EXPECT_EQ(service_->resolvePerson("host_1").value().getName(), "Hugo");
  EXPECT_EQ(service_->resolvePerson("Zoe").value().getName(), "Zoe");
  EXPECT_TRUE(service_->resolvePerson("guest_3").isFailure());
  EXPECT_TRUE(service_->resolvePersonWithPhoto("user_1").isFailure());
}

TEST_F(WorldStateServiceTest, NamesThatLookLikeAliases_AreRejected) {
  ASSERT_TRUE(service_->createOrUpdateKnownLocation(counter()).isSuccess());
  EXPECT_TRUE(service_
    ->registerObject("fruits_1", ObjectCategory::FRUITS,
                                   LocationZone::KITCHEN)
    .isFailure());
  EXPECT_TRUE(
      service_->registerPerson("guest_1", PersonType::GUEST, "kitchen_counter")
    .isFailure());
  EXPECT_TRUE(
      service_
    ->createOrUpdatePerson(Person("user_2", PersonType::USER, counter()))
    .isFailure());
  EXPECT_TRUE(
      service_
    ->registerObject("", ObjectCategory::FRUITS, LocationZone::KITCHEN)
    .isFailure());
  EXPECT_FALSE(repo_->loadObjectByName("fruits_1").has_value());
}

// -- Register ------------------------------------------------------------

TEST_F(WorldStateServiceTest, RegisterObject_StartsUnknownWithoutPoses) {
  ASSERT_TRUE(service_
    ->registerObject("apple", ObjectCategory::FRUITS,
                                   LocationZone::KITCHEN)
    .isSuccess());
  auto apple = service_->getObjectByName("apple").value();
  EXPECT_TRUE(apple.getOwnership().isUnknown());
  EXPECT_EQ(apple.getUsualZone(), LocationZone::KITCHEN);
  EXPECT_FALSE(apple.getNavigablePose().has_value());
}

TEST_F(WorldStateServiceTest,
       RegisterObject_Again_ChangesKnowledgeButKeepsRunState) {
  ASSERT_TRUE(service_->createOrUpdateKnownLocation(counter()).isSuccess());
  ASSERT_TRUE(service_
    ->registerObject("apple", ObjectCategory::FRUITS,
                                   LocationZone::KITCHEN)
    .isSuccess());
  ASSERT_TRUE(
      service_->setObjectAtLocation("apple", "kitchen_counter").isSuccess());
  ASSERT_TRUE(
      service_->setObjectNavigablePose("apple", Pose3D(1, 2, 0, 0, 0, 0, 1))
    .isSuccess());

  ASSERT_TRUE(
      service_
    ->registerObject("apple", ObjectCategory::FOOD, LocationZone::BEDROOM)
    .isSuccess());

  auto apple = service_->getObjectByName("apple").value();
  EXPECT_EQ(apple.getCategory(), ObjectCategory::FOOD);
  EXPECT_EQ(apple.getUsualZone(), LocationZone::BEDROOM);
  EXPECT_TRUE(apple.getOwnership().isAtLocation());
  ASSERT_TRUE(apple.getNavigablePose().has_value());
}

TEST_F(WorldStateServiceTest, RegisterPerson_NeedsAnExistingLocation) {
  auto result = service_->registerPerson("Ana", PersonType::GUEST, "nowhere");
  ASSERT_TRUE(result.isFailure());
  EXPECT_NE(result.error().find("nowhere"), std::string::npos);
  EXPECT_TRUE(service_->getPersonByName("Ana").isFailure());
}

TEST_F(WorldStateServiceTest,
       RegisterPerson_Again_ChangesTypeAndLocationKeepsTheRest) {
  ASSERT_TRUE(service_->createOrUpdateKnownLocation(counter()).isSuccess());
  ASSERT_TRUE(service_
    ->createOrUpdateKnownLocation(KnownLocation(
                      "sofa", Pose3D(), LocationZone::LIVING_ROOM))
    .isSuccess());
  ASSERT_TRUE(
      service_->registerPerson("Ana", PersonType::GUEST, "kitchen_counter")
    .isSuccess());
  ASSERT_TRUE(service_->setPersonFavoriteDrink("Ana", "tea").isSuccess());

  ASSERT_TRUE(service_->registerPerson("Ana", PersonType::CUSTOMER, "sofa")
    .isSuccess());

  auto ana = service_->getPersonByName("Ana").value();
  EXPECT_EQ(ana.getPersonType(), PersonType::CUSTOMER);
  EXPECT_EQ(ana.getKnownLocation().getName(), "sofa");
  EXPECT_EQ(ana.getFavoriteDrink(), "tea");
}

// -- Object run state --------------------------------------------------

TEST_F(WorldStateServiceTest, SetObjectOwnership_AllFourCases_ThroughAliases) {
  ASSERT_TRUE(service_->createOrUpdateKnownLocation(counter()).isSuccess());
  ASSERT_TRUE(
      service_->registerPerson("Ana", PersonType::GUEST, "kitchen_counter")
    .isSuccess());
  ASSERT_TRUE(service_
    ->registerObject("apple", ObjectCategory::FRUITS,
                                   LocationZone::KITCHEN)
    .isSuccess());

  ASSERT_TRUE(
      service_->setObjectAtLocation("fruits_1", "kitchen_counter").isSuccess());
  EXPECT_TRUE(
      service_->getObjectByName("apple").value().getOwnership().isAtLocation());

  ASSERT_TRUE(
      service_->setObjectHeldByPerson("fruits_1", "guest_1").isSuccess());
  auto held = service_->getObjectByName("apple").value().getOwnership();
  ASSERT_TRUE(held.isHeldByPerson());
  EXPECT_EQ(held.getPersonName(), "Ana");

  ASSERT_TRUE(
      service_->setObjectHeldByRobot("apple", RobotHand::LEFT).isSuccess());
  EXPECT_TRUE(service_->getObjectByName("apple")
    .value()
    .getOwnership()
    .isHeldByRobot());

  ASSERT_TRUE(service_->forgetObjectOwnership("apple").isSuccess());
  EXPECT_TRUE(
      service_->getObjectByName("apple").value().getOwnership().isUnknown());
}

TEST_F(WorldStateServiceTest,
       SetObjectAtLocation_UnknownLocationOrObject_Fails) {
  ASSERT_TRUE(service_
    ->registerObject("apple", ObjectCategory::FRUITS,
                                   LocationZone::KITCHEN)
    .isSuccess());
  EXPECT_TRUE(service_->setObjectAtLocation("apple", "nowhere").isFailure());
  EXPECT_TRUE(
      service_->setObjectAtLocation("ghost", "kitchen_counter").isFailure());
  EXPECT_TRUE(
      service_->getObjectByName("apple").value().getOwnership().isUnknown());
}

TEST_F(WorldStateServiceTest,
       SetObjectHeldByRobot_HandOccupied_FailsWithFriendlyMessage) {
  ASSERT_TRUE(service_->createOrUpdateKnownLocation(counter()).isSuccess());
  ASSERT_TRUE(service_->updateRobot(Robot("Justina", counter())).isSuccess());
  ASSERT_TRUE(service_
    ->registerObject("pear", ObjectCategory::FRUITS,
                                   LocationZone::KITCHEN)
    .isSuccess());
  ASSERT_TRUE(service_
    ->registerObject("kiwi", ObjectCategory::FRUITS,
                                   LocationZone::KITCHEN)
    .isSuccess());
  ASSERT_TRUE(
      service_->setObjectHeldByRobot("pear", RobotHand::LEFT).isSuccess());

  auto result = service_->setObjectHeldByRobot("kiwi", RobotHand::LEFT);
  ASSERT_TRUE(result.isFailure());
  EXPECT_NE(result.error().find("pear"), std::string::npos);
  EXPECT_TRUE(
      service_->setObjectHeldByRobot("kiwi", RobotHand::RIGHT).isSuccess());
  // Re-stating that the same object is in the same hand is fine.
  EXPECT_TRUE(
      service_->setObjectHeldByRobot("pear", RobotHand::LEFT).isSuccess());
}

TEST_F(WorldStateServiceTest, GetRobot_ShowsObjectsSetThroughTheirOwnership) {
  ASSERT_TRUE(service_->createOrUpdateKnownLocation(counter()).isSuccess());
  ASSERT_TRUE(service_->updateRobot(Robot("Justina", counter())).isSuccess());
  ASSERT_TRUE(service_
    ->registerObject("pear", ObjectCategory::FRUITS,
                                   LocationZone::KITCHEN)
    .isSuccess());
  ASSERT_TRUE(
      service_->setObjectHeldByRobot("pear", RobotHand::RIGHT).isSuccess());

  auto robot = service_->getRobot().value();
  ASSERT_TRUE(robot.getObjectInHand(RobotHand::RIGHT).has_value());
  EXPECT_EQ(robot.getObjectInHand(RobotHand::RIGHT)->getName(), "pear");
  EXPECT_FALSE(robot.getObjectInHand(RobotHand::LEFT).has_value());
}

TEST_F(WorldStateServiceTest, SetObjectPoses_AreStoredAndIndependent) {
  ASSERT_TRUE(service_
    ->registerObject("apple", ObjectCategory::FRUITS,
                                   LocationZone::KITCHEN)
    .isSuccess());
  ASSERT_TRUE(
      service_->setObjectNavigablePose("apple", Pose3D(1, 0, 0, 0, 0, 0, 1))
    .isSuccess());
  ASSERT_TRUE(service_->setObjectGraspPose("apple", Pose3D(2, 0, 0, 0, 0, 0, 1))
    .isSuccess());

  auto apple = service_->getObjectByName("apple").value();
  ASSERT_TRUE(apple.getNavigablePose().has_value());
  ASSERT_TRUE(apple.getGraspPose().has_value());
  EXPECT_FLOAT_EQ(apple.getNavigablePose()->getX(), 1.0f);
  EXPECT_FLOAT_EQ(apple.getGraspPose()->getX(), 2.0f);
}

// -- Robot and person run state ---------------------------------------

TEST_F(WorldStateServiceTest, SetRobotLocation_NeedsRobotAndLocation) {
  EXPECT_TRUE(
      service_->setRobotLocation("kitchen_counter").isFailure()); // no robot
  ASSERT_TRUE(service_->createOrUpdateKnownLocation(counter()).isSuccess());
  ASSERT_TRUE(service_
    ->createOrUpdateKnownLocation(KnownLocation(
                      "sofa", Pose3D(), LocationZone::LIVING_ROOM))
    .isSuccess());
  ASSERT_TRUE(service_->updateRobot(Robot("Justina", counter())).isSuccess());

  EXPECT_TRUE(service_->setRobotLocation("nowhere").isFailure());
  ASSERT_TRUE(service_->setRobotLocation("sofa").isSuccess());

  auto robot = service_->getRobot().value();
  EXPECT_EQ(robot.getKnownLocation().getName(), "sofa");
  EXPECT_EQ(robot.getStartLocation().getName(), "kitchen_counter");
}

TEST_F(WorldStateServiceTest, PersonSetters_ChangeOnlyTheirOwnField) {
  ASSERT_TRUE(service_->createOrUpdateKnownLocation(counter()).isSuccess());
  ASSERT_TRUE(service_
    ->createOrUpdateKnownLocation(KnownLocation(
                      "sofa", Pose3D(), LocationZone::LIVING_ROOM))
    .isSuccess());
  ASSERT_TRUE(
      service_->registerPerson("Ana", PersonType::GUEST, "kitchen_counter")
    .isSuccess());

  PhysicalDescription look;
  look.setClothes("red shirt");
  ASSERT_TRUE(service_->setPersonDescription("guest_1", look).isSuccess());
  ASSERT_TRUE(service_->setPersonFavoriteDrink("Ana", "tea").isSuccess());
  ASSERT_TRUE(
      service_->setPersonNavigablePose("Ana", Pose3D(5, 6, 0, 0, 0, 0, 1))
    .isSuccess());
  ASSERT_TRUE(service_->setPersonLocation("Ana", "sofa").isSuccess());
  ASSERT_TRUE(service_->setPersonType("Ana", PersonType::CUSTOMER).isSuccess());

  auto ana = service_->getPersonByName("Ana").value();
  EXPECT_EQ(ana.getPersonType(), PersonType::CUSTOMER);
  EXPECT_EQ(ana.getKnownLocation().getName(), "sofa");
  EXPECT_EQ(ana.getFavoriteDrink(), "tea");
  EXPECT_EQ(ana.getPhysicalDescription().getClothes(), "red shirt");
  ASSERT_TRUE(ana.getNavigablePose().has_value());
  EXPECT_FLOAT_EQ(ana.getNavigablePose()->getX(), 5.0f);
}

TEST_F(WorldStateServiceTest, SetPersonType_User_DemotesThePreviousUser) {
  ASSERT_TRUE(service_->createOrUpdateKnownLocation(counter()).isSuccess());
  ASSERT_TRUE(
      service_->registerPerson("Bob", PersonType::USER, "kitchen_counter")
    .isSuccess());
  ASSERT_TRUE(
      service_->registerPerson("Ana", PersonType::GUEST, "kitchen_counter")
    .isSuccess());

  ASSERT_TRUE(service_->setPersonType("Ana", PersonType::USER).isSuccess());

  EXPECT_EQ(service_->getPersonByName("Ana").value().getPersonType(),
            PersonType::USER);
  EXPECT_EQ(service_->getPersonByName("Bob").value().getPersonType(),
            PersonType::GUEST);
}

TEST_F(WorldStateServiceTest, SetPersonPhoto_StoresIt_AndOtherSettersKeepIt) {
  ASSERT_TRUE(service_->createOrUpdateKnownLocation(counter()).isSuccess());
  ASSERT_TRUE(
      service_->registerPerson("Ana", PersonType::GUEST, "kitchen_counter")
    .isSuccess());
  const std::vector<std::byte> photo{std::byte{1}, std::byte{2}, std::byte{3}};

  EXPECT_TRUE(service_->setPersonPhoto("Ana", {}).isFailure());
  ASSERT_TRUE(service_->setPersonPhoto("Ana", photo).isSuccess());
  ASSERT_TRUE(service_->setPersonFavoriteDrink("Ana", "tea").isSuccess());
  ASSERT_TRUE(service_->setPersonType("Ana", PersonType::CUSTOMER).isSuccess());

  auto withPhoto = service_->resolvePersonWithPhoto("Ana").value();
  ASSERT_TRUE(withPhoto.getPhoto().has_value());
  EXPECT_EQ(*withPhoto.getPhoto(), photo);
  EXPECT_FALSE(service_->resolvePerson("Ana").value().getPhoto().has_value());
}

TEST_F(WorldStateServiceTest, SetPersonOrder_ResolvesAliasesAndIsAllOrNothing) {
  ASSERT_TRUE(service_->createOrUpdateKnownLocation(counter()).isSuccess());
  ASSERT_TRUE(
      service_->registerPerson("Ana", PersonType::GUEST, "kitchen_counter")
    .isSuccess());
  ASSERT_TRUE(service_
    ->registerObject("apple", ObjectCategory::FRUITS,
                                   LocationZone::KITCHEN)
    .isSuccess());
  ASSERT_TRUE(service_
    ->registerObject("fork", ObjectCategory::CUTLERY,
                                   LocationZone::KITCHEN)
    .isSuccess());

  EXPECT_TRUE(service_->setPersonOrder("Ana", {"apple", "ghost"}).isFailure());
  EXPECT_TRUE(service_->getPersonByName("Ana").value().getOrder().empty());

  ASSERT_TRUE(
      service_->setPersonOrder("Ana", {"fruits_1", "fork"}).isSuccess());
  auto order = service_->getPersonByName("Ana").value().getOrder();
  ASSERT_EQ(order.size(), 2u);
  EXPECT_EQ(order[0].getName(), "apple");
  EXPECT_EQ(order[1].getName(), "fork");
}

TEST_F(WorldStateServiceTest, PersonSetters_UnknownPerson_Fail) {
  EXPECT_TRUE(service_->setPersonFavoriteDrink("ghost", "tea").isFailure());
  EXPECT_TRUE(service_->setPersonType("guest_9", PersonType::USER).isFailure());
  EXPECT_TRUE(service_->setObjectHeldByPerson("apple", "ghost").isFailure());
}
