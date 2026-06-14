add_executable(EntropyImageTests
  DicomSeriesTests.cpp
  ImageColorMapTests.cpp
  ImageCoreTests.cpp
  ImageHeaderTransformTests.cpp
  ImageSettingsTests.cpp
  ImageLoadingTests.cpp
  SegmentationDerivedDataTests.cpp
)

target_link_libraries(EntropyImageTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Image
  ${ITK_LIBRARIES}
  entropy_warnings
)

entropy_enable_coverage_for_target(EntropyImageTests)
entropy_register_coverage_test_target(EntropyImageTests)

target_include_directories(EntropyImageTests PRIVATE
  ${ITK_INCLUDE_DIRS}
)

set_target_properties(EntropyImageTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropyImageTests)

catch_discover_tests(EntropyImageTests)
