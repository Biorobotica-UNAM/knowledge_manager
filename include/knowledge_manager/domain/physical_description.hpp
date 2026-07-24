#pragma once

#include <string>

namespace knowledge_manager::domain
{

class PhysicalDescription {
public:
  PhysicalDescription();

  std::string getGender() const;
  void setGender(const std::string & gender);

  std::string getClothes() const;
  void setClothes(const std::string & clothes);

  std::string getAge() const;
  void setAge(const std::string & age);

  std::string getHairType() const;
  void setHairType(const std::string & hairType);

  std::string getFacialExpression() const;
  void setFacialExpression(const std::string & facialExpression);

  std::string getAccessories() const;
  void setAccessories(const std::string & accessories);

private:
  std::string gender_;
  std::string clothes_;
  std::string age_;
  std::string hairType_;
  std::string facialExpression_;
  std::string accessories_;
};

} // namespace knowledge_manager::domain
