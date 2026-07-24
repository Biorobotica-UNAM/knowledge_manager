#include "knowledge_manager/domain/object_ownership.hpp"

namespace knowledge_manager::domain
{

ObjectOwnership::ObjectOwnership(Ownership ownership)
: ownership_(std::move(ownership)) {}

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
  return ObjectOwnership(hand);
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
  return std::holds_alternative<RobotHand>(ownership_);
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
  return std::get<RobotHand>(ownership_);
}

} // namespace knowledge_manager::domain
