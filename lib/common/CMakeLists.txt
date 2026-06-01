add_library(EntropyCommon)
add_library(Entropy::Common ALIAS EntropyCommon)

if(WIN32 AND BUILD_SHARED_LIBS)
  set_target_properties(EntropyCommon PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

target_sources(EntropyCommon PRIVATE
  CoordinateFrame.cpp
  DirectionMaps.cpp
  InputParams.cpp
  InputParser.cpp
  MathFuncs.cpp
  Types.cpp
  UuidUtility.cpp
  Viewport.cpp
)

target_include_directories(EntropyCommon
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
  PRIVATE
    "${CMAKE_BINARY_DIR}" # for BuildStamp.h
    "${iconfont_INCLUDE_DIR}"
)

target_link_libraries(EntropyCommon
  PUBLIC
    glm::glm
    spdlog::spdlog
    stduuid
  PRIVATE
    CLI11::CLI11
)

target_compile_options(EntropyCommon PRIVATE
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

set_target_properties(EntropyCommon PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
