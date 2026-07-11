find_package(ITK ${itk_VERSION} REQUIRED
  COMPONENTS ${entropy_ITK_COMPONENTS}
  HINTS "${itk_PREFIX}/build")
include("${ITK_USE_FILE}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(WarpFieldGenerator_EXAMPLE_OUTPUT_DIR
  "${CMAKE_BINARY_DIR}/warp-field-examples"
  CACHE PATH "Output directory for generated warp field example images")

set(DeformationWarpExamples_OUTPUT_DIR
  "${CMAKE_BINARY_DIR}/deformation-warp-examples"
  CACHE PATH "Output directory for paired deformation warp test images")

add_library(WarpFieldGeneratorCore STATIC
  WarpFieldGenerator.cpp
)

target_include_directories(WarpFieldGeneratorCore PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(WarpFieldGeneratorCore PUBLIC
  glm::glm
  nlohmann_json::nlohmann_json
  ${ITK_LIBRARIES}
  entropy_warnings
)

add_executable(WarpFieldGenerator
  WarpFieldGeneratorMain.cpp
)

target_link_libraries(WarpFieldGenerator PRIVATE
  CLI11::CLI11
  WarpFieldGeneratorCore
  entropy_warnings
)

entropy_copy_runtime_dlls(WarpFieldGenerator)

add_custom_target(GenerateWarpFieldExamples
  COMMAND
    "${CMAKE_COMMAND}"
      -D "WARP_FIELD_GENERATOR=$<TARGET_FILE:WarpFieldGenerator>"
      -D "WARP_FIELD_EXAMPLE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/examples"
      -D "WARP_FIELD_EXAMPLE_OUTPUT_DIR=${WarpFieldGenerator_EXAMPLE_OUTPUT_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/GenerateWarpFieldExamples.cmake"
  DEPENDS WarpFieldGenerator
  COMMENT "Generating synthetic warp field example images"
  VERBATIM
)

add_custom_target(GenerateDeformationWarpExamples
  COMMAND
    "${CMAKE_COMMAND}"
      -D "IMAGE_GENERATOR=$<TARGET_FILE:ImageGenerator>"
      -D "WARP_FIELD_GENERATOR=$<TARGET_FILE:WarpFieldGenerator>"
      -D "DEFORMATION_WARP_EXAMPLE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../deformation_warp_examples"
      -D "DEFORMATION_WARP_EXAMPLE_OUTPUT_DIR=${DeformationWarpExamples_OUTPUT_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/../deformation_warp_examples/GenerateDeformationWarpExamples.cmake"
  DEPENDS ImageGenerator WarpFieldGenerator
  COMMENT "Generating paired reference moving and deformation field example files"
  VERBATIM
)

add_executable(TestWarpFieldGenerator
  WarpFieldGeneratorTests.cpp
)

target_link_libraries(TestWarpFieldGenerator PRIVATE
  Catch2::Catch2WithMain
  WarpFieldGeneratorCore
  entropy_warnings
)

entropy_copy_runtime_dlls(TestWarpFieldGenerator)

catch_discover_tests(TestWarpFieldGenerator)
