cmake_minimum_required(VERSION 3.28)

# This directory can also be configured without Entropy or its image/graphics dependencies.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(EntropyDeformation VERSION 0.1.0 LANGUAGES CXX)
  include(CTest)
endif()

if(NOT TARGET glm::glm)
  find_package(glm CONFIG REQUIRED)
endif()

add_library(EntropyDeformation STATIC)
add_library(Entropy::Deformation ALIAS EntropyDeformation)

target_sources(EntropyDeformation
  PRIVATE
    BrushStep.cpp
    CellVerification.cpp
    ContractError.cpp
    DeformationFieldIO.cpp
    EditHistory.cpp
    FieldDomain.cpp
    FieldEvidence.cpp
    FieldTiles.cpp
    MapPairDescriptor.cpp
    QualityPolicy.cpp
    StrokeSampling.cpp
    VelocityLattice.cpp
  PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/.."
    FILES
      BrushStep.h
      CellVerification.h
      ContractError.h
      DeformationFieldIO.h
      EditHistory.h
      FieldDomain.h
      FieldEvidence.h
      MapPairDescriptor.h
      QualityPolicy.h
      StrokeSampling.h
      Types.h
      VelocityLattice.h
)

target_include_directories(EntropyDeformation PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
  "$<INSTALL_INTERFACE:include>"
)

target_link_libraries(EntropyDeformation PUBLIC glm::glm)
target_compile_features(EntropyDeformation PUBLIC cxx_std_23)
set_target_properties(EntropyDeformation PROPERTIES CXX_EXTENSIONS OFF)

if(TARGET entropy_warnings)
  target_link_libraries(EntropyDeformation PRIVATE entropy_warnings)
else()
  target_compile_options(EntropyDeformation PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /permissive->
  )
endif()

if(COMMAND entropy_enable_coverage_for_target)
  entropy_enable_coverage_for_target(EntropyDeformation)
endif()

option(EntropyDeformation_BUILD_TESTING "Build the standalone deformation contract tests" ${BUILD_TESTING})
if(EntropyDeformation_BUILD_TESTING)
  enable_testing()
  add_subdirectory(test)
endif()
