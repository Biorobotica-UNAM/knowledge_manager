#pragma once

#include "knowledge_manager/domain/known_location.hpp"
#include "knowledge_manager/domain/manipulable_object.hpp"
#include "knowledge_manager/domain/robot_hand.hpp"

#include <array>
#include <optional>
#include <string>

namespace knowledge_manager::domain {

class Robot {
public:
  Robot();
  Robot(std::string name, KnownLocation knownLocation);

  std::string getName() const;
  void setName(const std::string &name);

  KnownLocation getKnownLocation() const;
  void setKnownLocation(const KnownLocation &knownLocation);

  std::optional<ManipulableObject> getObjectInHand(RobotHand hand) const;
  void setObjectInHand(RobotHand hand, std::optional<ManipulableObject> object);

private:
  std::string name_;
  KnownLocation knownLocation_;
  std::array<std::optional<ManipulableObject>, 2> handObjects_;
};

} // namespace knowledge_manager::domain
