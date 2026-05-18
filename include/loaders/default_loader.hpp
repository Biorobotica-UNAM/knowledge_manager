#pragma once

#include "loaders/base_loader.hpp"

#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

class DefaultLoader : public BaseLoader<std::vector<std::string>> {
public:
  explicit DefaultLoader(const std::string &filepath,
                         const std::string &exclude_suffix = "");
  std::vector<std::string> build() override;

private:
  std::string exclude_suffix_;
};
