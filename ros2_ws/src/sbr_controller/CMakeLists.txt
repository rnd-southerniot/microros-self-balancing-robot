cmake_minimum_required(VERSION 3.8)
project(sbr_controller)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(sbr_interfaces REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)

set(DEPS
  rclcpp
  std_msgs
  geometry_msgs
  sensor_msgs
  nav_msgs
  sbr_interfaces
  tf2
  tf2_ros
)

# ── safety_node ──────────────────────────────────────────────────
add_executable(safety_node src/safety_node.cpp)
ament_target_dependencies(safety_node ${DEPS})

# ── telemetry_node ───────────────────────────────────────────────
add_executable(telemetry_node src/telemetry_node.cpp)
ament_target_dependencies(telemetry_node ${DEPS})

# ── Install ──────────────────────────────────────────────────────
install(TARGETS
  safety_node
  telemetry_node
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
