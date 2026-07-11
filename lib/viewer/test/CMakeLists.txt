add_executable(TestViewer
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

target_link_libraries(TestViewer PRIVATE
  Catch2::Catch2WithMain
  Entropy::Viewer
  entropy_warnings
)

entropy_enable_coverage_for_target(TestViewer)
entropy_register_coverage_test_target(TestViewer)

set_target_properties(TestViewer PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(TestViewer)

catch_discover_tests(TestViewer)
