#include "loaders/default_loader.hpp"

#include <algorithm>

DefaultLoader::DefaultLoader(const std::string &filepath,
                             const std::string &exclude_suffix)
    : BaseLoader(filepath), exclude_suffix_(exclude_suffix) {}

std::vector<std::string> DefaultLoader::build() {
  std::vector<std::string> values;
  YAML::Node root = YAML::LoadFile(filepath_);

  for (const auto &node : root) {
    std::string key = node.first.as<std::string>();

    if (!exclude_suffix_.empty() && key.size() >= exclude_suffix_.size() &&
        key.substr(key.size() - exclude_suffix_.size()) == exclude_suffix_) {
      continue;
    }

    for (const auto &val : node.second) {
      values.push_back(val.as<std::string>());
    }
  }

  std::sort(values.begin(), values.end());
  return values;
}
