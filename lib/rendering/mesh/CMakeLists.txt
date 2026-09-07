target_sources(EntropyRendering PRIVATE
  AmbientOcclusionPass.cpp
  AmbientOcclusionResources.cpp
  MeshAdvancedLighting.cpp
  MeshBounds.cpp
  MeshCache.cpp
  MeshClipPlanes.cpp
  MeshCutaway.cpp
  MeshCrosshairsPolicy.cpp
  MeshDdpPass.cpp
  MeshDdpPolicy.cpp
  MeshDdpResources.cpp
  MeshExtraction.cpp
  MeshExtractionQueue.cpp
  MeshExtractionRunner.cpp
  MeshExtractionService.cpp
  MeshGlyphs.cpp
  MeshGpuData.cpp
  MeshGpuStore.cpp
  MeshImagePlane.cpp
  MeshImagePlaneRenderList.cpp
  MeshImagePlaneRenderable.cpp
  MeshImagePlaneScene.cpp
  MeshIsosurfacePolicy.cpp
  MeshKeys.cpp
  MeshLandmarkPolicy.cpp
  MeshMaterial.cpp
  MeshPicking.cpp
  MeshPrimitives.cpp
  MeshRenderableFactory.cpp
  MeshRenderer.cpp
  MeshResourceStore.cpp
  MeshRenderList.cpp
  MeshScalarGrid.cpp
  MeshScene.cpp
  MeshSegmentationPolicy.cpp
  MeshShadowMapPass.cpp
  MeshShadowMapProjection.cpp
  MeshShadowMapResources.cpp
  MeshUpload.cpp
  MeshValidation.cpp
)

# VTK is intentionally isolated from the base rendering target. Applications that
# upload prepared meshes can use Entropy::Rendering without a mesh-processing toolkit.
add_library(EntropyRenderingMeshProcessing STATIC
  MeshGeneration.cpp
  PolyDataGenerator.cpp
)
add_library(Entropy::RenderingMeshProcessing ALIAS EntropyRenderingMeshProcessing)

target_include_directories(EntropyRenderingMeshProcessing PUBLIC
  "${entropy_LIB_DIR}"
)

target_link_libraries(EntropyRenderingMeshProcessing
  PUBLIC
    Entropy::Rendering
    glm::glm
    ${VTK_LIBRARIES}
  PRIVATE
    entropy_warnings
)

entropy_enable_coverage_for_target(EntropyRenderingMeshProcessing)

set_target_properties(EntropyRenderingMeshProcessing PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)
