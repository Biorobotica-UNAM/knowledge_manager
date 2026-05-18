#pragma once

#include <map>
#include <string>
#include <vector>

// Carga todos los archivos YAML de un directorio y construye
// un mapa de: clave -> lista de frases
class PhraseLoader {
public:
  explicit PhraseLoader(const std::string &phrases_dir);

  // Devuelve un mapa completo { key -> [phrase, ...] }
  std::map<std::string, std::vector<std::string>> build();

private:
  std::string phrases_dir_;

  void load_file(const std::string &filepath,
                 std::map<std::string, std::vector<std::string>> &out);
};
