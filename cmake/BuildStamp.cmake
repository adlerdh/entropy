function(BuildStamp)

set(STAMP_HEADER_IN "${CMAKE_SOURCE_DIR}/cmake/BuildStamp.h.in")

if(NOT EXISTS ${STAMP_HEADER_IN})
  message(FATAL_ERROR "Input build stamp header template not found: ${STAMP_HEADER_IN}")
endif()

if(NOT CMAKE_CXX_COMPILER_LOADED)
  message(FATAL_ERROR "There is no C++ compiler loaded")
endif()

find_package(Git QUIET)

if(GIT_FOUND)
  message("Git executable found: ${GIT_EXECUTABLE} (version ${GIT_VERSION_STRING})")
else()
  message(FATAL_ERROR "Git executable not found")
endif()

# Git working branch
execute_process(
  COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE)

# Git commit hash
execute_process(
  COMMAND "${GIT_EXECUTABLE}" describe --match=DoNoTmAtCh --always --abbrev=50 --dirty
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_COMMIT_SHA1
  OUTPUT_STRIP_TRAILING_WHITESPACE)

# Git commit timestamp
execute_process(
  COMMAND "${GIT_EXECUTABLE}" show -s --format=%ci
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_COMMIT_TIMESTAMP
  OUTPUT_STRIP_TRAILING_WHITESPACE)

# Build timestamp
string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%SZ" UTC)

# Build directory
set(BUILD_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

message(STATUS "Project information:")
message(STATUS "  -App name: ${APP_NAME}")
message(STATUS "  -Project name: ${PROJECT_NAME}")
message(STATUS "  -Project vesion: ${PROJECT_VERSION}")
message(STATUS "  -Copyright: ${COPYRIGHT_LINE}")
message(STATUS "  -License: ${LICENSE_LINE}")

message(STATUS "Git commit:")
message(STATUS "  -Branch: ${GIT_BRANCH}")
message(STATUS "  -SHA1: ${GIT_COMMIT_SHA1}")
message(STATUS "  -Timestamp: ${GIT_COMMIT_TIMESTAMP}")

message(STATUS "Compiler:")
message(STATUS "  -Path: ${CMAKE_CXX_COMPILER}")
message(STATUS "  -ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  -Version: ${CMAKE_CXX_COMPILER_VERSION}")

message(STATUS "Build:")
message(STATUS "  -CMake generator: ${CMAKE_GENERATOR}")
message(STATUS "  -CMake version: ${CMAKE_VERSION}")
message(STATUS "  -Timestamp: ${BUILD_TIMESTAMP}")
message(STATUS "  -Directory: ${BUILD_DIRECTORY}")
message(STATUS "  -Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  -Shared libs: ${BUILD_SHARED_LIBS}")

cmake_host_system_information(RESULT HOST_PROCESSOR_NAME QUERY PROCESSOR_NAME)
cmake_host_system_information(RESULT HOST_PROCESSOR_DESCRIPTION QUERY PROCESSOR_DESCRIPTION)
cmake_host_system_information(RESULT HOST_NUMBER_OF_LOGICAL_CORES QUERY NUMBER_OF_LOGICAL_CORES)
cmake_host_system_information(RESULT HOST_NUMBER_OF_PHYSICAL_CORES QUERY NUMBER_OF_PHYSICAL_CORES)
cmake_host_system_information(RESULT HOST_TOTAL_PHYSICAL_MEMORY QUERY TOTAL_PHYSICAL_MEMORY)
cmake_host_system_information(RESULT HOST_IS_64BIT QUERY IS_64BIT)

message(STATUS "Host processor:")
message(STATUS "  -Processor: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
message(STATUS "  -Name: ${HOST_PROCESSOR_NAME}")
message(STATUS "  -Description: ${HOST_PROCESSOR_DESCRIPTION}")
message(STATUS "  -Is 64 bit: ${HOST_IS_64BIT}")
message(STATUS "  -Num. logical cores: ${HOST_NUMBER_OF_LOGICAL_CORES}")
message(STATUS "  -Num. physical cores: ${HOST_NUMBER_OF_PHYSICAL_CORES}")

cmake_host_system_information(RESULT HOST_OS_NAME QUERY OS_NAME)
cmake_host_system_information(RESULT HOST_OS_RELEASE QUERY OS_RELEASE)
cmake_host_system_information(RESULT HOST_OS_PLATFORM QUERY OS_PLATFORM)
cmake_host_system_information(RESULT HOST_OS_VERSION QUERY OS_VERSION)

message(STATUS "Host OS:")
message(STATUS "  -Name: ${HOST_OS_NAME}")
message(STATUS "  -Release: ${HOST_OS_RELEASE}")
message(STATUS "  -Platform: ${HOST_OS_PLATFORM}")
message(STATUS "  -Version: ${HOST_OS_VERSION}")

message(STATUS "Host system:")
message(STATUS "  -Name: ${CMAKE_HOST_SYSTEM_NAME}")
message(STATUS "  -Version: ${CMAKE_HOST_SYSTEM_VERSION}")

message(STATUS "Build stamp header file for ${PROJECT_NAME}: ${CMAKE_CURRENT_BINARY_DIR}/BuildStamp.h")

configure_file(
  ${STAMP_HEADER_IN}
  "${CMAKE_CURRENT_BINARY_DIR}/BuildStamp.h"
  @ONLY)

endfunction()
