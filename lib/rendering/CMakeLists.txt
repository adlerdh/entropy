add_subdirectory(shaders)

add_library(EntropyRendering STATIC)
add_library(Entropy::Rendering ALIAS EntropyRendering)

target_sources(EntropyRendering PRIVATE
  deformation/FieldTextures.cpp
  deformation/FieldPassRunner.cpp
  deformation/FieldReadback.cpp
  deformation/NumericalState.cpp
  deformation/FieldWorkspace.cpp
  ascii/AsciiClipboard.cpp
  FramePacer.cpp
  geometry/PixelEdgeGeometry.cpp
  geometry/ScaleBarGeometry.cpp
  helpers/ImageDrawingHelpers.cpp
  helpers/LightboxOffsetLabelFormat.cpp
  helpers/PipelineHelpers.cpp
  helpers/TextureSetupHelpers.cpp
  ImageShaderCapabilities.cpp
  metrics/LocalLinearResidualMetric.cpp
  metrics/LocalNccMetric.cpp
  RenderDerivedData.cpp
  RenderResources.cpp
  RenderSettings.cpp
  ShaderInfoSetup.cpp
  ShaderPreprocessor.cpp
  ShaderProgramSetup.cpp
  ShaderSourceSetup.cpp
  ShaderTextureDimension.cpp
  ShaderUniformSetup.cpp
  common/ShaderType.cpp
  gl/Uniforms.cpp
  gl/VertexAttributeInfo.cpp
  gl/VertexIndicesInfo.cpp
  gl/GLBufferObject.cpp
  gl/GLBufferTexture.cpp
  gl/GLErrorChecker.cpp
  gl/GLFrameBufferObject.cpp
  gl/GLShader.cpp
  gl/GLShaderProgram.cpp
  gl/GLTexture.cpp
  gl/GLVertexArrayObject.cpp
  gl/OpenGLContext.cpp
  gl/OpenGLRenderState.cpp
  gl/OpenGLStateGuard.cpp
  helpers/SliceIntersector.cpp
  physics/XrayAttenuation.cpp
)

if(APPLE AND Entropy_ENABLE_IWYU)
  # IWYU 0.26 on macOS stalls while analyzing this translation unit.
  # Keep compiling it; skip only the local IWYU invocation.
  set_source_files_properties(gl/Uniforms.cpp PROPERTIES SKIP_LINTING ON)
endif()

target_sources(EntropyRendering
  PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/.."
    FILES
      deformation/FieldTextures.h
      deformation/FieldPassRunner.h
      deformation/FieldReadback.h
      deformation/FieldWorkspace.h
      FramePacer.h
      ImageShaderCapabilities.h
      RenderDerivedData.h
      RenderResources.h
      RenderSettings.h
      ShaderInfoSetup.h
      ShaderPreprocessor.h
      ShaderProgramSetup.h
      ShaderSourceSetup.h
      ShaderTextureDimension.h
      ShaderUniformSetup.h
      TextureLayout.h
      ascii/AsciiClipboard.h
      common/ShaderType.h
      geometry/PixelEdgeGeometry.h
      geometry/ScaleBarGeometry.h
      gl/GLBufferObject.h
      gl/GLBufferTexture.h
      gl/GLBufferTypes.h
      gl/GLDrawTypes.h
      gl/GLErrorChecker.h
      gl/GLFBOAttachmentTypes.h
      gl/GLFrameBufferObject.h
      gl/GLShader.h
      gl/GLShaderProgram.h
      gl/GLShaderType.h
      gl/GLTexture.h
      gl/GLTextureTypes.h
      gl/GLUniformTypes.h
      gl/GLVertexArrayObject.h
      gl/OpenGLContext.h
      gl/OpenGLRenderState.h
      gl/OpenGLStateGuard.h
      gl/Uniforms.h
      gl/VertexAttributeInfo.h
      gl/VertexIndicesInfo.h
      helpers/ImageDrawingHelpers.h
      helpers/LightboxOffsetLabelFormat.h
      helpers/PipelineHelpers.h
      helpers/SliceIntersector.h
      helpers/SliceIntersectorTypes.h
      helpers/TextureSetupHelpers.h
      helpers/UnderlyingEnumType.h
      mesh/AmbientOcclusionPass.h
      mesh/AmbientOcclusionResources.h
      mesh/MeshAdvancedLighting.h
      mesh/MeshBounds.h
      mesh/MeshCache.h
      mesh/MeshClipPlanes.h
      mesh/MeshCompositing.h
      mesh/MeshCrosshairsPolicy.h
      mesh/MeshCutaway.h
      mesh/MeshData.h
      mesh/MeshDdpPass.h
      mesh/MeshDdpPolicy.h
      mesh/MeshDdpResources.h
      mesh/MeshDrawOptions.h
      mesh/MeshExtraction.h
      mesh/MeshExtractionQueue.h
      mesh/MeshExtractionRunner.h
      mesh/MeshExtractionService.h
      mesh/MeshGeneration.h
      mesh/MeshGenerationOptions.h
      mesh/MeshGlyphs.h
      mesh/MeshGpuData.h
      mesh/MeshGpuStore.h
      mesh/MeshHandle.h
      mesh/MeshImagePlane.h
      mesh/MeshImagePlaneRenderList.h
      mesh/MeshImagePlaneRenderable.h
      mesh/MeshImagePlaneScene.h
      mesh/MeshIsosurfacePolicy.h
      mesh/MeshKeys.h
      mesh/MeshLandmarkPolicy.h
      mesh/MeshMaterial.h
      mesh/MeshPicking.h
      mesh/MeshPlaneIntersection.h
      mesh/MeshPrimitives.h
      mesh/MeshRenderList.h
      mesh/MeshRenderable.h
      mesh/MeshRenderableFactory.h
      mesh/MeshRenderer.h
      mesh/MeshResourceStore.h
      mesh/MeshScalarGrid.h
      mesh/MeshScene.h
      mesh/MeshSegmentationPolicy.h
      mesh/MeshShadowMapPass.h
      mesh/MeshShadowMapProjection.h
      mesh/MeshShadowMapResources.h
      mesh/MeshUpload.h
      mesh/MeshValidation.h
      mesh/PolyDataGenerator.h
      metrics/LocalLinearResidualMetric.h
      metrics/LocalNccMetric.h
      physics/XrayAttenuation.h
)

add_subdirectory(mesh)

target_include_directories(EntropyRendering
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    "${glad_INCLUDE_DIR}"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(EntropyRendering
  PUBLIC
    Entropy::Common
    Entropy::Deformation
    glad
    glm::glm
    stduuid
  PRIVATE
    entropy::shaders
    entropy_warnings
)

entropy_enable_coverage_for_target(EntropyRendering)

set_target_properties(EntropyRendering PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
