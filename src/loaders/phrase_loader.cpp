#include "loaders/phrase_loader.hpp"

#include <filesystem>
#include <yaml-cpp/yaml.h>

namespace fs = std::filesystem;

PhraseLoader::PhraseLoader(const std::string &phrases_dir)
    : phrases_dir_(phrases_dir) {}

std::map<std::string, std::vector<std::string>> PhraseLoader::build() {
  std::map<std::string, std::vector<std::string>> phrase_map;

  for (const auto &entry : fs::directory_iterator(phrases_dir_)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    const std::string ext = entry.path().extension().string();
    if (ext != ".yaml" && ext != ".yml") {
      continue;
    }

    load_file(entry.path().string(), phrase_map);
  }

  return phrase_map;
}

void PhraseLoader::load_file(
    const std::string &filepath,
    std::map<std::string, std::vector<std::string>> &out) {
  YAML::Node root = YAML::LoadFile(filepath);

  for (const auto &node : root) {
    const std::string key = node.first.as<std::string>();

    // Si la clave ya existe en otro archivo, se extiende la lista
    auto &phrases = out[key];
    for (const auto &val : node.second) {
      phrases.push_back(val.as<std::string>());
    }
  }
}
