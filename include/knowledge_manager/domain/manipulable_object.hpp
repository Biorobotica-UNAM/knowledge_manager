#pragma once

#include "knowledge_manager/domain/object_ownership.hpp"
#include "knowledge_manager/domain/pose3d.hpp"

#include <optional>
#include <string>

namespace knowledge_manager::domain
{

class ManipulableObject {
public:
  ManipulableObject();
  ManipulableObject(
    std::string name, std::string category,
    ObjectOwnership ownership);

  std::string getName() const;
  void setName(const std::string & name);

  std::string getCategory() const;
  void setCategory(const std::string & category);

  ObjectOwnership getOwnership() const;
  void setOwnership(const ObjectOwnership & ownership);

  std::optional<Pose3D> getNavigablePose() const;
  void setNavigablePose(const Pose3D & pose);

  std::optional<Pose3D> getGraspPose() const;
  void setGraspPose(const Pose3D & pose);

private:
  std::string name_;
  std::string category_;
  ObjectOwnership ownership_; // siempre existe -> composición por valor
  std::optional<Pose3D> navigablePose_; // puede no existir -> optional
  std::optional<Pose3D> graspPose_;     // puede no existir -> optional
};

} // namespace knowledge_manager::domain
