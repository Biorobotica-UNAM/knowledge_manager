#pragma once

#include "knowledge_manager/domain/robot_hand.hpp"

#include <string>
#include <variant>

namespace knowledge_manager::domain {

struct HeldByLocation {
  std::string locationName;
};
struct HeldByPerson {
  std::string personName;
};
struct HeldByRobot {
  RobotHand hand;
};

using ObjectOwnership = std::variant<HeldByLocation, HeldByPerson, HeldByRobot>;

} // namespace knowledge_manager::domain
