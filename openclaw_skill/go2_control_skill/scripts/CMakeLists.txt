cmake_minimum_required(VERSION 3.16)
project(go2_openclaw_bridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED UNITREE_SDK2_ROOT)
  set(UNITREE_SDK2_ROOT "/home/radxa/unitree_sdk2/unitree_sdk2-main")
endif()

if(NOT EXISTS "${UNITREE_SDK2_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR "UNITREE_SDK2_ROOT is invalid: ${UNITREE_SDK2_ROOT}")
endif()

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${UNITREE_SDK2_ROOT}" "${CMAKE_BINARY_DIR}/unitree_sdk2")

add_executable(go2_controller go2_controller.cpp)
target_link_libraries(go2_controller PRIVATE unitree_sdk2)

set_target_properties(go2_controller PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
