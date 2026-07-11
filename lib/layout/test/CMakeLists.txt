add_executable(TestLayout
  ImageIndexMappingTests.cpp
  LayoutKindInfoTests.cpp
  LayoutPresetInfoTests.cpp
  LayoutHeaderIncludeTests.cpp
  LayoutFileSerializationTests.cpp
  LayoutSpecJsonTests.cpp
  SyncGroupIndexMapTests.cpp
)

target_link_libraries(TestLayout PRIVATE
  Catch2::Catch2WithMain
  Entropy::Layout
  entropy_warnings
)

entropy_enable_coverage_for_target(TestLayout)
entropy_register_coverage_test_target(TestLayout)

set_target_properties(TestLayout PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(TestLayout)

catch_discover_tests(TestLayout)
