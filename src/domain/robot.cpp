#include "knowledge_manager/domain/robot.hpp"

namespace knowledge_manager::domain {

Robot::Robot() : name_(""), knownLocation_(), handObjects_() {}

Robot::Robot(std::string name, KnownLocation knownLocation)
    : name_(std::move(name)), knownLocation_(std::move(knownLocation)),
      handObjects_() {}

std::string Robot::getName() const { return name_; }

void Robot::setName(const std::string &name) { name_ = name; }

KnownLocation Robot::getKnownLocation() const { return knownLocation_; }

void Robot::setKnownLocation(const KnownLocation &knownLocation) {
  knownLocation_ = knownLocation;
}

std::optional<ManipulableObject> Robot::getObjectInHand(RobotHand hand) const {
  return handObjects_[static_cast<size_t>(hand)];
}

void Robot::setObjectInHand(RobotHand hand,
                            std::optional<ManipulableObject> object) {
  handObjects_[static_cast<size_t>(hand)] = std::move(object);
}

} // namespace knowledge_manager::domain
