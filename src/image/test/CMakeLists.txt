add_executable(EntropyImageTests
  ImageColorMapTests.cpp
  "${entropy_SRC_DIR}/image/ImageColorMap.cpp"
)

target_include_directories(EntropyImageTests PRIVATE
  "${entropy_SRC_DIR}"
)

target_link_libraries(EntropyImageTests PRIVATE
  Catch2::Catch2WithMain
  glm::glm
  spdlog::spdlog
  entropy_warnings
)

set_target_properties(EntropyImageTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

catch_discover_tests(EntropyImageTests)
