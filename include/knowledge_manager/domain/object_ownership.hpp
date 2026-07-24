#pragma once

#include "knowledge_manager/domain/robot_hand.hpp"

#include <string>
#include <variant>

namespace knowledge_manager::domain
{

class ObjectOwnership {
public:
  static ObjectOwnership atLocation(std::string locationName);
  static ObjectOwnership heldByPerson(std::string personName);
  static ObjectOwnership heldByRobot(RobotHand hand);

  bool isAtLocation() const;
  bool isHeldByPerson() const;
  bool isHeldByRobot() const;

  std::string getLocationName() const;
  std::string getPersonName() const;
  RobotHand getRobotHand() const;

private:
  struct AtLocation
  {
    std::string name;
  };
  struct HeldByPerson
  {
    std::string name;
  };

  using Ownership = std::variant<AtLocation, HeldByPerson, RobotHand>;

  explicit ObjectOwnership(Ownership ownership);
  Ownership ownership_;
};

} // namespace knowledge_manager::domain
