#pragma once

#include <optional>
#include <string>
#include <utility>

namespace knowledge_manager::application
{

/// Outcome of a WorldStateService operation: either the requested
/// value or an error message, never both and never neither.
///
/// Instances are created only through the named constructors
/// success() and failure(), so the two states cannot be mixed up
/// (same idiom as domain::ObjectOwnership).
///
/// value() has a precondition: isSuccess() must be true. Calling it on
/// a failure throws std::bad_optional_access; that is a programming
/// error in the caller, not an expected outcome. error() likewise
/// requires isFailure().
template<typename T>
class Result {
public:
  static Result success(T value)
  {
    return Result(std::move(value), std::string());
  }

  static Result failure(std::string message)
  {
    return Result(std::nullopt, std::move(message));
  }

  bool isSuccess() const {return value_.has_value();}
  bool isFailure() const {return !isSuccess();}

  const T & value() const & {return value_.value();}
  T & value() & {return value_.value();}
  T && value() && {return std::move(value_).value();}

  const std::string & error() const {return error_;}

private:
  Result(std::optional<T> value, std::string error)
  : value_(std::move(value)), error_(std::move(error)) {}

  std::optional<T> value_;
  std::string error_;
};

/// Result of an operation that returns no data (e.g. a save or a
/// delete): it only says whether it worked.
template<>
class Result<void> {
public:
  static Result success() {return Result(true, std::string());}

  static Result failure(std::string message)
  {
    return Result(false, std::move(message));
  }

  bool isSuccess() const {return success_;}
  bool isFailure() const {return !success_;}

  const std::string & error() const {return error_;}

private:
  Result(bool success, std::string error)
  : success_(success), error_(std::move(error)) {}

  bool success_;
  std::string error_;
};

} // namespace knowledge_manager::application
