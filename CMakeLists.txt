cmake_minimum_required(VERSION 3.8)
project(knowledge_manager)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(ament_index_cpp REQUIRED)
 find_package(knowledge_interfaces REQUIRED)  # <- descomenta cuando exista ese paquete
# find_package(yaml-cpp REQUIRED)              # <- descomenta cuando uses InitialStateLoader/CategoryCatalog

# ── world_state_node ────────────────────────────────────────────────

set(DOMAIN_SOURCES
  src/domain/known_location.cpp
  src/domain/manipulable_object.cpp
  src/domain/person.cpp
  src/domain/physical_description.cpp
  src/domain/pose3d.cpp
  src/domain/robot.cpp
)

add_executable(world_state_node
  src/main.cpp
  ${DOMAIN_SOURCES}
)

target_include_directories(world_state_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
)

target_compile_features(world_state_node PUBLIC cxx_std_17)

ament_target_dependencies(world_state_node
  rclcpp
  ament_index_cpp
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

# ── Testing ──────────────────────────────────────────────────────────
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
