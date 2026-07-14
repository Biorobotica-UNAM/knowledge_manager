#include "knowledge_manager/domain/manipulable_object.hpp"

namespace knowledge_manager::domain {

ManipulableObject::ManipulableObject()
    : name_(""), category_(""), knownLocation_(), navigablePose_(std::nullopt),
      graspPose_(std::nullopt) {}

ManipulableObject::ManipulableObject(std::string name, std::string category,
                                     KnownLocation knownLocation)
    : name_(std::move(name)), category_(std::move(category)),
      knownLocation_(std::move(knownLocation)), navigablePose_(std::nullopt),
      graspPose_(std::nullopt) {}

std::string ManipulableObject::getName() const { return name_; }
void ManipulableObject::setName(const std::string &name) { name_ = name; }

std::string ManipulableObject::getCategory() const { return category_; }
void ManipulableObject::setCategory(const std::string &category) {
  category_ = category;
}

KnownLocation ManipulableObject::getKnownLocation() const {
  return knownLocation_;
}
void ManipulableObject::setKnownLocation(const KnownLocation &knownLocation) {
  knownLocation_ = knownLocation;
}

std::optional<Pose3D> ManipulableObject::getNavigablePose() const {
  return navigablePose_;
}
void ManipulableObject::setNavigablePose(const Pose3D &pose) {
  navigablePose_ = pose;
}

std::optional<Pose3D> ManipulableObject::getGraspPose() const {
  return graspPose_;
}
void ManipulableObject::setGraspPose(const Pose3D &pose) { graspPose_ = pose; }

} // namespace knowledge_manager::domain
