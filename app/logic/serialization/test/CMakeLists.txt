add_executable(EntropySerializationTests
  ProjectSerializationTests.cpp
)

target_sources(EntropySerializationTests PRIVATE
  "${entropy_APP_DIR}/logic/serialization/ProjectSerialization.cpp"
  "${entropy_APP_DIR}/windowing/LayoutSpecJson.cpp"
  "${entropy_EXT_DIR}/safeclib/strerrorlen_s.cpp"
)

target_link_libraries(EntropySerializationTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  entropy_warnings
)

entropy_enable_coverage_for_target(EntropySerializationTests)
entropy_register_coverage_test_target(EntropySerializationTests)

target_include_directories(EntropySerializationTests PRIVATE
  "${entropy_APP_DIR}"
  "${entropy_EXT_DIR}"
)

set_target_properties(EntropySerializationTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropySerializationTests)

catch_discover_tests(EntropySerializationTests)
