cmake_minimum_required(VERSION 3.8)
project(knowledge_manager)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ── Dependencies ──────────────────────────────────────────────────────────────

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(knowledge_interfaces REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(std_srvs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(ament_index_cpp REQUIRED)

# yaml-cpp ships as yaml_cpp_vendor in ROS2 distributions.
# If your distro exposes it directly, "yaml-cpp" also works.
find_package(yaml_cpp_vendor REQUIRED)

# ── Target ────────────────────────────────────────────────────────────────────

add_executable(knowledge_manager_node
  src/main.cpp
  src/knowledge_manager_node.cpp
  src/world_model.cpp
  src/yaml_loader.cpp
  src/yaml_persistence.cpp
)

target_include_directories(knowledge_manager_node
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

ament_target_dependencies(knowledge_manager_node
  rclcpp
  std_srvs
  std_msgs
  nlohmann_json
  knowledge_interfaces
  ament_index_cpp
  yaml_cpp_vendor
)

# yaml-cpp needs an explicit link — ament_target_dependencies does not
# handle it automatically for vendor packages.
target_link_libraries(knowledge_manager_node
  yaml-cpp
)

# ── Install ───────────────────────────────────────────────────────────────────

install(TARGETS
  knowledge_manager_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include
)

ament_package()
