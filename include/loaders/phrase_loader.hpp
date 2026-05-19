#pragma once

#include <map>
#include <string>
#include <vector>

// Carga todos los archivos YAML de un directorio y construye
// un mapa de: context -> key -> [frases]
// donde context es el nombre del archivo sin extensión (ej: "hri")
class PhraseLoader {
public:
  explicit PhraseLoader(const std::string &phrases_dir);

  // Devuelve { context -> { key -> [phrase, ...] } }
  std::map<std::string, std::map<std::string, std::vector<std::string>>>
  build();

private:
  std::string phrases_dir_;

  void load_file(
      const std::string &filepath, const std::string &context,
      std::map<std::string, std::map<std::string, std::vector<std::string>>>
          &out);
};
