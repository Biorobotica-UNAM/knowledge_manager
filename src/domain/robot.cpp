#include "knowledge_manager/domain/robot.hpp"

namespace knowledge_manager::domain {

Robot::Robot()
    : name_(""), knownLocation_(), rightArmObject_(std::nullopt),
      leftArmObject_(std::nullopt) {}

Robot::Robot(std::string name, KnownLocation knownLocation)
    : name_(std::move(name)), knownLocation_(std::move(knownLocation)),
      rightArmObject_(std::nullopt), leftArmObject_(std::nullopt) {}

std::string Robot::getName() const { return name_; }
void Robot::setName(const std::string &name) { name_ = name; }

KnownLocation Robot::getKnownLocation() const { return knownLocation_; }
void Robot::setKnownLocation(const KnownLocation &knownLocation) {
  knownLocation_ = knownLocation;
}

std::optional<ManipulableObject> Robot::getRightArmObject() const {
  return rightArmObject_;
}
void Robot::setRightArmObject(const std::optional<ManipulableObject> &object) {
  rightArmObject_ = object;
}

std::optional<ManipulableObject> Robot::getLeftArmObject() const {
  return leftArmObject_;
}
void Robot::setLeftArmObject(const std::optional<ManipulableObject> &object) {
  leftArmObject_ = object;
}
} // namespace knowledge_manager::domain
