#pragma once

#include "knowledge_manager/domain/known_location.hpp"
#include "knowledge_manager/domain/manipulable_object.hpp"
#include "knowledge_manager/domain/person_type.hpp"
#include "knowledge_manager/domain/physical_description.hpp"
#include "knowledge_manager/domain/pose3d.hpp"

#include <optional>
#include <string>
#include <vector>

namespace knowledge_manager::domain {

class Person {
public:
  Person();
  Person(std::string name, PersonType personType, KnownLocation knownLocation);

  std::string getName() const;
  void setName(const std::string &name);

  PersonType getPersonType() const;
  void setPersonType(PersonType personType);

  PhysicalDescription getPhysicalDescription() const;
  void setPhysicalDescription(const PhysicalDescription &physicalDescription);

  KnownLocation getKnownLocation() const;
  void setKnownLocation(const KnownLocation &knownLocation);

  std::optional<Pose3D> getNavigablePose() const;
  void setNavigablePose(const Pose3D &pose);

  std::string getFavoriteDrink() const;
  void setFavoriteDrink(const std::string &favoriteDrink);

  std::vector<ManipulableObject> getOrder() const;
  void setOrder(const std::vector<ManipulableObject> &order);

private:
  std::string name_;
  PersonType personType_;
  PhysicalDescription physicalDescription_;
  KnownLocation knownLocation_;
  std::optional<Pose3D> navigablePose_;
  std::string favoriteDrink_;
  std::vector<ManipulableObject> order_;
};

} // namespace knowledge_manager::domain
