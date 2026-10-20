#include "knowledge_manager/ros_adapter/message_conversions.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>

namespace knowledge_manager::ros_adapter
{

using namespace domain;

namespace
{

[[noreturn]] void invalidValue(const std::string & type, int value)
{
  throw std::invalid_argument(
    "invalid " + type + " value: " + std::to_string(value));
}

} // namespace

// -- domain -> message ---------------------------------------------------

geometry_msgs::msg::Pose toMessage(const Pose3D & pose)
{
  geometry_msgs::msg::Pose message;
  message.position.x = pose.getX();
  message.position.y = pose.getY();
  message.position.z = pose.getZ();
  message.orientation.x = pose.getQx();
  message.orientation.y = pose.getQy();
  message.orientation.z = pose.getQz();
  message.orientation.w = pose.getQw();
  return message;
}

// The enum switches have no default: the compiler warns (-Wswitch) when
// a value is added to the C++ enum and this file is not updated.

interfaces::msg::LocationZone toMessage(LocationZone zone)
{
  interfaces::msg::LocationZone message;
  switch (zone) {
    case LocationZone::KITCHEN:
      message.value = interfaces::msg::LocationZone::KITCHEN;
      return message;
    case LocationZone::LIVING_ROOM:
      message.value = interfaces::msg::LocationZone::LIVING_ROOM;
      return message;
    case LocationZone::BEDROOM:
      message.value = interfaces::msg::LocationZone::BEDROOM;
      return message;
    case LocationZone::LAUNDRY:
      message.value = interfaces::msg::LocationZone::LAUNDRY;
      return message;
  }
  invalidValue("LocationZone", static_cast<int>(zone));
}

interfaces::msg::ObjectCategory toMessage(ObjectCategory category)
{
  using M = interfaces::msg::ObjectCategory;
  M message;
  switch (category) {
    case ObjectCategory::FRUITS: message.value = M::FRUITS; return message;
    case ObjectCategory::TRASH: message.value = M::TRASH; return message;
    case ObjectCategory::TABLEWARE: message.value = M::TABLEWARE; return message;
    case ObjectCategory::CUTLERY: message.value = M::CUTLERY; return message;
    case ObjectCategory::GROCERY: message.value = M::GROCERY; return message;
    case ObjectCategory::DRINKS: message.value = M::DRINKS; return message;
    case ObjectCategory::SNACKS: message.value = M::SNACKS; return message;
    case ObjectCategory::BASKET: message.value = M::BASKET; return message;
    case ObjectCategory::FOOD: message.value = M::FOOD; return message;
    case ObjectCategory::CLEANING_SUPPLY: message.value = M::CLEANING_SUPPLY; return message;
    case ObjectCategory::CONTAINERS: message.value = M::CONTAINERS; return message;
    case ObjectCategory::CONTAINER_DROP: message.value = M::CONTAINER_DROP; return message;
    case ObjectCategory::ELECTRODOMESTICS: message.value = M::ELECTRODOMESTICS; return message;
    case ObjectCategory::TOYS: message.value = M::TOYS; return message;
    case ObjectCategory::FABRIC: message.value = M::FABRIC; return message;
  }
  invalidValue("ObjectCategory", static_cast<int>(category));
}

interfaces::msg::PersonType toMessage(PersonType type)
{
  using M = interfaces::msg::PersonType;
  M message;
  switch (type) {
    case PersonType::GUEST: message.value = M::GUEST; return message;
    case PersonType::CUSTOMER: message.value = M::CUSTOMER; return message;
    case PersonType::HOST: message.value = M::HOST; return message;
    case PersonType::USER: message.value = M::USER; return message;
  }
  invalidValue("PersonType", static_cast<int>(type));
}

interfaces::msg::RobotHand toMessage(RobotHand hand)
{
  using M = interfaces::msg::RobotHand;
  M message;
  switch (hand) {
    case RobotHand::LEFT: message.value = M::LEFT; return message;
    case RobotHand::RIGHT: message.value = M::RIGHT; return message;
  }
  invalidValue("RobotHand", static_cast<int>(hand));
}

interfaces::msg::ObjectOwnership toMessage(const ObjectOwnership & ownership)
{
  using M = interfaces::msg::ObjectOwnership;
  M message;
  if (ownership.isAtLocation()) {
    message.type = M::AT_LOCATION;
    message.location_name = ownership.getLocationName();
  } else if (ownership.isHeldByPerson()) {
    message.type = M::HELD_BY_PERSON;
    message.person_name = ownership.getPersonName();
  } else if (ownership.isHeldByRobot()) {
    message.type = M::HELD_BY_ROBOT;
    message.robot_hand = toMessage(ownership.getRobotHand());
  } else {
    message.type = M::UNKNOWN;
  }
  return message;
}

interfaces::msg::KnownLocation toMessage(const KnownLocation & location)
{
  interfaces::msg::KnownLocation message;
  message.name = location.getName();
  message.pose = toMessage(location.getPose());
  message.zone = toMessage(location.getZone());
  return message;
}

interfaces::msg::PhysicalDescription toMessage(const PhysicalDescription & description)
{
  interfaces::msg::PhysicalDescription message;
  message.gender = description.getGender();
  message.clothes = description.getClothes();
  message.age = description.getAge();
  message.hair = description.getHairType();
  message.facial_expression = description.getFacialExpression();
  message.accessories_phrase = description.getAccessories();
  return message;
}

interfaces::msg::ManipulableObject toMessage(const ManipulableObject & object)
{
  interfaces::msg::ManipulableObject message;
  message.name = object.getName();
  message.category = toMessage(object.getCategory());
  message.usual_zone = toMessage(object.getUsualZone());
  message.ownership = toMessage(object.getOwnership());
  if (object.getNavigablePose().has_value()) {
    message.has_navigable_pose = true;
    message.navigable_pose = toMessage(*object.getNavigablePose());
  }
  if (object.getGraspPose().has_value()) {
    message.has_grasp_pose = true;
    message.grasp_pose = toMessage(*object.getGraspPose());
  }
  return message;
}

interfaces::msg::Person toMessage(const Person & person)
{
  interfaces::msg::Person message;
  message.name = person.getName();
  message.type = toMessage(person.getPersonType());
  message.known_location = toMessage(person.getKnownLocation());
  message.description = toMessage(person.getPhysicalDescription());
  message.favorite_drink = person.getFavoriteDrink();
  if (person.getNavigablePose().has_value()) {
    message.has_navigable_pose = true;
    message.navigable_pose = toMessage(*person.getNavigablePose());
  }
  // getPhoto() returns a copy, so keep that copy: a reference taken from
  // `*person.getPhoto()` would point into a temporary that is gone by
  // the next line.
  const std::optional<std::vector<std::byte>> photo = person.getPhoto();
  if (photo.has_value()) {
    message.photo.reserve(photo->size());
    std::transform(
      photo->begin(), photo->end(), std::back_inserter(message.photo),
      [](std::byte b) {return static_cast<std::uint8_t>(b);});
  }
  for (const ManipulableObject & ordered : person.getOrder()) {
    message.order.push_back(ordered.getName());
  }
  return message;
}

interfaces::msg::Robot toMessage(const Robot & robot)
{
  interfaces::msg::Robot message;
  message.name = robot.getName();
  message.start_location = toMessage(robot.getStartLocation());
  message.current_location = toMessage(robot.getKnownLocation());
  if (const auto left = robot.getObjectInHand(RobotHand::LEFT)) {
    message.has_left_object = true;
    message.left_object = toMessage(*left);
  }
  if (const auto right = robot.getObjectInHand(RobotHand::RIGHT)) {
    message.has_right_object = true;
    message.right_object = toMessage(*right);
  }
  return message;
}

// -- message -> domain ---------------------------------------------------

Pose3D fromMessage(const geometry_msgs::msg::Pose & pose)
{
  return Pose3D(
    static_cast<float>(pose.position.x), static_cast<float>(pose.position.y),
    static_cast<float>(pose.position.z), static_cast<float>(pose.orientation.x),
    static_cast<float>(pose.orientation.y), static_cast<float>(pose.orientation.z),
    static_cast<float>(pose.orientation.w));
}

LocationZone fromMessage(const interfaces::msg::LocationZone & zone)
{
  using M = interfaces::msg::LocationZone;
  switch (zone.value) {
    case M::KITCHEN: return LocationZone::KITCHEN;
    case M::LIVING_ROOM: return LocationZone::LIVING_ROOM;
    case M::BEDROOM: return LocationZone::BEDROOM;
    case M::LAUNDRY: return LocationZone::LAUNDRY;
  }
  invalidValue("LocationZone", zone.value);
}

ObjectCategory fromMessage(const interfaces::msg::ObjectCategory & category)
{
  using M = interfaces::msg::ObjectCategory;
  switch (category.value) {
    case M::FRUITS: return ObjectCategory::FRUITS;
    case M::TRASH: return ObjectCategory::TRASH;
    case M::TABLEWARE: return ObjectCategory::TABLEWARE;
    case M::CUTLERY: return ObjectCategory::CUTLERY;
    case M::GROCERY: return ObjectCategory::GROCERY;
    case M::DRINKS: return ObjectCategory::DRINKS;
    case M::SNACKS: return ObjectCategory::SNACKS;
    case M::BASKET: return ObjectCategory::BASKET;
    case M::FOOD: return ObjectCategory::FOOD;
    case M::CLEANING_SUPPLY: return ObjectCategory::CLEANING_SUPPLY;
    case M::CONTAINERS: return ObjectCategory::CONTAINERS;
    case M::CONTAINER_DROP: return ObjectCategory::CONTAINER_DROP;
    case M::ELECTRODOMESTICS: return ObjectCategory::ELECTRODOMESTICS;
    case M::TOYS: return ObjectCategory::TOYS;
    case M::FABRIC: return ObjectCategory::FABRIC;
  }
  invalidValue("ObjectCategory", category.value);
}

PersonType fromMessage(const interfaces::msg::PersonType & type)
{
  using M = interfaces::msg::PersonType;
  switch (type.value) {
    case M::GUEST: return PersonType::GUEST;
    case M::CUSTOMER: return PersonType::CUSTOMER;
    case M::HOST: return PersonType::HOST;
    case M::USER: return PersonType::USER;
  }
  invalidValue("PersonType", type.value);
}

RobotHand fromMessage(const interfaces::msg::RobotHand & hand)
{
  using M = interfaces::msg::RobotHand;
  switch (hand.value) {
    case M::LEFT: return RobotHand::LEFT;
    case M::RIGHT: return RobotHand::RIGHT;
  }
  invalidValue("RobotHand", hand.value);
}

PhysicalDescription fromMessage(const interfaces::msg::PhysicalDescription & message)
{
  PhysicalDescription description;
  description.setGender(message.gender);
  description.setClothes(message.clothes);
  description.setAge(message.age);
  description.setHairType(message.hair);
  description.setFacialExpression(message.facial_expression);
  description.setAccessories(message.accessories_phrase);
  return description;
}

} // namespace knowledge_manager::ros_adapter
