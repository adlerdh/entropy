add_executable(EntropyInteractionTests
  ButtonStateTests.cpp
)

target_sources(EntropyInteractionTests PRIVATE
  "${entropy_APP_DIR}/logic/interaction/events/ButtonState.cpp"
)

target_link_libraries(EntropyInteractionTests PRIVATE
  Catch2::Catch2WithMain
  entropy_warnings
)

entropy_enable_coverage_for_target(EntropyInteractionTests)
entropy_register_coverage_test_target(EntropyInteractionTests)

target_include_directories(EntropyInteractionTests PRIVATE
  "${entropy_APP_DIR}"
)

set_target_properties(EntropyInteractionTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropyInteractionTests)

catch_discover_tests(EntropyInteractionTests)
