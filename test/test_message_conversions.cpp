#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "knowledge_manager/domain/known_location.hpp"
#include "knowledge_manager/domain/manipulable_object.hpp"
#include "knowledge_manager/domain/object_ownership.hpp"
#include "knowledge_manager/domain/person.hpp"
#include "knowledge_manager/domain/robot.hpp"
#include "knowledge_manager/ros_adapter/message_conversions.hpp"

using namespace knowledge_manager::domain;
using namespace knowledge_manager::ros_adapter;
namespace msg = knowledge_interfaces::msg;

namespace
{

KnownLocation counter()
{
  return KnownLocation("kitchen_counter", Pose3D(1, 2, 3, 0, 0, 0, 1),
                       LocationZone::KITCHEN);
}

} // namespace

TEST(MessageConversions, EveryObjectCategoryRoundTrips) {
  for (uint8_t value = 0; value <= msg::ObjectCategory::FABRIC; ++value) {
    msg::ObjectCategory original;
    original.value = value;
    EXPECT_EQ(toMessage(fromMessage(original)).value, value);
  }
  msg::ObjectCategory invalid;
  invalid.value = msg::ObjectCategory::FABRIC + 1;
  EXPECT_THROW(fromMessage(invalid), std::invalid_argument);
}

TEST(MessageConversions, EveryPersonTypeZoneAndHandRoundTrips) {
  for (uint8_t value = 0; value <= msg::PersonType::USER; ++value) {
    msg::PersonType type;
    type.value = value;
    EXPECT_EQ(toMessage(fromMessage(type)).value, value);
  }
  for (uint8_t value = 0; value <= msg::LocationZone::LAUNDRY; ++value) {
    msg::LocationZone zone;
    zone.value = value;
    EXPECT_EQ(toMessage(fromMessage(zone)).value, value);
  }
  for (uint8_t value = 0; value <= msg::RobotHand::RIGHT; ++value) {
    msg::RobotHand hand;
    hand.value = value;
    EXPECT_EQ(toMessage(fromMessage(hand)).value, value);
  }
}

TEST(MessageConversions, EnumValuesMatchTheCppEnums) {
  // The numbers in the .msg files are a contract with every client:
  // they must stay equal to the order of the C++ enums.
  EXPECT_EQ(toMessage(LocationZone::LAUNDRY).value, msg::LocationZone::LAUNDRY);
  EXPECT_EQ(toMessage(ObjectCategory::FABRIC).value,
            msg::ObjectCategory::FABRIC);
  EXPECT_EQ(toMessage(PersonType::USER).value, msg::PersonType::USER);
  EXPECT_EQ(toMessage(PersonType::CUSTOMER).value, msg::PersonType::CUSTOMER);
  EXPECT_EQ(toMessage(RobotHand::RIGHT).value, msg::RobotHand::RIGHT);
  EXPECT_EQ(static_cast<int>(ObjectCategory::FABRIC),
            msg::ObjectCategory::FABRIC);
  EXPECT_EQ(static_cast<int>(PersonType::USER), msg::PersonType::USER);
  EXPECT_EQ(static_cast<int>(LocationZone::LAUNDRY),
            msg::LocationZone::LAUNDRY);
}

TEST(MessageConversions, PoseRoundTrips) {
  const Pose3D original(1.5f, -2.0f, 0.25f, 0.1f, 0.2f, 0.3f, 0.9f);
  const Pose3D back = fromMessage(toMessage(original));
  EXPECT_FLOAT_EQ(back.getX(), 1.5f);
  EXPECT_FLOAT_EQ(back.getY(), -2.0f);
  EXPECT_FLOAT_EQ(back.getZ(), 0.25f);
  EXPECT_FLOAT_EQ(back.getQx(), 0.1f);
  EXPECT_FLOAT_EQ(back.getQy(), 0.2f);
  EXPECT_FLOAT_EQ(back.getQz(), 0.3f);
  EXPECT_FLOAT_EQ(back.getQw(), 0.9f);
}

