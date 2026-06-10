add_executable(EntropyLayoutTests
  "${entropy_APP_DIR}/logic/serialization/test/ProjectSerializationTests.cpp"
  "${entropy_APP_DIR}/windowing/test/LayoutFileSerializationTests.cpp"
  "${entropy_APP_DIR}/windowing/test/LayoutSpecJsonTests.cpp"
)

target_sources(EntropyLayoutTests PRIVATE
  "${entropy_APP_DIR}/logic/serialization/ProjectSerialization.cpp"
  "${entropy_APP_DIR}/windowing/LayoutFileSerialization.cpp"
  "${entropy_APP_DIR}/windowing/LayoutSpecJson.cpp"
  "${entropy_EXT_DIR}/safeclib/strerrorlen_s.cpp"
)

target_link_libraries(EntropyLayoutTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  entropy_warnings
)

target_include_directories(EntropyLayoutTests PRIVATE
  "${entropy_APP_DIR}"
  "${entropy_EXT_DIR}"
)

set_target_properties(EntropyLayoutTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropyLayoutTests)

catch_discover_tests(EntropyLayoutTests)
