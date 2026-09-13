add_subdirectory(shaders)

add_library(EntropyRendering STATIC)
add_library(Entropy::Rendering ALIAS EntropyRendering)

target_sources(EntropyRendering PRIVATE
  rendering/ascii/AsciiClipboard.cpp
  rendering/FramePacer.cpp
  rendering/geometry/PixelEdgeGeometry.cpp
  rendering/geometry/ScaleBarGeometry.cpp
  rendering/helpers/ImageDrawingHelpers.cpp
  rendering/helpers/LightboxOffsetLabelFormat.cpp
  rendering/helpers/PipelineHelpers.cpp
  rendering/helpers/TextureSetupHelpers.cpp
  rendering/ImageShaderCapabilities.cpp
  rendering/metrics/LocalLinearResidualMetric.cpp
  rendering/metrics/LocalNccMetric.cpp
  rendering/RenderDerivedData.cpp
  rendering/RenderResources.cpp
  rendering/RenderSettings.cpp
  rendering/ShaderInfoSetup.cpp
  rendering/ShaderPreprocessor.cpp
  rendering/ShaderProgramSetup.cpp
  rendering/ShaderSourceSetup.cpp
  rendering/ShaderTextureDimension.cpp
  rendering/ShaderUniformSetup.cpp
  rendering/common/ShaderType.cpp
  rendering/gl/Uniforms.cpp
  rendering/gl/VertexAttributeInfo.cpp
  rendering/gl/VertexIndicesInfo.cpp
  rendering/gl/GLBufferObject.cpp
  rendering/gl/GLBufferTexture.cpp
  rendering/gl/GLErrorChecker.cpp
  rendering/gl/GLFrameBufferObject.cpp
  rendering/gl/GLShader.cpp
  rendering/gl/GLShaderProgram.cpp
  rendering/gl/GLTexture.cpp
  rendering/gl/GLVertexArrayObject.cpp
  rendering/gl/OpenGLContext.cpp
  rendering/gl/OpenGLRenderState.cpp
  rendering/gl/OpenGLStateGuard.cpp
  rendering/helpers/SliceIntersector.cpp
  rendering/physics/XrayAttenuation.cpp
)

target_sources(EntropyRendering
  PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
      rendering/FramePacer.h
      rendering/ImageShaderCapabilities.h
      rendering/RenderDerivedData.h
      rendering/RenderResources.h
      rendering/RenderSettings.h
      rendering/ShaderInfoSetup.h
      rendering/ShaderPreprocessor.h
      rendering/ShaderProgramSetup.h
      rendering/ShaderSourceSetup.h
      rendering/ShaderTextureDimension.h
      rendering/ShaderUniformSetup.h
      rendering/TextureLayout.h
      rendering/ascii/AsciiClipboard.h
      rendering/common/ShaderType.h
      rendering/geometry/PixelEdgeGeometry.h
      rendering/geometry/ScaleBarGeometry.h
      rendering/gl/GLBufferObject.h
      rendering/gl/GLBufferTexture.h
      rendering/gl/GLBufferTypes.h
      rendering/gl/GLDrawTypes.h
      rendering/gl/GLErrorChecker.h
      rendering/gl/GLFBOAttachmentTypes.h
      rendering/gl/GLFrameBufferObject.h
      rendering/gl/GLShader.h
      rendering/gl/GLShaderProgram.h
      rendering/gl/GLShaderType.h
      rendering/gl/GLTexture.h
      rendering/gl/GLTextureTypes.h
      rendering/gl/GLUniformTypes.h
      rendering/gl/GLVertexArrayObject.h
      rendering/gl/OpenGLContext.h
      rendering/gl/OpenGLRenderState.h
      rendering/gl/OpenGLStateGuard.h
      rendering/gl/Uniforms.h
      rendering/gl/VertexAttributeInfo.h
      rendering/gl/VertexIndicesInfo.h
      rendering/helpers/ImageDrawingHelpers.h
      rendering/helpers/LightboxOffsetLabelFormat.h
      rendering/helpers/PipelineHelpers.h
      rendering/helpers/SliceIntersector.h
      rendering/helpers/SliceIntersectorTypes.h
      rendering/helpers/TextureSetupHelpers.h
      rendering/helpers/UnderlyingEnumType.h
      rendering/mesh/AmbientOcclusionPass.h
      rendering/mesh/AmbientOcclusionResources.h
      rendering/mesh/MeshAdvancedLighting.h
      rendering/mesh/MeshBounds.h
      rendering/mesh/MeshCache.h
      rendering/mesh/MeshClipPlanes.h
      rendering/mesh/MeshCompositing.h
      rendering/mesh/MeshCrosshairsPolicy.h
      rendering/mesh/MeshCutaway.h
      rendering/mesh/MeshData.h
      rendering/mesh/MeshDdpPass.h
      rendering/mesh/MeshDdpPolicy.h
      rendering/mesh/MeshDdpResources.h
      rendering/mesh/MeshDrawOptions.h
      rendering/mesh/MeshExtraction.h
      rendering/mesh/MeshExtractionQueue.h
      rendering/mesh/MeshExtractionRunner.h
      rendering/mesh/MeshExtractionService.h
      rendering/mesh/MeshGeneration.h
      rendering/mesh/MeshGenerationOptions.h
      rendering/mesh/MeshGlyphs.h
      rendering/mesh/MeshGpuData.h
      rendering/mesh/MeshGpuStore.h
      rendering/mesh/MeshHandle.h
      rendering/mesh/MeshImagePlane.h
      rendering/mesh/MeshImagePlaneRenderList.h
      rendering/mesh/MeshImagePlaneRenderable.h
      rendering/mesh/MeshImagePlaneScene.h
      rendering/mesh/MeshIsosurfacePolicy.h
      rendering/mesh/MeshKeys.h
      rendering/mesh/MeshLandmarkPolicy.h
      rendering/mesh/MeshMaterial.h
      rendering/mesh/MeshPicking.h
      rendering/mesh/MeshPlaneIntersection.h
      rendering/mesh/MeshPrimitives.h
      rendering/mesh/MeshRenderList.h
      rendering/mesh/MeshRenderable.h
      rendering/mesh/MeshRenderableFactory.h
      rendering/mesh/MeshRenderer.h
      rendering/mesh/MeshResourceStore.h
      rendering/mesh/MeshScalarGrid.h
      rendering/mesh/MeshScene.h
      rendering/mesh/MeshSegmentationPolicy.h
      rendering/mesh/MeshShadowMapPass.h
      rendering/mesh/MeshShadowMapProjection.h
      rendering/mesh/MeshShadowMapResources.h
      rendering/mesh/MeshUpload.h
      rendering/mesh/MeshValidation.h
      rendering/mesh/PolyDataGenerator.h
      rendering/metrics/LocalLinearResidualMetric.h
      rendering/metrics/LocalNccMetric.h
      rendering/physics/XrayAttenuation.h
)

add_subdirectory(rendering/mesh)

target_include_directories(EntropyRendering
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    "${glad_INCLUDE_DIR}"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/rendering"
)

target_link_libraries(EntropyRendering
  PUBLIC
    Entropy::Common
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
