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
  MeshPlaneIntersection.cpp
  PolyDataGenerator.cpp
)

if(Entropy_ENABLE_IWYU)
  # IWYU 0.21 on Ubuntu 24 uses a Clang 17 frontend that crashes while parsing GCC 13 std::expected:
  set_source_files_properties(MeshPlaneIntersection.cpp PROPERTIES SKIP_LINTING ON)
endif()
add_library(Entropy::RenderingMeshProcessing ALIAS EntropyRenderingMeshProcessing)

target_link_libraries(EntropyRenderingMeshProcessing
  PUBLIC
    Entropy::Rendering
    glm::glm
    VTK::CommonCore
    VTK::CommonDataModel
  PRIVATE
    VTK::CommonExecutionModel
    VTK::FiltersCore
    VTK::FiltersGeneral
    VTK::FiltersSources
    VTK::IOGeometry
    VTK::IOPLY
    VTK::IOXML
    entropy_warnings
)

entropy_enable_coverage_for_target(EntropyRenderingMeshProcessing)

set_target_properties(EntropyRenderingMeshProcessing PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)
