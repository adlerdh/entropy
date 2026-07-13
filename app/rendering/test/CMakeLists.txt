add_executable(TestRendering
  AsciiClipboardTests.cpp
  ImageDrawingHelpersTests.cpp
  LightboxOffsetLabelFormatTests.cpp
  LocalLinearResidualMetricTests.cpp
  LocalNccMetricTests.cpp
  PixelEdgeGeometryTests.cpp
  PipelineHelpersTests.cpp
  ShaderProgramSetupTests.cpp
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
  "${entropy_APP_DIR}/rendering/helpers/PipelineHelpers.cpp"
  "${entropy_APP_DIR}/rendering/helpers/TextureSetupHelpers.cpp"
  "${entropy_APP_DIR}/rendering/helpers/VectorDrawingHelpers.cpp"
  "${entropy_APP_DIR}/rendering/metrics/LocalLinearResidualMetric.cpp"
  "${entropy_APP_DIR}/rendering/metrics/LocalNccMetric.cpp"
  "${entropy_APP_DIR}/rendering/physics/XrayAttenuation.cpp"
  "${entropy_APP_DIR}/rendering/ShaderInfoSetup.cpp"
  "${entropy_APP_DIR}/rendering/ShaderProgramSetup.cpp"
  "${entropy_APP_DIR}/rendering/ShaderSourceSetup.cpp"
  "${entropy_APP_DIR}/rendering/ShaderUniformSetup.cpp"
  "${entropy_APP_DIR}/rendering/common/ShaderType.cpp"
  "${entropy_APP_DIR}/rendering/utility/containers/Uniforms.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLErrorChecker.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLShader.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLShaderProgram.cpp"
)

target_link_libraries(TestRendering PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  entropy::shaders
  glad
  entropy_warnings
)

entropy_enable_coverage_for_target(TestRendering)
entropy_register_coverage_test_target(TestRendering)

target_include_directories(TestRendering PRIVATE
  "${entropy_APP_DIR}"
  "${glad_INCLUDE_DIR}"
)

set_target_properties(TestRendering PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_copy_runtime_dlls(TestRendering)

catch_discover_tests(TestRendering)
