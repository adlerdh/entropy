add_executable(TestRenderingLibrary
  AsciiClipboardTests.cpp
  FramePacerTests.cpp
  ImageDrawingHelpersTests.cpp
  ImageShaderCapabilitiesTests.cpp
  LightboxOffsetLabelFormatTests.cpp
  LocalLinearResidualMetricTests.cpp
  LocalNccMetricTests.cpp
  MeshServicesTests.cpp
  PixelEdgeGeometryTests.cpp
  PipelineHelpersTests.cpp
  RenderDerivedDataTests.cpp
  RenderSettingsTests.cpp
  ScaleBarGeometryTests.cpp
  ShaderCompilationTests.cpp
  ShaderPreprocessorTests.cpp
  ShaderProgramSetupTests.cpp
  UniformsTests.cpp
  TextureSetupHelpersTests.cpp
  XrayAttenuationTests.cpp
)

find_program(entropy_GLSLANG_VALIDATOR NAMES glslangValidator)
if(entropy_GLSLANG_VALIDATOR)
  target_compile_definitions(TestRenderingLibrary PRIVATE
    ENTROPY_GLSLANG_VALIDATOR="${entropy_GLSLANG_VALIDATOR}"
  )
endif()

target_link_libraries(TestRenderingLibrary PRIVATE
  Catch2::Catch2WithMain
  Entropy::Rendering
  entropy_warnings
)

entropy_enable_coverage_for_target(TestRenderingLibrary)
entropy_register_coverage_test_target(TestRenderingLibrary)

set_target_properties(TestRenderingLibrary PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

catch_discover_tests(TestRenderingLibrary)
