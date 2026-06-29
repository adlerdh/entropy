add_library(EntropyRegistration STATIC)
add_library(Entropy::Registration ALIAS EntropyRegistration)

target_sources(EntropyRegistration PRIVATE
  AffineTransformIO.cpp
  Artifacts.cpp
  Availability.cpp
  Capabilities.cpp
  Commands.cpp
  Config.cpp
  Execution.cpp
  ImportPlan.cpp
  Json.cpp
  Jobs.cpp
  Process.cpp
  Progress.cpp
  Setup.cpp
  Types.cpp
  Validation.cpp
)

target_include_directories(EntropyRegistration
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(EntropyRegistration
  PUBLIC
    glm::glm
    nlohmann_json::nlohmann_json
  PRIVATE
    entropy_logging_level
    entropy_warnings
)

target_compile_definitions(EntropyRegistration PRIVATE
  ENTROPY_FIREANTS_BRIDGE_PYTHONPATH="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_options(EntropyRegistration PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
    -Werror
    -Wall -Wextra -Wpointer-arith -Winit-self -Wunreachable-code -Wshadow
    -Wno-error=array-bounds
    -Wno-error=empty-body
    -Wno-error=float-conversion
    -Wno-error=maybe-uninitialized
    -Wno-error=stringop-overflow
    -ftrapv
  >
  $<$<CXX_COMPILER_ID:AppleClang>:
    -Werror -Wall -Wextra -Wpointer-arith -Winit-self -Wunreachable-code
    -Wno-error=array-bounds
    -Wshadow -ftrapv
  >
  $<$<CXX_COMPILER_ID:MSVC>:
    /W4
  >
)

entropy_enable_coverage_for_target(EntropyRegistration)

set_target_properties(EntropyRegistration PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
