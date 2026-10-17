#include "knowledge_manager/domain/robot.hpp"

#include <cstddef>
#include <utility>

namespace knowledge_manager::domain
{

Robot::Robot(std::string name, KnownLocation startLocation)
: name_(std::move(name)), startLocation_(std::move(startLocation)),
  currentLocation_(startLocation_) {}

std::string Robot::getName() const {return name_;}
void Robot::setName(const std::string & name) {name_ = name;}

KnownLocation Robot::getStartLocation() const {return startLocation_;}
void Robot::setStartLocation(const KnownLocation & startLocation)
{
  startLocation_ = startLocation;
}

KnownLocation Robot::getKnownLocation() const {return currentLocation_;}
void Robot::setKnownLocation(const KnownLocation & knownLocation)
{
  currentLocation_ = knownLocation;
}

std::optional<ManipulableObject> Robot::getObjectInHand(RobotHand hand) const
{
  return handObjects_[static_cast<std::size_t>(hand)];
}

void Robot::setObjectInHand(
  RobotHand hand,
  std::optional<ManipulableObject> object)
{
  handObjects_[static_cast<std::size_t>(hand)] = std::move(object);
}

} // namespace knowledge_manager::domain
