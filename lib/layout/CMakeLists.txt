add_library(EntropyLayout STATIC)
add_library(Entropy::Layout ALIAS EntropyLayout)

target_sources(EntropyLayout PRIVATE
  LayoutFileSerialization.cpp
  LayoutSpecJson.cpp
)

target_include_directories(EntropyLayout
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(EntropyLayout
  PUBLIC
    Entropy::ViewerTypes
    nlohmann_json::nlohmann_json
  PRIVATE
    entropy_logging_level
    entropy_warnings
    spdlog::spdlog
)

target_compile_options(EntropyLayout PRIVATE
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

entropy_enable_coverage_for_target(EntropyLayout)

set_target_properties(EntropyLayout PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