TEST(MessageConversions, OwnershipUsesOnlyTheFieldOfItsCase) {
  const auto atLocation =
    toMessage(ObjectOwnership::atLocation("kitchen_counter"));
  EXPECT_EQ(atLocation.type, msg::ObjectOwnership::AT_LOCATION);
  EXPECT_EQ(atLocation.location_name, "kitchen_counter");
  EXPECT_EQ(atLocation.person_name, "");

  const auto byPerson = toMessage(ObjectOwnership::heldByPerson("Ana"));
  EXPECT_EQ(byPerson.type, msg::ObjectOwnership::HELD_BY_PERSON);
  EXPECT_EQ(byPerson.person_name, "Ana");

  const auto byRobot = toMessage(ObjectOwnership::heldByRobot(RobotHand::LEFT));
  EXPECT_EQ(byRobot.type, msg::ObjectOwnership::HELD_BY_ROBOT);
  EXPECT_EQ(byRobot.robot_hand.value, msg::RobotHand::LEFT);

  EXPECT_EQ(toMessage(ObjectOwnership::unknown()).type,
            msg::ObjectOwnership::UNKNOWN);
}

TEST(MessageConversions, ObjectFlagsOptionalPoses) {
  ManipulableObject object("apple", ObjectCategory::FRUITS,
    LocationZone::KITCHEN, ObjectOwnership::unknown());
  auto bare = toMessage(object);
  EXPECT_FALSE(bare.has_navigable_pose);
  EXPECT_FALSE(bare.has_grasp_pose);

  object.setGraspPose(Pose3D(7, 0, 0, 0, 0, 0, 1));
  auto withGrasp = toMessage(object);
  EXPECT_FALSE(withGrasp.has_navigable_pose);
  ASSERT_TRUE(withGrasp.has_grasp_pose);
  EXPECT_DOUBLE_EQ(withGrasp.grasp_pose.position.x, 7.0);
}

TEST(MessageConversions, PersonKeepsPhotoBytesOrderAndDescription) {
  Person person("Ana", PersonType::CUSTOMER, counter());
  person.setPhoto(
    {std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF}});
  person.setOrder(
    {ManipulableObject("apple", ObjectCategory::FRUITS, LocationZone::KITCHEN,
                         ObjectOwnership::unknown())});
  PhysicalDescription look;
  look.setClothes("red shirt");
  look.setHairType("long");
  person.setPhysicalDescription(look);

  const auto message = toMessage(person);
  EXPECT_EQ(message.photo, (std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF}));
  ASSERT_EQ(message.order.size(), 1u);
  EXPECT_EQ(message.order[0], "apple");
  EXPECT_EQ(message.type.value, msg::PersonType::CUSTOMER);
  EXPECT_EQ(message.description.clothes, "red shirt");
  EXPECT_EQ(message.description.hair, "long");
  EXPECT_EQ(message.known_location.zone.value, msg::LocationZone::KITCHEN);
  EXPECT_FALSE(message.has_navigable_pose);
}

TEST(MessageConversions, PersonWithoutPhoto_HasEmptyPhoto) {
  EXPECT_TRUE(
      toMessage(Person("Ana", PersonType::GUEST, counter())).photo.empty());
}

TEST(MessageConversions, RobotShowsStartCurrentLocationAndHands) {
  Robot robot("Justina", counter());
  robot.setKnownLocation(
      KnownLocation("sofa", Pose3D(), LocationZone::LIVING_ROOM));
  robot.setObjectInHand(
      RobotHand::RIGHT,
      ManipulableObject("spoon", ObjectCategory::CUTLERY, LocationZone::KITCHEN,
                        ObjectOwnership::heldByRobot(RobotHand::RIGHT)));

  const auto message = toMessage(robot);
  EXPECT_EQ(message.start_location.name, "kitchen_counter");
  EXPECT_EQ(message.current_location.name, "sofa");
  EXPECT_FALSE(message.has_left_object);
  ASSERT_TRUE(message.has_right_object);
  EXPECT_EQ(message.right_object.name, "spoon");
}

TEST(MessageConversions, PhysicalDescriptionRoundTrips) {
  msg::PhysicalDescription message;
  message.gender = "female";
  message.clothes = "red shirt";
  message.age = "adult";
  message.hair = "long";
  message.facial_expression = "smiling";
  message.accessories_phrase = "wearing glasses";

  const auto back = toMessage(fromMessage(message));
  EXPECT_EQ(back.gender, "female");
  EXPECT_EQ(back.clothes, "red shirt");
  EXPECT_EQ(back.age, "adult");
  EXPECT_EQ(back.hair, "long");
  EXPECT_EQ(back.facial_expression, "smiling");
  EXPECT_EQ(back.accessories_phrase, "wearing glasses");
}
