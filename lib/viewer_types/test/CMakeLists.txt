add_executable(EntropyViewerTypesTests
  CameraSyncGroupsTests.cpp
  FrameImageSelectionTests.cpp
  FrameViewportTests.cpp
  ImageSelectionTests.cpp
  LayoutTypesTests.cpp
  ViewModesTests.cpp
  ViewTypesTests.cpp
  ViewerTypesHeaderIncludeTests.cpp
)

target_link_libraries(EntropyViewerTypesTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::ViewerTypes
  entropy_warnings
)

entropy_enable_coverage_for_target(EntropyViewerTypesTests)
entropy_register_coverage_test_target(EntropyViewerTypesTests)

set_target_properties(EntropyViewerTypesTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropyViewerTypesTests)

catch_discover_tests(EntropyViewerTypesTests)
