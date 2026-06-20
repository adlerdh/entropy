add_executable(EntropyRenderingTests
  PixelEdgeGeometryTests.cpp
  ScaleBarGeometryTests.cpp
)

target_sources(EntropyRenderingTests PRIVATE
  "${entropy_APP_DIR}/rendering/PixelEdgeGeometry.cpp"
  "${entropy_APP_DIR}/rendering/ScaleBarGeometry.cpp"
)

target_link_libraries(EntropyRenderingTests PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  entropy_warnings
)

entropy_enable_coverage_for_target(EntropyRenderingTests)
entropy_register_coverage_test_target(EntropyRenderingTests)

target_include_directories(EntropyRenderingTests PRIVATE
  "${entropy_APP_DIR}"
)

set_target_properties(EntropyRenderingTests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(EntropyRenderingTests)

catch_discover_tests(EntropyRenderingTests)
