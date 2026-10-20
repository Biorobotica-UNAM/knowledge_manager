#include "knowledge_manager/ros_adapter/ros_manager.hpp"

#include <cstddef>
#include <exception>
#include <utility>

#include "knowledge_interfaces/srv/delete_object.hpp"
#include "knowledge_interfaces/srv/delete_person.hpp"
#include "knowledge_interfaces/srv/forget_object_ownership.hpp"
#include "knowledge_interfaces/srv/get_all_known_locations.hpp"
#include "knowledge_interfaces/srv/get_known_location.hpp"
#include "knowledge_interfaces/srv/get_known_locations_by_zone.hpp"
#include "knowledge_interfaces/srv/get_object.hpp"
#include "knowledge_interfaces/srv/get_objects_by_category.hpp"
#include "knowledge_interfaces/srv/get_person.hpp"
#include "knowledge_interfaces/srv/get_person_with_photo.hpp"
#include "knowledge_interfaces/srv/get_persons_by_type.hpp"
#include "knowledge_interfaces/srv/get_persons_by_type_with_photo.hpp"
#include "knowledge_interfaces/srv/get_robot.hpp"
#include "knowledge_interfaces/srv/register_object.hpp"
#include "knowledge_interfaces/srv/register_person.hpp"
#include "knowledge_interfaces/srv/reset_runtime_state.hpp"
#include "knowledge_interfaces/srv/save_known_location.hpp"
#include "knowledge_interfaces/srv/set_object_at_location.hpp"
#include "knowledge_interfaces/srv/set_object_grasp_pose.hpp"
#include "knowledge_interfaces/srv/set_object_held_by_person.hpp"
#include "knowledge_interfaces/srv/set_object_held_by_robot.hpp"
#include "knowledge_interfaces/srv/set_object_navigable_pose.hpp"
#include "knowledge_interfaces/srv/set_person_description.hpp"
#include "knowledge_interfaces/srv/set_person_favorite_drink.hpp"
#include "knowledge_interfaces/srv/set_person_location.hpp"
#include "knowledge_interfaces/srv/set_person_navigable_pose.hpp"
#include "knowledge_interfaces/srv/set_person_order.hpp"
#include "knowledge_interfaces/srv/set_person_photo.hpp"
#include "knowledge_interfaces/srv/set_person_type.hpp"
#include "knowledge_interfaces/srv/set_robot_location.hpp"

#include "knowledge_manager/domain/known_location.hpp"
#include "knowledge_manager/ros_adapter/message_conversions.hpp"

