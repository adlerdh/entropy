add_executable(EntropyImageTests
  DicomSeriesTests.cpp
  ImageColorMapTests.cpp
  ImageCoreTests.cpp
  ImageHeaderTransformTests.cpp
  ImageSettingsTests.cpp
  ImageTimeAxisTests.cpp
  TimePlaybackControllerTests.cpp
  ComprehensiveImageLoadingTests.cpp
  ImageLoadingTests.cpp
  SegmentationDerivedDataTests.cpp
  ../../../test/image_generator/ImageGenerator.cpp
)

target_link_libraries(EntropyImageTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Image
  nlohmann_json::nlohmann_json
  ${ITK_LIBRARIES}
  entropy_warnings
)

entropy_enable_coverage_for_target(EntropyImageTests)
entropy_register_coverage_test_target(EntropyImageTests)

target_include_directories(EntropyImageTests PRIVATE
  ${ITK_INCLUDE_DIRS}
  "${CMAKE_SOURCE_DIR}/test/image_generator"
)

set_target_properties(EntropyImageTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropyImageTests)

catch_discover_tests(EntropyImageTests)
