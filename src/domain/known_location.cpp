#include "knowledge_manager/domain/known_location.hpp"

namespace knowledge_manager::domain {

KnownLocation::KnownLocation() : name_(""), pose_() {}

KnownLocation::KnownLocation(std::string name, Pose3D pose)
    : name_(std::move(name)), pose_(pose) {}

std::string KnownLocation::getName() const { return name_; }
void KnownLocation::setName(const std::string &name) { name_ = name; }

Pose3D KnownLocation::getPose() const { return pose_; }
void KnownLocation::setPose(const Pose3D &pose) { pose_ = pose; }

} // namespace knowledge_manager::domain
