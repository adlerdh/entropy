add_executable(EntropyRenderingTests
  AsciiClipboardTests.cpp
  LightboxOffsetLabelFormatTests.cpp
  LocalLinearResidualMetricTests.cpp
  LocalNccMetricTests.cpp
  PixelEdgeGeometryTests.cpp
  ScaleBarGeometryTests.cpp
  XrayAttenuationTests.cpp
)

target_sources(EntropyRenderingTests PRIVATE
  "${entropy_APP_DIR}/rendering/ascii/AsciiClipboard.cpp"
  "${entropy_APP_DIR}/rendering/LocalLinearResidualMetric.cpp"
  "${entropy_APP_DIR}/rendering/LightboxOffsetLabelFormat.cpp"
  "${entropy_APP_DIR}/rendering/LocalNccMetric.cpp"
  "${entropy_APP_DIR}/rendering/PixelEdgeGeometry.cpp"
  "${entropy_APP_DIR}/rendering/ScaleBarGeometry.cpp"
  "${entropy_APP_DIR}/rendering/XrayAttenuation.cpp"
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
