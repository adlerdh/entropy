add_executable(EntropyCommonTests
  InputParserTests.cpp
  LoggingSettingsTests.cpp
  MathFuncsTests.cpp
)

target_link_libraries(EntropyCommonTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  entropy_warnings
)

entropy_enable_coverage_for_target(EntropyCommonTests)
entropy_register_coverage_test_target(EntropyCommonTests)

set_target_properties(EntropyCommonTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropyCommonTests)

catch_discover_tests(EntropyCommonTests)
