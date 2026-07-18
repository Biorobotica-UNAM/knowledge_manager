#pragma once

#include <string>

namespace knowledge_manager::persistence {

/// Abstract logging interface owned by this package.
///
/// KnowledgeRepository needs to report infrastructure-level events
/// (e.g. a SQLite constraint violation) for diagnosis, but must not
/// depend on any specific logging backend (in particular, not on
/// rclcpp) -- doing so would make the repository, and everything that
/// composes it, untestable without a running ROS2 context.
///
/// Concrete subclasses decide where messages actually go:
///   - RclcppLogger (outside this package) forwards to RCLCPP_* macros.
///   - ConsoleLogger (test-only) writes to stdout/stderr.
class Logger {
public:
  virtual ~Logger() = default;

  /// Reports a message describing a failure that likely requires
  /// attention (e.g. a repository operation that could not complete
  /// due to an infrastructure fault).
  virtual void error(const std::string &message) = 0;

  /// Reports a message describing a recoverable or non-critical
  /// anomaly.
  virtual void warn(const std::string &message) = 0;

  /// Reports a message describing a normal, expected event worth
  /// recording (e.g. successful startup).
  virtual void info(const std::string &message) = 0;
};

} // namespace knowledge_manager::persistence
