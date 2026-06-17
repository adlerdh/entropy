add_library(EntropyViewerTypes STATIC)
add_library(Entropy::ViewerTypes ALIAS EntropyViewerTypes)

target_sources(EntropyViewerTypes PRIVATE
  ViewModes.cpp
  ViewTypes.cpp
)

target_include_directories(EntropyViewerTypes
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(EntropyViewerTypes
  PUBLIC
    entropy_logging_level
  PRIVATE
    entropy_warnings
)

target_compile_options(EntropyViewerTypes PRIVATE
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

entropy_enable_coverage_for_target(EntropyViewerTypes)

set_target_properties(EntropyViewerTypes PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
