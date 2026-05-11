#pragma once

#include "loaders/base_loader.hpp"

#include <algorithm>
#include <yaml-cpp/yaml.h>

class DefaultLoader : public BaseLoader<std::vector<std::string>> {
public:
  explicit DefaultLoader(const std::string &filepath);
  std::vector<std::string> build() override;
};
