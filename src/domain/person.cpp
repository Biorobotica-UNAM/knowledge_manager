#include "knowledge_manager/domain/person.hpp"

namespace knowledge_manager::domain
{

Person::Person()
: name_(""), personType_(PersonType::GUEST), physicalDescription_(),
  knownLocation_(), navigablePose_(std::nullopt), favoriteDrink_(""),
  order_(), photo_(std::nullopt) {}

Person::Person(
  std::string name, PersonType personType,
  KnownLocation knownLocation)
: name_(std::move(name)), personType_(personType), physicalDescription_(),
  knownLocation_(std::move(knownLocation)), navigablePose_(std::nullopt),
  favoriteDrink_(""), order_(), photo_(std::nullopt) {}

std::string Person::getName() const {return name_;}
void Person::setName(const std::string & name) {name_ = name;}

PersonType Person::getPersonType() const {return personType_;}
void Person::setPersonType(PersonType personType) {personType_ = personType;}

PhysicalDescription Person::getPhysicalDescription() const
{
  return physicalDescription_;
}
void Person::setPhysicalDescription(
  const PhysicalDescription & physicalDescription)
{
  physicalDescription_ = physicalDescription;
}

KnownLocation Person::getKnownLocation() const {return knownLocation_;}
void Person::setKnownLocation(const KnownLocation & knownLocation)
{
  knownLocation_ = knownLocation;
}

std::optional<Pose3D> Person::getNavigablePose() const
{
  return navigablePose_;
}
void Person::setNavigablePose(const Pose3D & pose) {navigablePose_ = pose;}

std::string Person::getFavoriteDrink() const {return favoriteDrink_;}
void Person::setFavoriteDrink(const std::string & favoriteDrink)
{
  favoriteDrink_ = favoriteDrink;
}

std::vector<ManipulableObject> Person::getOrder() const {return order_;}
void Person::setOrder(const std::vector<ManipulableObject> & order)
{
  order_ = order;
}

std::optional<std::vector<std::byte>> Person::getPhoto() const
{
  return photo_;
}
void Person::setPhoto(const std::vector<std::byte> & photo) {photo_ = photo;}

} // namespace knowledge_manager::domain
