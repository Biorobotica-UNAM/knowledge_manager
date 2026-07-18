#include "knowledge_manager/config/object_category.hpp"

#include <algorithm>
#include <yaml-cpp/yaml.h>

namespace knowledge_manager::config {

ObjectCategory::ObjectCategory() : validCategories_() {}

void ObjectCategory::loadFromFile(const std::string &path) {
  validCategories_.clear();

  YAML::Node root = YAML::LoadFile(path);

  // El archivo .yaml tiene que tener una estructura del tipo:
  // categroia1:
  //    -obj1
  //    -obj2
  // categroia2:
  //    -obj1
  //    -obj2

  // Solo interesan las llaves de primer nivel (drink, fruits, ...)
  for (YAML::const_iterator it = root.begin(); it != root.end(); ++it) {
    validCategories_.push_back(it->first.as<std::string>());
  }
}

bool ObjectCategory::isValid(const std::string &category) const {
  return std::find(validCategories_.begin(), validCategories_.end(),
                   category) != validCategories_.end();
}

std::vector<std::string> ObjectCategory::getValidCategories() const {
  return validCategories_;
}

} // namespace knowledge_manager::config
