add_executable(TestInteraction
  ButtonStateTests.cpp
)

target_sources(TestInteraction PRIVATE
  "${entropy_APP_DIR}/logic/interaction/events/ButtonState.cpp"
)

target_link_libraries(TestInteraction PRIVATE
  Catch2::Catch2WithMain
  entropy_warnings
)

entropy_enable_coverage_for_target(TestInteraction)
entropy_register_coverage_test_target(TestInteraction)

target_include_directories(TestInteraction PRIVATE
  "${entropy_APP_DIR}"
)

set_target_properties(TestInteraction PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(TestInteraction)

catch_discover_tests(TestInteraction)
