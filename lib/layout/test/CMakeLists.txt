add_executable(EntropyLayoutTests
  LayoutHeaderIncludeTests.cpp
  LayoutFileSerializationTests.cpp
  LayoutSpecJsonTests.cpp
)

target_link_libraries(EntropyLayoutTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Layout
  entropy_warnings
)

entropy_enable_coverage_for_target(EntropyLayoutTests)
entropy_register_coverage_test_target(EntropyLayoutTests)

set_target_properties(EntropyLayoutTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropyLayoutTests)

catch_discover_tests(EntropyLayoutTests)
