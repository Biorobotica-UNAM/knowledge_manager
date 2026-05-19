cmake_minimum_required(VERSION 3.8)
project(knowledge_manager)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(knowledge_interfaces REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(ament_index_cpp REQUIRED)

# ── world_state_node ──────────────────────────────────────────────────────────
add_executable(world_state_node
  src/main_world.cpp
  src/knowledge_manager_node.cpp
  src/loaders/default_loader.cpp
  src/loaders/world_loader.cpp
)
target_include_directories(world_state_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>)
target_compile_features(world_state_node PUBLIC c_std_99 cxx_std_17)
ament_target_dependencies(world_state_node
  rclcpp
  knowledge_interfaces
  ament_index_cpp
)
target_link_libraries(world_state_node yaml-cpp)

# ── phrase_manager_node ───────────────────────────────────────────────────────
add_executable(phrase_manager_node
  src/main_phrase.cpp
  src/phrase_manager_node.cpp
  src/loaders/phrase_loader.cpp
)
target_include_directories(phrase_manager_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>)
target_compile_features(phrase_manager_node PUBLIC c_std_99 cxx_std_17)
ament_target_dependencies(phrase_manager_node
  rclcpp
  knowledge_interfaces
  ament_index_cpp
)
target_link_libraries(phrase_manager_node yaml-cpp)

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS
  world_state_node
  phrase_manager_node
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
