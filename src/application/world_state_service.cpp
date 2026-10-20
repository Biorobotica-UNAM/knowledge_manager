#include "knowledge_manager/application/world_state_service.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <stdexcept>
#include <utility>

#include "knowledge_manager/persistence/repository_exception.hpp"

namespace knowledge_manager::application
{

using namespace domain;
using persistence::RepositoryException;

namespace
{

// The only place that converts the repository's exception into the
// service's error channel. Every public method below ends up in one
// call to guarded() or guardedVoid(), so none repeats the try/catch.
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

// -- Positional aliases: "<kind>_<n>" -----------------------------------

std::string categoryAlias(ObjectCategory category)
{
  switch (category) {
    case ObjectCategory::FRUITS:
      return "fruits";
    case ObjectCategory::TRASH:
      return "trash";
    case ObjectCategory::TABLEWARE:
      return "tableware";
    case ObjectCategory::CUTLERY:
      return "cutlery";
    case ObjectCategory::GROCERY:
      return "grocery";
    case ObjectCategory::DRINKS:
      return "drinks";
    case ObjectCategory::SNACKS:
      return "snacks";
    case ObjectCategory::BASKET:
      return "basket";
    case ObjectCategory::FOOD:
      return "food";
    case ObjectCategory::CLEANING_SUPPLY:
      return "cleaning_supply";
    case ObjectCategory::CONTAINERS:
      return "containers";
    case ObjectCategory::CONTAINER_DROP:
      return "container_drop";
    case ObjectCategory::ELECTRODOMESTICS:
      return "electrodomestics";
    case ObjectCategory::TOYS:
      return "toys";
    case ObjectCategory::FABRIC:
      return "fabric";
  }
  throw std::logic_error("Unrecognized ObjectCategory value");
}

std::string personTypeAlias(PersonType type)
{
  switch (type) {
    case PersonType::GUEST:
      return "guest";
    case PersonType::CUSTOMER:
      return "customer";
    case PersonType::HOST:
      return "host";
    case PersonType::USER:
      return "user";
  }
  throw std::logic_error("Unrecognized PersonType value");
}

// Every enumerator, so that an alias can be mapped back to its value.
// When a value is added to ObjectCategory or PersonType, the switch
// above warns (-Wswitch); this list must be extended too.
constexpr std::array<ObjectCategory, 15> kAllCategories{
  ObjectCategory::FRUITS,
  ObjectCategory::TRASH,
  ObjectCategory::TABLEWARE,
  ObjectCategory::CUTLERY,
  ObjectCategory::GROCERY,
  ObjectCategory::DRINKS,
  ObjectCategory::SNACKS,
  ObjectCategory::BASKET,
  ObjectCategory::FOOD,
  ObjectCategory::CLEANING_SUPPLY,
  ObjectCategory::CONTAINERS,
  ObjectCategory::CONTAINER_DROP,
  ObjectCategory::ELECTRODOMESTICS,
  ObjectCategory::TOYS,
  ObjectCategory::FABRIC};

constexpr std::array<PersonType, 4> kAllPersonTypes{
  PersonType::GUEST, PersonType::CUSTOMER, PersonType::HOST,
  PersonType::USER};

struct AliasParts
{
  std::string kind;
  int position;
};

// "fruits_2" -> {"fruits", 2}; anything else -> nullopt. Says nothing
// about whether the kind exists.
std::optional<AliasParts> splitAlias(const std::string & text)
{
  const std::size_t underscore = text.rfind('_');
  if (underscore == std::string::npos || underscore == 0 ||
    underscore + 1 >= text.size())
  {
    return std::nullopt;
  }
  const std::string digits = text.substr(underscore + 1);
  const bool allDigits =
    std::all_of(digits.begin(), digits.end(),
      [](unsigned char c) {return std::isdigit(c) != 0;});
  if (!allDigits || digits.size() > 6) {
    return std::nullopt;
  }
  return AliasParts{text.substr(0, underscore), std::stoi(digits)};
}

struct ObjectAlias
{
  ObjectCategory category;
  int position;
};

struct PersonAlias
{
  PersonType type;
  int position;
};

std::optional<ObjectAlias> asObjectAlias(const std::string & text)
{
  const auto parts = splitAlias(text);
  if (!parts.has_value()) {
    return std::nullopt;
  }
  for (ObjectCategory category : kAllCategories) {
    if (categoryAlias(category) == parts->kind) {
      return ObjectAlias{category, parts->position};
    }
  }
  return std::nullopt;
}

std::optional<PersonAlias> asPersonAlias(const std::string & text)
{
  const auto parts = splitAlias(text);
  if (!parts.has_value()) {
    return std::nullopt;
  }
  for (PersonType type : kAllPersonTypes) {
    if (personTypeAlias(type) == parts->kind) {
      return PersonAlias{type, parts->position};
    }
  }
  return std::nullopt;
}

// Why this name cannot be used for an object or a person, or "".
std::string nameProblem(const std::string & name)
{
  if (name.empty()) {
    return "a name cannot be empty";
  }
  if (asObjectAlias(name).has_value() || asPersonAlias(name).has_value()) {
    return "'" + name +
           "' cannot be used as a name: it looks like a "
           "positional alias";
  }
  return std::string();
}

std::string handText(RobotHand hand)
{
  return hand == RobotHand::LEFT ? "left" : "right";
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

Result<void>
WorldStateService::setRobotLocation(const std::string & locationName)
{
  return guarded<void>([&]() {
             std::optional<Robot> robot = repository_.loadRobot();
             if (!robot.has_value()) {
               return Result<void>::failure("robot not found");
             }
             std::optional<KnownLocation> location =
             repository_.loadKnownLocationByName(locationName);
             if (!location.has_value()) {
               return Result<void>::failure("known location not found: " + locationName);
             }
             robot->setKnownLocation(*location);
             repository_.saveRobot(*robot);
             return Result<void>::success();
  });
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

Result<ManipulableObject>
WorldStateService::resolveObject(const std::string & reference)
{
  if (const auto alias = asObjectAlias(reference)) {
    return getObjectByCategoryAndPosition(alias->category, alias->position);
  }
  return getObjectByName(reference);
}

Result<void> WorldStateService::registerObject(
  const std::string & name,
  ObjectCategory category,
  LocationZone usualZone)
{
  return guarded<void>([&]() {
             const std::string problem = nameProblem(name);
             if (!problem.empty()) {
               return Result<void>::failure(problem);
             }
             std::optional<ManipulableObject> existing =
             repository_.loadObjectByName(name);
             if (existing.has_value()) {
               existing->setCategory(category);
               existing->setUsualZone(usualZone);
               repository_.saveObject(*existing);
             } else {
               repository_.saveObject(ManipulableObject(name, category, usualZone,
                                               ObjectOwnership::unknown()));
             }
             return Result<void>::success();
  });
}

Result<void>
WorldStateService::createOrUpdateObject(const ManipulableObject & object)
{
  return guarded<void>([&]() {
             const std::string problem = findObjectProblem(object);
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

Result<void>
WorldStateService::setObjectAtLocation(
  const std::string & objectReference,
  const std::string & locationName)
{
  return changeObject(objectReference, [&](ManipulableObject & object) {
             object.setOwnership(ObjectOwnership::atLocation(locationName));
  });
}

Result<void>
WorldStateService::setObjectHeldByPerson(
  const std::string & objectReference,
  const std::string & personReference)
{
  const Result<Person> person = resolvePerson(personReference);
  if (person.isFailure()) {
    return Result<void>::failure(person.error());
  }
  const std::string personName = person.value().getName();
  return changeObject(objectReference, [&](ManipulableObject & object) {
             object.setOwnership(ObjectOwnership::heldByPerson(personName));
  });
}

Result<void>
WorldStateService::setObjectHeldByRobot(
  const std::string & objectReference,
  RobotHand hand)
{
  return changeObject(objectReference, [&](ManipulableObject & object) {
             object.setOwnership(ObjectOwnership::heldByRobot(hand));
  });
}

Result<void>
WorldStateService::forgetObjectOwnership(const std::string & objectReference)
{
  return changeObject(objectReference, [](ManipulableObject & object) {
             object.setOwnership(ObjectOwnership::unknown());
  });
}

Result<void>
WorldStateService::setObjectNavigablePose(
  const std::string & objectReference,
  const Pose3D & pose)
{
  return changeObject(objectReference, [&](ManipulableObject & object) {
             object.setNavigablePose(pose);
  });
}

Result<void>
WorldStateService::setObjectGraspPose(
  const std::string & objectReference,
  const Pose3D & pose)
{
  return changeObject(objectReference, [&](ManipulableObject & object) {
             object.setGraspPose(pose);
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

Result<Person> WorldStateService::resolvePerson(const std::string & reference)
{
  if (const auto alias = asPersonAlias(reference)) {
    return getPersonByTypeAndPosition(alias->type, alias->position);
  }
  return getPersonByName(reference);
}

Result<Person>
WorldStateService::resolvePersonWithPhoto(const std::string & reference)
{
  if (const auto alias = asPersonAlias(reference)) {
    return getPersonByTypeAndPositionWithPhoto(alias->type, alias->position);
  }
  return getPersonByNameWithPhoto(reference);
}

Result<void>
WorldStateService::registerPerson(
  const std::string & name, PersonType type,
  const std::string & locationName)
{
  const std::string problem = nameProblem(name);
  if (!problem.empty()) {
    return Result<void>::failure(problem);
  }
  const Result<KnownLocation> location = getKnownLocationByName(locationName);
  if (location.isFailure()) {
    return Result<void>::failure(location.error());
  }
  return guardedVoid([&]() {
             std::optional<Person> existing = repository_.loadPersonByName(name);
             repository_.savePerson(
        existing.has_value() ? rebuiltPerson(*existing, type, location.value()) :
                               Person(name, type, location.value()));
  });
}

Result<void> WorldStateService::createOrUpdatePerson(const Person & person)
{
  return guarded<void>([&]() {
             const std::string problem = findPersonProblem(person);
             if (!problem.empty()) {
               return Result<void>::failure(problem);
             }
             repository_.savePerson(person);
             return Result<void>::success();
  });
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

Result<void>
WorldStateService::setPersonType(
  const std::string & personReference,
  PersonType type)
{
  return changePerson(personReference, [&](const Person & person) {
             return rebuiltPerson(person, type, person.getKnownLocation());
  });
}

Result<void>
WorldStateService::setPersonLocation(
  const std::string & personReference,
  const std::string & locationName)
{
  const Result<KnownLocation> location = getKnownLocationByName(locationName);
  if (location.isFailure()) {
    return Result<void>::failure(location.error());
  }
  return changePerson(personReference, [&](const Person & person) {
             return rebuiltPerson(person, person.getPersonType(), location.value());
  });
}

Result<void>
WorldStateService::setPersonNavigablePose(
  const std::string & personReference,
  const Pose3D & pose)
{
  return changePerson(personReference, [&](const Person & person) {
             Person changed = person;
             changed.setNavigablePose(pose);
             return changed;
  });
}

Result<void> WorldStateService::setPersonDescription(
  const std::string & personReference,
  const PhysicalDescription & description)
{
  return changePerson(personReference, [&](const Person & person) {
             Person changed = person;
             changed.setPhysicalDescription(description);
             return changed;
  });
}

Result<void>
WorldStateService::setPersonPhoto(
  const std::string & personReference,
  const std::vector<std::byte> & photo)
{
  if (photo.empty()) {
    return Result<void>::failure("a photo cannot be empty");
  }
  return changePerson(personReference, [&](const Person & person) {
             Person changed = person;
             changed.setPhoto(photo);
             return changed;
  });
}

Result<void>
WorldStateService::setPersonFavoriteDrink(
  const std::string & personReference,
  const std::string & drink)
{
  return changePerson(personReference, [&](const Person & person) {
             Person changed = person;
             changed.setFavoriteDrink(drink);
             return changed;
  });
}

Result<void> WorldStateService::setPersonOrder(
  const std::string & personReference,
  const std::vector<std::string> & objectReferences)
{
  std::vector<ManipulableObject> objects;
  for (const std::string & reference : objectReferences) {
    Result<ManipulableObject> object = resolveObject(reference);
    if (object.isFailure()) {
      return Result<void>::failure(object.error());
    }
    objects.push_back(std::move(object).value());
  }
  return changePerson(personReference, [&](const Person & person) {
             Person changed = person;
             changed.setOrder(objects);
             return changed;
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
WorldStateService::findObjectProblem(const ManipulableObject & object)
{
  const std::string nameIssue = nameProblem(object.getName());
  if (!nameIssue.empty()) {
    return nameIssue;
  }
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
  if (ownership.isHeldByRobot()) {
    const std::optional<Robot> robot = repository_.loadRobot();
    if (robot.has_value()) {
      const std::optional<ManipulableObject> held =
        robot->getObjectInHand(ownership.getRobotHand());
      if (held.has_value() && held->getName() != object.getName()) {
        return "the " + handText(ownership.getRobotHand()) +
               " hand already holds '" + held->getName() + "'";
      }
    }
  }
  return std::string();
}

std::string WorldStateService::findPersonProblem(const Person & person)
{
  return nameProblem(person.getName());
}

Result<void> WorldStateService::changeObject(
  const std::string & reference,
  const std::function<void(ManipulableObject &)> & change)
{
  Result<ManipulableObject> found = resolveObject(reference);
  if (found.isFailure()) {
    return Result<void>::failure(found.error());
  }
  ManipulableObject object = std::move(found).value();
  change(object);
  return createOrUpdateObject(object);
}

Result<void> WorldStateService::changePerson(
  const std::string & reference,
  const std::function<Person(const Person &)> & change)
{
  const Result<Person> found = resolvePerson(reference);
  if (found.isFailure()) {
    return Result<void>::failure(found.error());
  }
  return createOrUpdatePerson(change(found.value()));
}

Person WorldStateService::rebuiltPerson(
  const Person & person, PersonType type,
  const KnownLocation & location)
{
  Person rebuilt(person.getName(), type, location);
  rebuilt.setPhysicalDescription(person.getPhysicalDescription());
  rebuilt.setFavoriteDrink(person.getFavoriteDrink());
  rebuilt.setOrder(person.getOrder());
  if (person.getNavigablePose().has_value()) {
    rebuilt.setNavigablePose(*person.getNavigablePose());
  }
  if (person.getPhoto().has_value()) {
    rebuilt.setPhoto(*person.getPhoto());
  }
  return rebuilt;
}

} // namespace knowledge_manager::application
