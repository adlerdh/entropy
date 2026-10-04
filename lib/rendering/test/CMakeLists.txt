add_executable(TestRenderingLibrary
  AsciiClipboardTests.cpp
  FramePacerTests.cpp
  FieldWorkspaceTests.cpp
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

entropy_copy_runtime_dlls(TestRenderingLibrary)

catch_discover_tests(TestRenderingLibrary)

# Build the executable with the other tests, but register it with CTest only on
# graphics workers. Running it without a working GL context fails, never skips.
option(Entropy_ENABLE_DEFORMATION_GPU_TESTS "Register required GL 3.3 deformation tests with CTest" OFF)
if(TARGET EntropyDeformationReference)
  add_executable(TestDeformationGpu DeformationGpuTests.cpp)
  target_link_libraries(TestDeformationGpu PRIVATE
    Catch2::Catch2WithMain Entropy::Rendering EntropyDeformationReference glfw entropy_warnings
  )
  set_target_properties(TestDeformationGpu PROPERTIES
    CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
  )
  foreach(header FieldTextures FieldPassRunner)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/include_${header}.cpp")
    file(GENERATE OUTPUT "${source}" CONTENT "#include \"rendering/deformation/${header}.h\"\n")
    target_sources(TestDeformationGpu PRIVATE "${source}")
  endforeach()
  entropy_enable_coverage_for_target(TestDeformationGpu)
  entropy_register_coverage_test_target(TestDeformationGpu)
  entropy_copy_runtime_dlls(TestDeformationGpu)
  if(Entropy_ENABLE_DEFORMATION_GPU_TESTS)
    catch_discover_tests(TestDeformationGpu PROPERTIES LABELS deformation-gpu)
  endif()
elseif(Entropy_ENABLE_DEFORMATION_GPU_TESTS)
  message(FATAL_ERROR "Deformation GPU tests require EntropyDeformation_BUILD_TESTING=ON")
endif()
