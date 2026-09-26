cmake_minimum_required(VERSION 3.8)
project(knowledge_manager)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(knowledge_interfaces REQUIRED)
find_package(yaml-cpp REQUIRED)

# ── SQLiteCpp (KnowledgeRepository's persistence backend) ───────────
# Not available as an apt package (only the raw C API, libsqlite3-dev,
# is), so it is fetched and built as part of this project instead of
# requiring every teammate/CI machine to install it manually.
include(FetchContent)
fetchcontent_declare(
  SQLiteCpp
  GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
  GIT_TAG        3.3.1
  GIT_SUBMODULES sqlite3
)
# We only need the library itself, not SQLiteCpp's own tests/examples.
set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE)
set(SQLITECPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SQLITECPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
fetchcontent_makeavailable(SQLiteCpp)

# ── knowledge_manager_core (library, shared by the node and tests) ──
# Domain, config, and persistence sources are built once as a static
# library instead of being compiled directly into world_state_node.
# This lets test executables link against the exact same object code
# without also pulling in main.cpp or duplicating the compile step --
# a test should be able to isolate the piece it exercises (e.g.
# KnowledgeRepository) without depending on the whole node.
set(DOMAIN_SOURCES
  src/domain/known_location.cpp
  src/domain/manipulable_object.cpp
  src/domain/object_ownership.cpp
  src/domain/person.cpp
  src/domain/physical_description.cpp
  src/domain/pose3d.cpp
  src/domain/robot.cpp
)

# set(CONFIG_SOURCES
# )

set(PERSISTENCE_SOURCES
  src/persistence/knowledge_repository.cpp
)

add_library(knowledge_manager_core
  ${DOMAIN_SOURCES}
  # ${CONFIG_SOURCES}
  ${PERSISTENCE_SOURCES}
)

target_include_directories(knowledge_manager_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
)

target_compile_features(knowledge_manager_core PUBLIC cxx_std_17)

ament_target_dependencies(knowledge_manager_core
  ament_index_cpp
  yaml-cpp
)

target_link_libraries(knowledge_manager_core
  yaml-cpp
  SQLiteCpp
)

# ── world_state_node ────────────────────────────────────────────────
add_executable(world_state_node
  src/main.cpp
)

target_compile_features(world_state_node PUBLIC cxx_std_17)

ament_target_dependencies(world_state_node
  rclcpp
)

target_link_libraries(world_state_node
  knowledge_manager_core
)

# ── Install ──────────────────────────────────────────────────────────
install(TARGETS
  world_state_node
  DESTINATION lib/${PROJECT_NAME}
)

install(
  DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

# schema.sql needs to be installed alongside the executable so
# KnowledgeRepository can be given a real filesystem path to it at
# runtime (ROSManager::initialize resolves this path, typically via
# ament_index_cpp::get_package_share_directory).
install(FILES schema.sql
  DESTINATION share/${PROJECT_NAME}
)

# ── Testing ──────────────────────────────────────────────────────────
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()

  # ament_add_gtest compiles the given source as its own executable,
  # linked against GoogleTest (which provides its own main()), and
  # registers it with CTest so `colcon test` discovers and runs it
  # automatically. Each call declares ONE test executable; add another
  # ament_add_gtest(...) block per new test file as the suite grows.
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_knowledge_repository
    test/test_knowledge_repository.cpp
  )
  target_link_libraries(test_knowledge_repository
    knowledge_manager_core
  )
  # Gives the test an absolute, always-correct path to schema.sql,
  # regardless of the working directory ctest/colcon test invokes the
  # binary from.
  target_compile_definitions(test_knowledge_repository PRIVATE
    KNOWLEDGE_MANAGER_SCHEMA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/schema.sql"
  )
endif()

ament_package()
