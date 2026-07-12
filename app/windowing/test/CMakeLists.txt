add_executable(TestWindowing
  WindowingHeaderIncludeTests.cpp
)

target_link_libraries(TestWindowing PRIVATE
  Catch2::Catch2WithMain
  Entropy::Layout
  Entropy::Viewer
  nlohmann_json::nlohmann_json
  entropy_warnings
)

entropy_enable_coverage_for_target(TestWindowing)
entropy_register_coverage_test_target(TestWindowing)

target_include_directories(TestWindowing PRIVATE
  "${entropy_APP_DIR}"
)

set_target_properties(TestWindowing PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(TestWindowing)

catch_discover_tests(TestWindowing)
