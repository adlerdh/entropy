add_library(EntropyViewer STATIC)
add_library(Entropy::Viewer ALIAS EntropyViewer)

target_sources(EntropyViewer
  PRIVATE
    viewer/CameraSyncGroups.cpp
    viewer/FrameHitGeometry.cpp
    viewer/FrameImageSelection.cpp
    viewer/FrameViewport.cpp
    viewer/ImageSelection.cpp
    viewer/ViewModes.cpp
    viewer/ViewTypes.cpp
  PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
      viewer/CameraSyncGroups.h
      viewer/FrameHitGeometry.h
      viewer/FrameImageSelection.h
      viewer/FrameViewport.h
      viewer/ImageSelection.h
      viewer/LayoutTypes.h
      viewer/ThreeDSceneContents.h
      viewer/ViewModes.h
      viewer/ViewTypes.h
)

target_include_directories(EntropyViewer
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
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
