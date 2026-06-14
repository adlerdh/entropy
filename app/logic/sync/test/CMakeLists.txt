add_executable(EntropySyncTests
  EntropyInstanceSyncProtocolTests.cpp
  ItkSnapSyncProtocolTests.cpp
)

target_sources(EntropySyncTests PRIVATE
  "${entropy_APP_DIR}/logic/sync/EntropyInstanceSyncProtocol.cpp"
  "${entropy_APP_DIR}/logic/sync/ItkSnapSyncProtocol.cpp"
)

target_link_libraries(EntropySyncTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  nlohmann_json::nlohmann_json
  entropy_warnings
)

entropy_enable_coverage_for_target(EntropySyncTests)
entropy_register_coverage_test_target(EntropySyncTests)

target_include_directories(EntropySyncTests PRIVATE
  "${entropy_APP_DIR}"
)

set_target_properties(EntropySyncTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropySyncTests)

catch_discover_tests(EntropySyncTests)
