#include "knowledge_manager/config/object_category.hpp"

#include <algorithm>
#include <yaml-cpp/yaml.h>

namespace knowledge_manager::config {

ObjectCategory::ObjectCategory() : validCategories_() {}

void ObjectCategory::loadFromFile(const std::string &path) {
  validCategories_.clear();

  YAML::Node root = YAML::LoadFile(path);

  // Solo interesan las llaves de priner nivel (drink, fruits, ...)
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
