#pragma once

#include "knowledge_manager/domain/pose3d.hpp"
#include <string>

namespace knowledge_manager::domain {

class KnownLocation {
public:
  KnownLocation();
  KnownLocation(std::string name, Pose3D pose);

  std::string getName() const;
  void setName(const std::string &name);

  Pose3D getPose() const;
  void setPose(const Pose3D &pose);

private:
  std::string name_;
  Pose3D pose_; // composición: KnownLocation "tiene un" Pose3D, siempre existe
};

} // namespace knowledge_manager::domain
