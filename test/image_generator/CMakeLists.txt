find_package(ITK ${itk_VERSION} REQUIRED
  COMPONENTS ${entropy_ITK_COMPONENTS}
  HINTS "${itk_PREFIX}/build")
include("${ITK_USE_FILE}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ImageGenerator_EXAMPLE_OUTPUT_DIR
  "${CMAKE_BINARY_DIR}/image-generator-examples"
  CACHE PATH "Output directory for generated synthetic image example files")

add_library(ImageGeneratorCore STATIC
  ImageGenerator.cpp
)

target_include_directories(ImageGeneratorCore PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(ImageGeneratorCore PUBLIC
  nlohmann_json::nlohmann_json
  ${ITK_LIBRARIES}
  entropy_warnings
)

add_executable(ImageGenerator
  ImageGeneratorMain.cpp
)

target_link_libraries(ImageGenerator PRIVATE
  CLI11::CLI11
  ImageGeneratorCore
  entropy_warnings
)

entropy_copy_runtime_dlls(ImageGenerator)

add_executable(Large2DImageGenerator
  Large2DImageGeneratorMain.cpp
)

target_link_libraries(Large2DImageGenerator PRIVATE
  CLI11::CLI11
  ImageGeneratorCore
  entropy_warnings
)

entropy_copy_runtime_dlls(Large2DImageGenerator)

add_custom_target(GenerateImageExamples
  COMMAND
    "${CMAKE_COMMAND}"
      -D "IMAGE_GENERATOR=$<TARGET_FILE:ImageGenerator>"
      -D "IMAGE_GENERATOR_EXAMPLE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/examples"
      -D "IMAGE_GENERATOR_EXAMPLE_OUTPUT_DIR=${ImageGenerator_EXAMPLE_OUTPUT_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/GenerateImageExamples.cmake"
  DEPENDS ImageGenerator
  COMMENT "Generating synthetic image example files"
  VERBATIM
)

add_executable(TestImageGenerator
  ImageGeneratorTests.cpp
)

target_link_libraries(TestImageGenerator PRIVATE
  Catch2::Catch2WithMain
  ImageGeneratorCore
  entropy_warnings
)

entropy_copy_runtime_dlls(TestImageGenerator)

catch_discover_tests(TestImageGenerator)
