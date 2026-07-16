#include "knowledge_manager/domain/manipulable_object.hpp"

namespace knowledge_manager::domain {

ManipulableObject::ManipulableObject()
    : name_(""), category_(""), ownership_(ObjectOwnership::atLocation("")),
      navigablePose_(std::nullopt), graspPose_(std::nullopt) {}

ManipulableObject::ManipulableObject(std::string name, std::string category,
                                     ObjectOwnership ownership)
    : name_(std::move(name)), category_(std::move(category)),
      ownership_(std::move(ownership)), navigablePose_(std::nullopt),
      graspPose_(std::nullopt) {}

std::string ManipulableObject::getName() const { return name_; }
void ManipulableObject::setName(const std::string &name) { name_ = name; }

std::string ManipulableObject::getCategory() const { return category_; }
void ManipulableObject::setCategory(const std::string &category) {
  category_ = category;
}

ObjectOwnership ManipulableObject::getOwnership() const { return ownership_; }
void ManipulableObject::setOwnership(const ObjectOwnership &ownership) {
  ownership_ = ownership;
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
