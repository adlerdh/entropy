# Insight Toolkit (ITK) is an implementation detail of EntropyImage
find_package(ITK ${itk_VERSION} REQUIRED
  COMPONENTS ${entropy_ITK_COMPONENTS}
  HINTS "${itk_PREFIX}/build")
message(STATUS "Found ITK in ${ITK_DIR}")
include("${ITK_USE_FILE}")

add_library(EntropyImage STATIC)
add_library(Entropy::Image ALIAS EntropyImage)

target_sources(EntropyImage PRIVATE
  Image.cpp
  ImageColorMap.cpp
  ImageDerivedData.cpp
  ImageHeader.cpp
  ImageIoInfo.cpp
  ImageSettings.cpp
  ImageTransformations.cpp
  ImageUtility.cpp
  SegUtil.cpp
)

target_include_directories(EntropyImage
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
  PRIVATE
    ${ITK_INCLUDE_DIRS}
)

target_link_libraries(EntropyImage
  PUBLIC
    Entropy::Common
  PRIVATE
    ${ITK_LIBRARIES}
)

target_compile_options(EntropyImage PRIVATE
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

set_target_properties(EntropyImage PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
