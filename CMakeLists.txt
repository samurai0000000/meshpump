# CMakeLists.txt
#
# Copyright (C) 2025, Charles Chiou

cmake_minimum_required(VERSION 3.13)

project(meshpump)

add_subdirectory(libmeshtastic)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -frtti")

set(USE_PIGPIO ON)

include_directories(${CMAKE_CURRENT_LIST_DIR})
add_compile_options(-Wall -Wextra -Werror)
add_compile_options(-g -O2)

add_executable(meshpump meshpump.cxx MeshPump.cxx LedMatrix.cxx)
target_include_directories(meshpump PRIVATE ${MOSQUITTO_INCLUDE_DIR})
target_link_libraries(meshpump PUBLIC libmeshtastic)
target_link_libraries(meshpump PRIVATE ${MOSQUITTO_LIBRARY})
if (USE_PIGPIO)
  target_compile_definitions(meshpump PRIVATE USE_PIGPIO=${USE_PIGPIO})
  target_link_libraries(meshpump PRIVATE pigpiod_if)
endif ()
