# CMakeLists.txt
#
# Copyright (C) 2025, Charles Chiou

cmake_minimum_required(VERSION 3.13)

project(meshpump VERSION 2.1.5 LANGUAGES C CXX ASM)

find_path(CONFIG++_INCLUDE_DIR NAMES libconfig.h++)
find_library(CONFIG++_LIBRARY NAMES config++ libconfig++)
if (NOT CONFIG++_INCLUDE_DIR OR NOT CONFIG++_LIBRARY)
  message(FATAL_ERROR "libconfig++ not found")
endif ()

add_subdirectory(libmeshtastic)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -frtti")

if (CMAKE_CXX_FLAGS MATCHES "-fsanitize=address")
  include(CheckCXXSourceRuns)
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_REQUIRED_LINK_OPTIONS "${CMAKE_EXE_LINKER_FLAGS}")
  check_cxx_source_runs("int main() { return 0; }" ASAN_RUNS)
  if (NOT ASAN_RUNS)
    message(WARNING "Target kernel virtual address space does not support AddressSanitizer 64-bit allocator (e.g. 39-bit VMA aarch64). Falling back to -fsanitize=undefined.")
    string(REPLACE "-fsanitize=address,undefined" "-fsanitize=undefined" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-fsanitize=address,undefined" "-fsanitize=undefined" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "-fsanitize=address,undefined" "-fsanitize=undefined" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    string(REPLACE "-fsanitize=address" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-fsanitize=address" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "-fsanitize=address" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
  endif ()
endif ()

find_package(Threads REQUIRED)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shim_inc)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/shim_inc/lgpio.h
  "#ifndef _LGPIO_H\n#include \"LgpioShim.h\"\n#endif\n"
  )

find_path(LGPIO_INCLUDE_DIR NAMES lgpio.h)
find_library(LGPIO_LIBRARY NAMES lgpio)

if (LGPIO_INCLUDE_DIR AND LGPIO_LIBRARY)
  set(MESHPUMP_LGPIO_LIB ${LGPIO_LIBRARY})
  set(MESHPUMP_LGPIO_INC ${LGPIO_INCLUDE_DIR})
  set(MESHPUMP_EXTRA_SRCS "")
else ()
  message(STATUS "liblgpio not found; using host LgpioShim fallback")
  set(MESHPUMP_LGPIO_LIB "")
  set(MESHPUMP_LGPIO_INC ${CMAKE_CURRENT_BINARY_DIR}/shim_inc)
  set(MESHPUMP_EXTRA_SRCS LgpioShim.c)
endif ()

include_directories(${CMAKE_CURRENT_LIST_DIR})
add_compile_options(-Wall -Wextra -Werror -Wno-narrowing)
add_compile_options(-g -O2)

set(WHOAMI_OUTPUT)
execute_process(
  COMMAND whoami
  OUTPUT_VARIABLE WHOAMI_OUTPUT
  RESULT_VARIABLE WHOAMI_RESULT
  ERROR_VARIABLE WHOAMI_ERROR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )
set(HOSTNAME_OUTPUT)
execute_process(
  COMMAND hostname
  OUTPUT_VARIABLE HOSTNAME_OUTPUT
  RESULT_VARIABLE HOSTNAME_RESULT
  ERROR_VARIABLE HOSTNAME_ERROR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )
set(DATE_OUTPUT)
execute_process(
  COMMAND date "+%Y-%m-%d %H:%M:%S"
  OUTPUT_VARIABLE DATE_OUTPUT
  RESULT_VARIABLE DATE_RESULT
  ERROR_VARIABLE DATE_ERROR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  )
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.h
  @ONLY
  )

add_executable(meshpump
  meshpump.cxx
  MeshPump.cxx
  LedMatrix.cxx
  MeshPumpShell.cxx
  ${MESHPUMP_EXTRA_SRCS}
  )
target_include_directories(meshpump PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(meshpump PRIVATE
  ${CONFIG++_INCLUDE_DIR}
  ${MESHPUMP_LGPIO_INC})
target_link_libraries(meshpump PRIVATE
  libmeshtastic
  Threads::Threads
  ${CONFIG++_LIBRARY}
  ${MESHPUMP_LGPIO_LIB})

find_package(PkgConfig REQUIRED)
pkg_check_modules(CPPUTEST REQUIRED cpputest)

set(TEST_SUITE_SRCS
  test/TestMain.cxx
  test/TestLedMatrix.cxx
  test/TestMeshPump.cxx
  MeshPump.cxx
  MeshPumpShell.cxx
  LedMatrix.cxx
  LgpioShim.c
  )

add_executable(test_meshpump_suite ${TEST_SUITE_SRCS})
target_compile_definitions(test_meshpump_suite PRIVATE
  CPPUTEST_MEM_LEAK_DETECTION_DISABLED=1
  MESHPUMP_USE_LGPIO_SHIM=1
  )
target_include_directories(test_meshpump_suite PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}/shim_inc
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/test
  ${CONFIG++_INCLUDE_DIR}
  SYSTEM PRIVATE ${CPPUTEST_INCLUDE_DIRS}
  )
if (CPPUTEST_LIBRARY_DIRS)
  target_link_directories(test_meshpump_suite PRIVATE ${CPPUTEST_LIBRARY_DIRS})
endif ()
target_link_libraries(test_meshpump_suite PRIVATE
  libmeshtastic
  ${CONFIG++_LIBRARY}
  Threads::Threads
  ${CPPUTEST_LIBRARIES}
  )

find_library(RAPIDCHECK_LIBRARY rapidcheck)
if (RAPIDCHECK_LIBRARY)
  add_executable(test_meshpump_fuzz
    test/TestMeshPumpFuzz.cxx
    MeshPump.cxx
    MeshPumpShell.cxx
    LedMatrix.cxx
    LgpioShim.c
    )
  set_property(TARGET test_meshpump_fuzz PROPERTY CXX_STANDARD 14)
  target_compile_definitions(test_meshpump_fuzz PRIVATE
    MESHPUMP_USE_LGPIO_SHIM=1
    )
  target_include_directories(test_meshpump_fuzz PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/shim_inc
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/test
    ${CONFIG++_INCLUDE_DIR}
    )
  target_link_libraries(test_meshpump_fuzz PRIVATE
    libmeshtastic
    ${CONFIG++_LIBRARY}
    ${RAPIDCHECK_LIBRARY}
    Threads::Threads
    )

  add_custom_target(test
    COMMAND test_meshpump_suite -v
    COMMAND test_meshpump_fuzz
    DEPENDS test_meshpump_suite test_meshpump_fuzz
    )
else ()
  add_custom_target(test
    COMMAND test_meshpump_suite -v
    DEPENDS test_meshpump_suite
    )
endif ()
