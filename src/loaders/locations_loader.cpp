#include "loaders/locations_loader.hpp"

LocationsLoader::LocationsLoader(const std::string &filepath)
    : BaseLoader(filepath) {}

std::vector<std::string> LocationsLoader::build() {
  std::vector<std::string> locations{};
  YAML::Node root = YAML::LoadFile(filepath_);

  for (const auto &node : root) {
    std::string key = node.first.as<std::string>();

    if (key.size() >= 5 && key.substr(key.size() - 5) == "_name")
      continue;

    for (const auto &val : node.second) {
      locations.push_back(val.as<std::string>());
    }
  }

  std::sort(locations.begin(), locations.end());
  return locations;
}
