add_executable(TestRendering
  AsciiClipboardTests.cpp
  ImageDrawingHelpersTests.cpp
  LightboxOffsetLabelFormatTests.cpp
  LocalLinearResidualMetricTests.cpp
  LocalNccMetricTests.cpp
  PixelEdgeGeometryTests.cpp
  ScaleBarGeometryTests.cpp
  TextureSetupHelpersTests.cpp
  VectorDrawingHelpersTests.cpp
  XrayAttenuationTests.cpp
)

target_sources(TestRendering PRIVATE
  "${entropy_APP_DIR}/rendering/ascii/AsciiClipboard.cpp"
  "${entropy_APP_DIR}/rendering/geometry/PixelEdgeGeometry.cpp"
  "${entropy_APP_DIR}/rendering/geometry/ScaleBarGeometry.cpp"
  "${entropy_APP_DIR}/rendering/helpers/ImageDrawingHelpers.cpp"
  "${entropy_APP_DIR}/rendering/helpers/LightboxOffsetLabelFormat.cpp"
  "${entropy_APP_DIR}/rendering/helpers/TextureSetupHelpers.cpp"
  "${entropy_APP_DIR}/rendering/helpers/VectorDrawingHelpers.cpp"
  "${entropy_APP_DIR}/rendering/metrics/LocalLinearResidualMetric.cpp"
  "${entropy_APP_DIR}/rendering/metrics/LocalNccMetric.cpp"
  "${entropy_APP_DIR}/rendering/physics/XrayAttenuation.cpp"
)

target_link_libraries(TestRendering PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  entropy_warnings
)

entropy_enable_coverage_for_target(TestRendering)
entropy_register_coverage_test_target(TestRendering)

target_include_directories(TestRendering PRIVATE
  "${entropy_APP_DIR}"
)

set_target_properties(TestRendering PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(TestRendering)

catch_discover_tests(TestRendering)
