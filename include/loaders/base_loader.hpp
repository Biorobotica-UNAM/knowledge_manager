#pragma once

#include <string>
#include <vector>

template <typename T>

class BaseLoader {
public:
  explicit BaseLoader(const std::string &filepath) : filepath_(filepath) {};
  virtual ~BaseLoader() = default;

  virtual T build() = 0;

protected:
  std::string filepath_;
};
