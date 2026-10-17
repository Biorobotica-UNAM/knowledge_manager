#include "knowledge_manager/domain/manipulable_object.hpp"

#include <utility>

namespace knowledge_manager::domain
{

ManipulableObject::ManipulableObject(
  std::string name, ObjectCategory category,
  LocationZone usualZone,
  ObjectOwnership ownership)
: name_(std::move(name)), category_(category), usualZone_(usualZone),
  ownership_(std::move(ownership)) {}

std::string ManipulableObject::getName() const {return name_;}
void ManipulableObject::setName(const std::string & name) {name_ = name;}

ObjectCategory ManipulableObject::getCategory() const {return category_;}
void ManipulableObject::setCategory(ObjectCategory category)
{
  category_ = category;
}

LocationZone ManipulableObject::getUsualZone() const {return usualZone_;}
void ManipulableObject::setUsualZone(LocationZone zone) {usualZone_ = zone;}

ObjectOwnership ManipulableObject::getOwnership() const {return ownership_;}
void ManipulableObject::setOwnership(const ObjectOwnership & ownership)
{
  ownership_ = ownership;
}

std::optional<Pose3D> ManipulableObject::getNavigablePose() const
{
  return navigablePose_;
}
void ManipulableObject::setNavigablePose(const Pose3D & pose)
{
  navigablePose_ = pose;
}

std::optional<Pose3D> ManipulableObject::getGraspPose() const
{
  return graspPose_;
}
void ManipulableObject::setGraspPose(const Pose3D & pose) {graspPose_ = pose;}

} // namespace knowledge_manager::domain
