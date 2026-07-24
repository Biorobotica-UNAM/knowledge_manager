// src/main.cpp
#include "knowledge_manager/domain/known_location.hpp"
#include "knowledge_manager/domain/pose3d.hpp"

#include <iostream>

int main()
{
  using namespace knowledge_manager::domain;

  Pose3D pose(1.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
  KnownLocation kitchen("kitchen", pose);

  std::cout << "Location: " << kitchen.getName()
            << " x=" << kitchen.getPose().getX() << std::endl;

  return 0;
}
