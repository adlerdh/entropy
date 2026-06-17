add_executable(EntropyViewerTests
  CameraSyncGroupsTests.cpp
  FrameHitGeometryTests.cpp
  FrameImageSelectionTests.cpp
  FrameViewportTests.cpp
  ImageSelectionTests.cpp
  LayoutTypesTests.cpp
  ViewModesTests.cpp
  ViewTypesTests.cpp
  ViewerHeaderIncludeTests.cpp
)

target_link_libraries(EntropyViewerTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Viewer
  entropy_warnings
)

entropy_enable_coverage_for_target(EntropyViewerTests)
entropy_register_coverage_test_target(EntropyViewerTests)

set_target_properties(EntropyViewerTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropyViewerTests)

catch_discover_tests(EntropyViewerTests)
