add_executable(TestRendering
  AsciiClipboardTests.cpp
  EmbeddedResourceTests.cpp
  ImageDrawingHelpersTests.cpp
  LightboxOffsetLabelFormatTests.cpp
  LocalLinearResidualMetricTests.cpp
  LocalNccMetricTests.cpp
  MeshTests.cpp
  MeshOpenGlResourceTests.cpp
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
  "${entropy_APP_DIR}/rendering/mesh/MeshAdvancedLighting.cpp"
  "${entropy_APP_DIR}/rendering/mesh/AmbientOcclusionResources.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshBounds.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshCache.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshClipPlanes.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshCrosshairsPolicy.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshDdpPolicy.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshDdpResources.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshExtraction.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshExtractionJobs.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshExtractionQueue.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshExtractionRunner.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshGeneration.cpp"
  "${entropy_APP_DIR}/rendering/mesh/PolyDataGenerator.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshGlyphs.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshImageAdapter.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshImagePlane.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshImagePlaneRenderList.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshImagePlaneRenderable.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshImagePlaneScene.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshIsosurfacePolicy.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshLandmarkPolicy.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshKeys.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshMaterial.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshPicking.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshPrimitives.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshRenderableFactory.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshRenderList.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshResourceLifecycle.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshScalarGrid.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshScene.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshSegmentationPolicy.cpp"
  "${entropy_APP_DIR}/rendering/mesh/SegmentationExtractionBatch.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshShadowMapProjection.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshShadowMapResources.cpp"
  "${entropy_APP_DIR}/rendering/mesh/MeshValidation.cpp"
  "${entropy_APP_DIR}/rendering/metrics/LocalLinearResidualMetric.cpp"
  "${entropy_APP_DIR}/rendering/metrics/LocalNccMetric.cpp"
  "${entropy_APP_DIR}/rendering/physics/XrayAttenuation.cpp"
  "${entropy_APP_DIR}/rendering/ShaderInfoSetup.cpp"
  "${entropy_APP_DIR}/rendering/ShaderProgramSetup.cpp"
  "${entropy_APP_DIR}/rendering/ShaderSourceSetup.cpp"
  "${entropy_APP_DIR}/rendering/ShaderUniformSetup.cpp"
  "${entropy_APP_DIR}/rendering/common/ShaderType.cpp"
  "${entropy_APP_DIR}/rendering/utility/containers/Uniforms.cpp"
  "${entropy_APP_DIR}/rendering/utility/containers/VertexAttributeInfo.cpp"
  "${entropy_APP_DIR}/rendering/utility/containers/VertexIndicesInfo.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLErrorChecker.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLFrameBufferObject.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLShader.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLShaderProgram.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLTexture.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLVertexArrayObject.cpp"
  "${entropy_APP_DIR}/rendering/utility/math/SliceIntersector.cpp"
)

target_link_libraries(TestRendering PRIVATE
  Catch2::Catch2WithMain
  Entropy::Common
  Entropy::Image
  entropy::colormaps
  entropy::fonts
  entropy::shaders
  glad
  glfw
  entropy_warnings
  ${VTK_LIBRARIES}
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
