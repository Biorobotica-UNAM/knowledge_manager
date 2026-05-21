#include "loaders/phrase_loader.hpp"

#include <filesystem>
#include <yaml-cpp/yaml.h>

namespace fs = std::filesystem;

PhraseLoader::PhraseLoader(const std::string &phrases_dir)
    : phrases_dir_(phrases_dir) {}

PhraseLoader::PhraseMap PhraseLoader::build() const {
  PhraseMap phrase_map;

  for (const auto &entry : fs::directory_iterator(phrases_dir_)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    const std::string ext = entry.path().extension().string();
    if (ext != ".yaml" && ext != ".yml") {
      continue;
    }

    const std::string context = entry.path().stem().string();
    load_file(entry.path().string(), context, phrase_map);
  }

  return phrase_map;
}

void PhraseLoader::load_file(const std::string &filepath,
                             const std::string &context, PhraseMap &out) const {
  const YAML::Node root = YAML::LoadFile(filepath);

  for (const auto &node : root) {
    const std::string key = node.first.as<std::string>();
    for (const auto &val : node.second) {
      out[context][key].push_back(val.as<std::string>());
    }
  }
}
