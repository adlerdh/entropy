add_executable(EntropyRegistrationTests
  ArtifactsTests.cpp
  AvailabilityTests.cpp
  CapabilitiesTests.cpp
  CommandsTests.cpp
  ConfigTests.cpp
  ExecutionTests.cpp
  ImportPlanTests.cpp
  JsonTests.cpp
  JobsTests.cpp
  ProcessTests.cpp
  ProgressTests.cpp
  SetupTests.cpp
  ValidationTests.cpp
)

target_link_libraries(EntropyRegistrationTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Registration
  entropy_warnings
)

target_compile_definitions(EntropyRegistrationTests PRIVATE
  ENTROPY_TEST_CMAKE_COMMAND="${CMAKE_COMMAND}"
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
