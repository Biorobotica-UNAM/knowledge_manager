#pragma once

#include <string>
#include <vector>

namespace knowledge_manager::config
{

class ObjectCategory {
public:
  ObjectCategory();

  void loadFromFile(const std::string & path);
  bool isValid(const std::string & category) const;
  std::vector<std::string> getValidCategories() const;

private:
  std::vector<std::string> validCategories_;
};

} // namespace knowledge_manager::config
