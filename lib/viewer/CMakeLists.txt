add_library(EntropyViewer STATIC)
add_library(Entropy::Viewer ALIAS EntropyViewer)

target_sources(EntropyViewer PRIVATE
  CameraSyncGroups.cpp
  FrameHitGeometry.cpp
  FrameImageSelection.cpp
  FrameViewport.cpp
  ImageSelection.cpp
  ViewModes.cpp
  ViewTypes.cpp
)

target_include_directories(EntropyViewer
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(EntropyViewer
  PUBLIC
    Entropy::Common
    entropy_logging_level
  PRIVATE
    entropy_warnings
)

entropy_enable_coverage_for_target(EntropyViewer)

set_target_properties(EntropyViewer PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
