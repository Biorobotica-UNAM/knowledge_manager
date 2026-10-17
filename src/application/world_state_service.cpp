#include "knowledge_manager/application/world_state_service.hpp"

#include <optional>
#include <utility>

#include "knowledge_manager/persistence/repository_exception.hpp"

namespace knowledge_manager::application
{

using namespace domain;
using persistence::RepositoryException;

namespace
{

// The only place that converts the repository's exception into the
// service's error channel. Every public method below is one call to
// guarded() or guardedVoid(), so none repeats the try/catch.
template<typename T, typename Operation>
Result<T> guarded(Operation operation)
{
  try {
    return operation();
  } catch (const RepositoryException & e) {
    return Result<T>::failure(e.what());
  }
}

template<typename Operation> Result<void> guardedVoid(Operation operation)
{
  return guarded<void>([&]() {
             operation();
             return Result<void>::success();
  });
}

// Repository "not found" (nullopt) -> failed Result.
template<typename T>
Result<T> foundOrFailure(std::optional<T> found, const std::string & message)
{
  if (!found.has_value()) {
    return Result<T>::failure(message);
  }
  return Result<T>::success(std::move(*found));
}

// 1-based positional access over a first-registration-ordered list.
// A position outside 1..size is "not registered yet", reported as a
// failure whose message says so.
template<typename T>
Result<T> atPosition(
  std::vector<T> items, int position,
  const std::string & description)
{
  if (position < 1 || static_cast<std::size_t>(position) > items.size()) {
    return Result<T>::failure("no " + description + " at position " +
                              std::to_string(position) + " (" +
                              std::to_string(items.size()) + " registered)");
  }
  return Result<T>::success(std::move(items[position - 1]));
}

} // namespace

WorldStateService::WorldStateService(
  persistence::KnowledgeRepository & repository)
: repository_(repository) {}

// -- Robot -----------------------------------------------------------

Result<Robot> WorldStateService::getRobot()
{
  return guarded<Robot>([&]() {
             return foundOrFailure(repository_.loadRobot(), "robot not found");
  });
}

Result<void> WorldStateService::updateRobot(const Robot & robot)
{
  return guardedVoid([&]() {repository_.saveRobot(robot);});
}

// -- ManipulableObject -----------------------------------------------

Result<ManipulableObject>
WorldStateService::getObjectByName(const std::string & name)
{
  return guarded<ManipulableObject>([&]() {
             return foundOrFailure(repository_.loadObjectByName(name),
                          "object not found: " + name);
  });
}

Result<std::vector<ManipulableObject>>
WorldStateService::getObjectsByCategory(ObjectCategory category)
{
  return guarded<std::vector<ManipulableObject>>([&]() {
             return Result<std::vector<ManipulableObject>>::success(
        repository_.loadObjectsByCategory(category));
  });
}

Result<ManipulableObject>
WorldStateService::getObjectByCategoryAndPosition(
  ObjectCategory category,
  int position)
{
  return guarded<ManipulableObject>([&]() {
             return atPosition(repository_.loadObjectsByCategory(category), position,
                      "object in this category");
  });
}

Result<void>
WorldStateService::createOrUpdateObject(const ManipulableObject & object)
{
  return guarded<void>([&]() {
             const std::string problem = findOwnershipProblem(object);
             if (!problem.empty()) {
               return Result<void>::failure(problem);
             }
             repository_.saveObject(object);
             return Result<void>::success();
  });
}

Result<void> WorldStateService::deleteObject(const std::string & name)
{
  return guarded<void>([&]() {
             if (!repository_.loadObjectByName(name).has_value()) {
               return Result<void>::failure("object not found: " + name);
             }
             repository_.deleteObject(name);
             return Result<void>::success();
  });
}

// -- Person ----------------------------------------------------------

Result<Person> WorldStateService::getPersonByName(const std::string & name)
{
  return guarded<Person>([&]() {
             return foundOrFailure(repository_.loadPersonByName(name),
                          "person not found: " + name);
  });
}

Result<Person>
WorldStateService::getPersonByNameWithPhoto(const std::string & name)
{
  return guarded<Person>([&]() {
             return foundOrFailure(repository_.loadPersonByNameWithPhoto(name),
                          "person not found: " + name);
  });
}

Result<std::vector<Person>>
WorldStateService::getPersonsByType(PersonType type)
{
  return guarded<std::vector<Person>>([&]() {
             return Result<std::vector<Person>>::success(
        repository_.loadPersonsByType(type));
  });
}

Result<std::vector<Person>>
WorldStateService::getPersonsByTypeWithPhoto(PersonType type)
{
  return guarded<std::vector<Person>>([&]() {
             return Result<std::vector<Person>>::success(
        repository_.loadPersonsByTypeWithPhoto(type));
  });
}

Result<Person> WorldStateService::getPersonByTypeAndPosition(
  PersonType type,
  int position)
{
  return guarded<Person>([&]() {
             return atPosition(repository_.loadPersonsByType(type), position,
                      "person of this type");
  });
}

Result<Person>
WorldStateService::getPersonByTypeAndPositionWithPhoto(
  PersonType type,
  int position)
{
  return guarded<Person>([&]() {
             return atPosition(repository_.loadPersonsByTypeWithPhoto(type), position,
                      "person of this type");
  });
}

Result<void> WorldStateService::createOrUpdatePerson(const Person & person)
{
  return guardedVoid([&]() {repository_.savePerson(person);});
}

Result<void> WorldStateService::deletePerson(const std::string & name)
{
  return guarded<void>([&]() {
             if (!repository_.loadPersonByName(name).has_value()) {
               return Result<void>::failure("person not found: " + name);
             }
             repository_.deletePerson(name);
             return Result<void>::success();
  });
}

// -- KnownLocation ---------------------------------------------------

Result<KnownLocation>
WorldStateService::getKnownLocationByName(const std::string & name)
{
  return guarded<KnownLocation>([&]() {
             return foundOrFailure(repository_.loadKnownLocationByName(name),
                          "known location not found: " + name);
  });
}

Result<std::vector<KnownLocation>> WorldStateService::getAllKnownLocations()
{
  return guarded<std::vector<KnownLocation>>([&]() {
             return Result<std::vector<KnownLocation>>::success(
        repository_.loadAllKnownLocations());
  });
}

Result<std::vector<KnownLocation>>
WorldStateService::getKnownLocationsByZone(LocationZone zone)
{
  return guarded<std::vector<KnownLocation>>([&]() {
             return Result<std::vector<KnownLocation>>::success(
        repository_.loadKnownLocationsByZone(zone));
  });
}

Result<void>
WorldStateService::createOrUpdateKnownLocation(const KnownLocation & location)
{
  return guardedVoid([&]() {repository_.saveKnownLocation(location);});
}

// -- Run state -------------------------------------------------------

Result<void> WorldStateService::resetRuntimeState()
{
  return guardedVoid([&]() {repository_.resetRuntimeState();});
}

// -- Private ---------------------------------------------------------

std::string
WorldStateService::findOwnershipProblem(const ManipulableObject & object)
{
  const ObjectOwnership ownership = object.getOwnership();
  if (ownership.isAtLocation() &&
    !repository_.loadKnownLocationByName(ownership.getLocationName())
    .has_value())
  {
    return "known location not found: " + ownership.getLocationName();
  }
  if (ownership.isHeldByPerson() &&
    !repository_.loadPersonByName(ownership.getPersonName()).has_value())
  {
    return "person not found: " + ownership.getPersonName();
  }
  return std::string();
}

} // namespace knowledge_manager::application
