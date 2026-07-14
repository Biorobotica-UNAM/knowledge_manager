#pragma once

#include "knowledge_manager/domain/known_location.hpp"
#include "knowledge_manager/domain/manipulable_object.hpp"

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

  std::optional<ManipulableObject> getRightArmObject() const;
  void setRightArmObject(const std::optional<ManipulableObject> &object);

  std::optional<ManipulableObject> getLeftArmObject() const;
  void setLeftArmObject(const std::optional<ManipulableObject> &object);

private:
  std::string name_;
  KnownLocation knownLocation_;
  std::optional<ManipulableObject> rightArmObject_;
  std::optional<ManipulableObject> leftArmObject_;
};

} // namespace knowledge_manager::domain
