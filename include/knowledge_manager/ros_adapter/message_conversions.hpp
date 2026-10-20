#pragma once

#include <utility>
#include <vector>

#include "geometry_msgs/msg/pose.hpp"
#include "knowledge_interfaces/msg/known_location.hpp"
#include "knowledge_interfaces/msg/location_zone.hpp"
#include "knowledge_interfaces/msg/manipulable_object.hpp"
#include "knowledge_interfaces/msg/object_category.hpp"
#include "knowledge_interfaces/msg/object_ownership.hpp"
#include "knowledge_interfaces/msg/person.hpp"
#include "knowledge_interfaces/msg/person_type.hpp"
#include "knowledge_interfaces/msg/physical_description.hpp"
#include "knowledge_interfaces/msg/robot.hpp"
#include "knowledge_interfaces/msg/robot_hand.hpp"

#include "knowledge_manager/domain/known_location.hpp"
#include "knowledge_manager/domain/location_zone.hpp"
#include "knowledge_manager/domain/manipulable_object.hpp"
#include "knowledge_manager/domain/object_category.hpp"
#include "knowledge_manager/domain/object_ownership.hpp"
#include "knowledge_manager/domain/person.hpp"
#include "knowledge_manager/domain/person_type.hpp"
#include "knowledge_manager/domain/physical_description.hpp"
#include "knowledge_manager/domain/pose3d.hpp"
#include "knowledge_manager/domain/robot.hpp"
#include "knowledge_manager/domain/robot_hand.hpp"

/// The only place that knows both the domain classes and the ROS
/// messages that mirror them. The domain never depends on ROS; the
/// messages never depend on the domain; this file translates between
/// the two (Adapter, Design Patterns).
///
/// Direction domain -> message (toMessage) exists for everything a
/// service returns. Direction message -> domain (fromMessage) exists
/// only for what a service receives: poses, enums and descriptions.
///
/// fromMessage throws std::invalid_argument when an enum value is not
/// one of the defined constants (a client sent 99, say). ROSManager
/// turns that into a failed response.
namespace knowledge_manager::ros_adapter
{

namespace interfaces = knowledge_interfaces;

// -- domain -> message ---------------------------------------------------
geometry_msgs::msg::Pose toMessage(const domain::Pose3D & pose);
interfaces::msg::LocationZone toMessage(domain::LocationZone zone);
interfaces::msg::ObjectCategory toMessage(domain::ObjectCategory category);
interfaces::msg::PersonType toMessage(domain::PersonType type);
interfaces::msg::RobotHand toMessage(domain::RobotHand hand);
interfaces::msg::ObjectOwnership
toMessage(const domain::ObjectOwnership & ownership);
interfaces::msg::KnownLocation toMessage(const domain::KnownLocation & location);
interfaces::msg::PhysicalDescription
toMessage(const domain::PhysicalDescription & description);
interfaces::msg::ManipulableObject
toMessage(const domain::ManipulableObject & object);
interfaces::msg::Person toMessage(const domain::Person & person);
interfaces::msg::Robot toMessage(const domain::Robot & robot);

/// toMessage applied to every element, in order.
template<typename T> auto toMessages(const std::vector<T> & items)
{
  std::vector<decltype(toMessage(std::declval<const T &>()))> messages;
  messages.reserve(items.size());
  for (const T & item : items) {
    messages.push_back(toMessage(item));
  }
  return messages;
}

// -- message -> domain ---------------------------------------------------
domain::Pose3D fromMessage(const geometry_msgs::msg::Pose & pose);
domain::LocationZone fromMessage(const interfaces::msg::LocationZone & zone);
domain::ObjectCategory
fromMessage(const interfaces::msg::ObjectCategory & category);
domain::PersonType fromMessage(const interfaces::msg::PersonType & type);
domain::RobotHand fromMessage(const interfaces::msg::RobotHand & hand);
domain::PhysicalDescription
fromMessage(const interfaces::msg::PhysicalDescription & description);

} // namespace knowledge_manager::ros_adapter
