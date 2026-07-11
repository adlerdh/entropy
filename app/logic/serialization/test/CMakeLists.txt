add_executable(TestSerialization
  ProjectSerializationTests.cpp
)

target_sources(TestSerialization PRIVATE
  "${entropy_APP_DIR}/logic/serialization/ProjectSerialization.cpp"
  "${entropy_EXT_DIR}/safeclib/strerrorlen_s.cpp"
)

target_link_libraries(TestSerialization PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  Entropy::Layout
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  entropy_warnings
)

entropy_enable_coverage_for_target(TestSerialization)
entropy_register_coverage_test_target(TestSerialization)

target_include_directories(TestSerialization PRIVATE
  "${entropy_APP_DIR}"
  "${entropy_EXT_DIR}"
)

set_target_properties(TestSerialization PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(TestSerialization)

catch_discover_tests(TestSerialization)
