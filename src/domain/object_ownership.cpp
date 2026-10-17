#include "knowledge_manager/domain/object_ownership.hpp"

#include <utility>

namespace knowledge_manager::domain
{

ObjectOwnership::ObjectOwnership(Ownership ownership)
: ownership_(std::move(ownership)) {}

ObjectOwnership ObjectOwnership::unknown()
{
  return ObjectOwnership(Unknown{});
}
ObjectOwnership ObjectOwnership::atLocation(std::string locationName)
{
  return ObjectOwnership(AtLocation{std::move(locationName)});
}
ObjectOwnership ObjectOwnership::heldByPerson(std::string personName)
{
  return ObjectOwnership(HeldByPerson{std::move(personName)});
}
ObjectOwnership ObjectOwnership::heldByRobot(RobotHand hand)
{
  return ObjectOwnership(HeldByRobot{hand});
}

bool ObjectOwnership::isUnknown() const
{
  return std::holds_alternative<Unknown>(ownership_);
}
bool ObjectOwnership::isAtLocation() const
{
  return std::holds_alternative<AtLocation>(ownership_);
}
bool ObjectOwnership::isHeldByPerson() const
{
  return std::holds_alternative<HeldByPerson>(ownership_);
}
bool ObjectOwnership::isHeldByRobot() const
{
  return std::holds_alternative<HeldByRobot>(ownership_);
}

std::string ObjectOwnership::getLocationName() const
{
  return std::get<AtLocation>(ownership_).name;
}
std::string ObjectOwnership::getPersonName() const
{
  return std::get<HeldByPerson>(ownership_).name;
}
RobotHand ObjectOwnership::getRobotHand() const
{
  return std::get<HeldByRobot>(ownership_).hand;
}

} // namespace knowledge_manager::domain
