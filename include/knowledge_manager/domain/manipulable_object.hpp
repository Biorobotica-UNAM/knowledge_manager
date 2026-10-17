#pragma once

#include "knowledge_manager/domain/location_zone.hpp"
#include "knowledge_manager/domain/object_category.hpp"
#include "knowledge_manager/domain/object_ownership.hpp"
#include "knowledge_manager/domain/pose3d.hpp"

#include <optional>
#include <string>

namespace knowledge_manager::domain
{

class ManipulableObject {
public:
  ManipulableObject(
    std::string name, ObjectCategory category,
    LocationZone usualZone, ObjectOwnership ownership);

  std::string getName() const;
  void setName(const std::string & name);

  ObjectCategory getCategory() const;
  void setCategory(ObjectCategory category);

  LocationZone getUsualZone() const;
  void setUsualZone(LocationZone zone);

  ObjectOwnership getOwnership() const;
  void setOwnership(const ObjectOwnership & ownership);

  std::optional<Pose3D> getNavigablePose() const;
  void setNavigablePose(const Pose3D & pose);

  std::optional<Pose3D> getGraspPose() const;
  void setGraspPose(const Pose3D & pose);

private:
  // Knowledge: stable, persists across restarts.
  std::string name_;
  ObjectCategory category_;
  LocationZone usualZone_;

  // Run state: unknown at the start of every run.
  ObjectOwnership ownership_;
  std::optional<Pose3D> navigablePose_;
  std::optional<Pose3D> graspPose_;
};

} // namespace knowledge_manager::domain
