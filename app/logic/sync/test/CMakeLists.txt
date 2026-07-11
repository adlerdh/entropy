add_executable(TestSync
  TestInstanceSyncProtocol.cpp
  ItkSnapSyncProtocolTests.cpp
)

target_sources(TestSync PRIVATE
  "${entropy_APP_DIR}/logic/sync/EntropyInstanceSyncProtocol.cpp"
  "${entropy_APP_DIR}/logic/sync/ItkSnapSyncProtocol.cpp"
)

target_link_libraries(TestSync PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  nlohmann_json::nlohmann_json
  entropy_warnings
)

entropy_enable_coverage_for_target(TestSync)
entropy_register_coverage_test_target(TestSync)

target_include_directories(TestSync PRIVATE
  "${entropy_APP_DIR}"
)

set_target_properties(TestSync PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(TestSync)

catch_discover_tests(TestSync)
