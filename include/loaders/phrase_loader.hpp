#pragma once

#include <map>
#include <string>
#include <vector>

// Loads all YAML files from a directory and builds a phrase map:
// context -> key -> [phrases]
// where context is the filename stem (e.g., "hri")
class PhraseLoader {
public:
  explicit PhraseLoader(const std::string &phrases_dir);

  using PhraseMap =
      std::map<std::string, std::map<std::string, std::vector<std::string>>>;

  PhraseMap build() const;

private:
  std::string phrases_dir_;

  void load_file(const std::string &filepath, const std::string &context,
                 PhraseMap &out) const;
};
