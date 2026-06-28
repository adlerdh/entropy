add_executable(EntropyRegistrationTests
  CapabilitiesTests.cpp
  CommandsTests.cpp
  JsonTests.cpp
  ProgressTests.cpp
  ValidationTests.cpp
)

target_link_libraries(EntropyRegistrationTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Registration
  entropy_warnings
)

entropy_enable_coverage_for_target(EntropyRegistrationTests)
entropy_register_coverage_test_target(EntropyRegistrationTests)

set_target_properties(EntropyRegistrationTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropyRegistrationTests)

catch_discover_tests(EntropyRegistrationTests)
