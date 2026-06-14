add_executable(EntropyWindowingTests
  LayoutFileSerializationTests.cpp
  LayoutSpecJsonTests.cpp
  LinuxUiScaleTests.cpp
)

target_sources(EntropyWindowingTests PRIVATE
  "${entropy_APP_DIR}/windowing/LayoutFileSerialization.cpp"
  "${entropy_APP_DIR}/windowing/LayoutSpecJson.cpp"
  "${entropy_APP_DIR}/windowing/LinuxUiScale.cpp"
)

target_link_libraries(EntropyWindowingTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  entropy_warnings
)

entropy_enable_coverage_for_target(EntropyWindowingTests)
entropy_register_coverage_test_target(EntropyWindowingTests)

target_include_directories(EntropyWindowingTests PRIVATE
  "${entropy_APP_DIR}"
)

set_target_properties(EntropyWindowingTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropyWindowingTests)

catch_discover_tests(EntropyWindowingTests)
