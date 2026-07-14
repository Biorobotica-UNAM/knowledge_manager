#pragma once

#include "knowledge_manager/domain/known_location.hpp"
#include "knowledge_manager/domain/pose3d.hpp"

#include <optional>
#include <string>

namespace knowledge_manager::domain {

class ManipulableObject {
public:
  ManipulableObject();
  ManipulableObject(std::string name, std::string category,
                    KnownLocation knownLocation);

  std::string getName() const;
  void setName(const std::string &name);

  std::string getCategory() const;
  void setCategory(const std::string &category);

  KnownLocation getKnownLocation() const;
  void setKnownLocation(const KnownLocation &knownLocation);

  std::optional<Pose3D> getNavigablePose() const;
  void setNavigablePose(const Pose3D &pose);

  std::optional<Pose3D> getGraspPose() const;
  void setGraspPose(const Pose3D &pose);

private:
  std::string name_;
  std::string category_;
  KnownLocation knownLocation_; // siempre existe -> composición por valor
  std::optional<Pose3D> navigablePose_; // puede no existir -> optional
  std::optional<Pose3D> graspPose_;     // puede no existir -> optional
};

} // namespace knowledge_manager::domain
