#include "knowledge_manager/domain/physical_description.hpp"

namespace knowledge_manager::domain
{

PhysicalDescription::PhysicalDescription()
: gender_(""), clothes_(""), age_(""), hairType_(""), facialExpression_(""),
  accessories_("") {}

std::string PhysicalDescription::getGender() const {return gender_;}
void PhysicalDescription::setGender(const std::string & gender)
{
  gender_ = gender;
}

std::string PhysicalDescription::getClothes() const {return clothes_;}
void PhysicalDescription::setClothes(const std::string & clothes)
{
  clothes_ = clothes;
}

std::string PhysicalDescription::getAge() const {return age_;}
void PhysicalDescription::setAge(const std::string & age) {age_ = age;}

std::string PhysicalDescription::getHairType() const {return hairType_;}
void PhysicalDescription::setHairType(const std::string & hairType)
{
  hairType_ = hairType;
}

std::string PhysicalDescription::getFacialExpression() const
{
  return facialExpression_;
}
void PhysicalDescription::setFacialExpression(
  const std::string & facialExpression)
{
  facialExpression_ = facialExpression;
}

std::string PhysicalDescription::getAccessories() const {return accessories_;}
void PhysicalDescription::setAccessories(const std::string & accessories)
{
  accessories_ = accessories;
}

} // namespace knowledge_manager::domain
