add_executable(EntropyCommonTests
  MathFuncsTests.cpp
  "${entropy_SRC_DIR}/common/CoordinateFrame.cpp"
  "${entropy_SRC_DIR}/common/MathFuncs.cpp"
)

target_include_directories(EntropyCommonTests PRIVATE
  "${entropy_SRC_DIR}"
)

target_link_libraries(EntropyCommonTests PRIVATE
  Catch2::Catch2WithMain
  glm::glm
  spdlog::spdlog
  entropy_warnings
)

set_target_properties(EntropyCommonTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

catch_discover_tests(EntropyCommonTests)
