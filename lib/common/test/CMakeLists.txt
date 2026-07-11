add_executable(TestCommon
  ClipboardFormatsTests.cpp
  CommonHeaderIncludeTests.cpp
  CoordinateFrameTests.cpp
  DirectionMapsTests.cpp
  InputParserTests.cpp
  LoggingSettingsTests.cpp
  MathFuncsTests.cpp
  TypesTests.cpp
  UuidUtilityTests.cpp
  ViewportTests.cpp
)

target_link_libraries(TestCommon PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  entropy_warnings
)

entropy_enable_coverage_for_target(TestCommon)
entropy_register_coverage_test_target(TestCommon)

set_target_properties(TestCommon PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(TestCommon)

catch_discover_tests(TestCommon)