namespace knowledge_manager::ros_adapter
{

namespace srv = knowledge_interfaces::srv;
using application::Result;

namespace
{

// Prefix of every service name.
const char kServicePrefix[] = "knowledge_manager/";

// Turns a Result carrying data into the Result<void> that
// handleServiceCall expects, handing the data to `fill` so it can be
// written into the response. `fill` only runs on success.
template<typename T, typename Fill>
Result<void> deliver(const Result<T> & result, Fill fill)
{
  if (result.isFailure()) {
    return Result<void>::failure(result.error());
  }
  fill(result.value());
  return Result<void>::success();
}

} // namespace

ROSManager::ROSManager(
  rclcpp::Node::SharedPtr node,
  application::WorldStateService & world,
  persistence::Logger & logger)
: node_(std::move(node)), world_(world), logger_(logger) {}

Result<void> ROSManager::initialize()
{
  if (!services_.empty()) {
    return Result<void>::failure("ROSManager is already initialized");
  }

  const Result<void> reset = world_.resetRuntimeState();
  if (reset.isFailure()) {
    logger_.error("the run state could not be reset: " + reset.error());
    return reset;
  }

  advertiseQueries();
  advertiseKnowledgeCommands();
  advertiseObjectStateCommands();
  advertiseRobotAndPersonStateCommands();
  advertiseAdministration();

  logger_.info("knowledge base ready, " + std::to_string(services_.size()) +
               " services advertised");
  return Result<void>::success();
}

template<typename Srv, typename Operation>
void ROSManager::serve(const std::string & name, Operation operation)
{
  services_.push_back(node_->create_service<Srv>(
      std::string(kServicePrefix) + name,
      [this, operation](const std::shared_ptr<typename Srv::Request> request,
      std::shared_ptr<typename Srv::Response> response) {
        handleServiceCall(*request, *response, operation);
      }));
}

template<typename Request, typename Response, typename Operation>
void ROSManager::handleServiceCall(
  const Request & request, Response & response,
  Operation operation)
{
  Result<void> outcome = Result<void>::success();
  try {
    outcome = operation(request, response);
  } catch (const std::exception & e) {
    // Not an expected outcome (those come back as failed Results):
    // a malformed request, such as an enum value nobody defined, or a
    // bug. It must not reach the executor and take the node down.
    logger_.error(std::string("service call raised: ") + e.what());
    outcome = Result<void>::failure(e.what());
  }

  response.success = outcome.isSuccess();
  response.error = outcome.isSuccess() ? std::string() : outcome.error();
  if (outcome.isFailure()) {
    logger_.info("service call failed: " + outcome.error());
  }
}

// -- Queries -----------------------------------------------------------

void ROSManager::advertiseQueries()
{
  serve<srv::GetRobot>("get_robot", [this](const auto &, auto & response) {
      return deliver(world_.getRobot(), [&](const domain::Robot & robot) {
               response.robot = toMessage(robot);
    });
  });

  serve<srv::GetObject>(
      "get_object", [this](const auto & request, auto & response) {
      return deliver(world_.resolveObject(request.reference),
             [&](const domain::ManipulableObject & object) {
               response.object = toMessage(object);
                       });
      });

  serve<srv::GetObjectsByCategory>(
      "get_objects_by_category", [this](const auto & request, auto & response) {
      return deliver(
            world_.getObjectsByCategory(fromMessage(request.category)),
        [&](const std::vector<domain::ManipulableObject> & objects) {
          response.objects = toMessages(objects);
            });
      });

  serve<srv::GetPerson>(
      "get_person", [this](const auto & request, auto & response) {
      return deliver(world_.resolvePerson(request.reference),
             [&](const domain::Person & person) {
               response.person = toMessage(person);
                       });
      });

  serve<srv::GetPersonWithPhoto>(
      "get_person_with_photo", [this](const auto & request, auto & response) {
      return deliver(world_.resolvePersonWithPhoto(request.reference),
             [&](const domain::Person & person) {
               response.person = toMessage(person);
                       });
      });

  serve<srv::GetPersonsByType>(
      "get_persons_by_type", [this](const auto & request, auto & response) {
      return deliver(world_.getPersonsByType(fromMessage(request.type)),
             [&](const std::vector<domain::Person> & persons) {
               response.persons = toMessages(persons);
                       });
      });

  serve<srv::GetPersonsByTypeWithPhoto>(
      "get_persons_by_type_with_photo",
    [this](const auto & request, auto & response) {
      return deliver(
            world_.getPersonsByTypeWithPhoto(fromMessage(request.type)),
        [&](const std::vector<domain::Person> & persons) {
          response.persons = toMessages(persons);
            });
      });

  serve<srv::GetKnownLocation>(
      "get_known_location", [this](const auto & request, auto & response) {
      return deliver(world_.getKnownLocationByName(request.name),
             [&](const domain::KnownLocation & location) {
               response.location = toMessage(location);
                       });
      });

  serve<srv::GetAllKnownLocations>(
      "get_all_known_locations", [this](const auto &, auto & response) {
      return deliver(
            world_.getAllKnownLocations(),
        [&](const std::vector<domain::KnownLocation> & locations) {
          response.locations = toMessages(locations);
            });
      });

  serve<srv::GetKnownLocationsByZone>(
      "get_known_locations_by_zone",
    [this](const auto & request, auto & response) {
      return deliver(
            world_.getKnownLocationsByZone(fromMessage(request.zone)),
        [&](const std::vector<domain::KnownLocation> & locations) {
          response.locations = toMessages(locations);
            });
      });
}

// -- Knowledge: what survives a restart ----------------------------------

void ROSManager::advertiseKnowledgeCommands()
{
  serve<srv::SaveKnownLocation>(
      "save_known_location", [this](const auto & request, auto &) {
      return world_.createOrUpdateKnownLocation(
            domain::KnownLocation(request.name, fromMessage(request.pose),
                                  fromMessage(request.zone)));
      });

  serve<srv::RegisterObject>("register_object", [this](const auto & request,
    auto &) {
      return world_.registerObject(request.name, fromMessage(request.category),
                                 fromMessage(request.usual_zone));
  });

  serve<srv::DeleteObject>("delete_object",
    [this](const auto & request, auto &) {
      return world_.deleteObject(request.name);
                           });

  serve<srv::RegisterPerson>(
      "register_person", [this](const auto & request, auto &) {
      return world_.registerPerson(request.name, fromMessage(request.type),
                                     request.location_name);
      });

  serve<srv::DeletePerson>("delete_person",
    [this](const auto & request, auto &) {
      return world_.deletePerson(request.name);
                           });
}

// -- Run state of objects --------------------------------------------------

void ROSManager::advertiseObjectStateCommands()
{
  serve<srv::SetObjectAtLocation>("set_object_at_location",
    [this](const auto & request, auto &) {
      return world_.setObjectAtLocation(
                                        request.object, request.location_name);
                                  });

  serve<srv::SetObjectHeldByPerson>(
      "set_object_held_by_person", [this](const auto & request, auto &) {
      return world_.setObjectHeldByPerson(request.object, request.person);
      });

  serve<srv::SetObjectHeldByRobot>(
      "set_object_held_by_robot", [this](const auto & request, auto &) {
      return world_.setObjectHeldByRobot(request.object,
                                           fromMessage(request.hand));
      });

  serve<srv::ForgetObjectOwnership>(
      "forget_object_ownership", [this](const auto & request, auto &) {
      return world_.forgetObjectOwnership(request.object);
      });

  serve<srv::SetObjectNavigablePose>(
      "set_object_navigable_pose", [this](const auto & request, auto &) {
      return world_.setObjectNavigablePose(request.object,
                                             fromMessage(request.pose));
      });

  serve<srv::SetObjectGraspPose>(
      "set_object_grasp_pose", [this](const auto & request, auto &) {
      return world_.setObjectGraspPose(request.object,
                                         fromMessage(request.pose));
      });
}

// -- Run state of the robot and of persons -----------------------------------

void ROSManager::advertiseRobotAndPersonStateCommands()
{
  serve<srv::SetRobotLocation>(
      "set_robot_location", [this](const auto & request, auto &) {
      return world_.setRobotLocation(request.location_name);
      });

  serve<srv::SetPersonType>(
      "set_person_type", [this](const auto & request, auto &) {
      return world_.setPersonType(request.person, fromMessage(request.type));
      });

  serve<srv::SetPersonLocation>(
      "set_person_location", [this](const auto & request, auto &) {
      return world_.setPersonLocation(request.person, request.location_name);
      });

  serve<srv::SetPersonNavigablePose>(
      "set_person_navigable_pose", [this](const auto & request, auto &) {
      return world_.setPersonNavigablePose(request.person,
                                             fromMessage(request.pose));
      });

  serve<srv::SetPersonDescription>(
      "set_person_description", [this](const auto & request, auto &) {
      return world_.setPersonDescription(request.person,
                                           fromMessage(request.description));
      });

  serve<srv::SetPersonPhoto>(
      "set_person_photo", [this](const auto & request, auto &) {
      std::vector<std::byte> photo;
      photo.reserve(request.photo.size());
      for (const auto byte : request.photo) {
        photo.push_back(static_cast<std::byte>(byte));
      }
      return world_.setPersonPhoto(request.person, photo);
      });

  serve<srv::SetPersonFavoriteDrink>(
      "set_person_favorite_drink", [this](const auto & request, auto &) {
      return world_.setPersonFavoriteDrink(request.person, request.drink);
      });

  serve<srv::SetPersonOrder>(
      "set_person_order", [this](const auto & request, auto &) {
      return world_.setPersonOrder(request.person, request.objects);
      });
}

// -- Administration
// ------------------------------------------------------------

void ROSManager::advertiseAdministration()
{
  // Must not be called while a plan is running: it forgets where every
  // object is and removes every person who is not a host.
  serve<srv::ResetRuntimeState>(
      "reset_runtime_state",
    [this](const auto &, auto &) {return world_.resetRuntimeState();});
}

} // namespace knowledge_manager::ros_adapter
