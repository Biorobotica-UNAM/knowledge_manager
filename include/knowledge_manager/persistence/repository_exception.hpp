#pragma once

#include <stdexcept>
#include <string>

namespace knowledge_manager::persistence
{

/// Represents an infrastructure-level failure raised by
/// KnowledgeRepository when it cannot complete a persistence
/// operation (e.g. a constraint violation, a connection failure, or a
/// corrupted database file).
///
/// KnowledgeRepository catches the underlying SQLite library's own
/// exception type internally and re-raises it as a
/// RepositoryException, so that callers never need to know which
/// SQLite library sits underneath the repository. This keeps the
/// boundary with the third-party persistence library in a single
/// place: changing that library later only requires changes inside
/// KnowledgeRepository's implementation.
///
/// Reserved for failures a caller cannot meaningfully recover from at
/// the point of the call. Expected, recoverable outcomes such as "no
/// object with that name" are represented as a null object return
/// instead of this exception.
///
/// Inherits from std::exception (a C++ language/library convention,
/// distinct from the underlying SQLite library's own exception type)
/// so that generic handlers such as catch (const std::exception&) can
/// still observe it at higher layers if needed.
class RepositoryException : public std::exception {
public:
  explicit RepositoryException(std::string message)
  : message_(std::move(message)) {}

  /// Returns a human-readable description of the underlying failure,
  /// suitable for logging or for inclusion in a higher-level Result
  /// error message assembled by WorldStateService.
  const char * what() const noexcept override {return message_.c_str();}

private:
  std::string message_;
};

} // namespace knowledge_manager::persistence
