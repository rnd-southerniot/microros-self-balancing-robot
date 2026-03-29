cmake_minimum_required(VERSION 3.8)
project(sbr_description)

find_package(ament_cmake REQUIRED)

install(DIRECTORY urdf/
  DESTINATION share/${PROJECT_NAME}/urdf
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/meshes/")
  install(DIRECTORY meshes/
    DESTINATION share/${PROJECT_NAME}/meshes
  )
endif()

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
