#include "loaders/default_loader.hpp"

DefaultLoader::DefaultLoader(const std::string &filepath)
    : BaseLoader(filepath) {}

std::vector<std::string> DefaultLoader::build() {
  std::vector<std::string> objects{};
  YAML::Node root = YAML::LoadFile(filepath_);

  for (const auto &node : root) {
    std::string key = node.first.as<std::string>();

    for (const auto &val : node.second) {
      objects.push_back(val.as<std::string>());
    }
  }

  std::sort(objects.begin(), objects.end());
  return objects;
}
