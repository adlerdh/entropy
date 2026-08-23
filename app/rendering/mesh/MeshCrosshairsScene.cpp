#include "rendering/Rendering.h"

#include "common/UuidUtility.h"
#include "image/Image.h"
#include "logic/app/Data.h"
#include "rendering/PrivateMethods.h"
#include "rendering/RenderData.h"
#include "rendering/mesh/MeshCrosshairsPolicy.h"
#include "rendering/mesh/MeshGlyphs.h"
#include "rendering/mesh/MeshPrimitives.h"
#include "rendering/mesh/MeshScene.h"
#include "windowing/View.h"

#include <glm/geometric.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace
{

const rendering::mesh::MeshHandle& crosshairsSphereMeshHandle()
{
  static const rendering::mesh::MeshHandle handle{.uid = generateRandomUuid(), .geometryVersion = 1};
  return handle;
}

} // namespace

bool Rendering::appendMeshCrosshairsRenderableForView(
  const View& view,
  std::vector<rendering::mesh::MeshRenderable>& renderables)
{
  const RenderData& renderData = m_appData.renderData();
  const std::optional<ImgSegPair> maybeImgSegPair = raycastImageForView(view);
  if (!maybeImgSegPair || !maybeImgSegPair->first) {
    return false;
  }

  const Image* image = m_appData.image(*maybeImgSegPair->first);
  if (!image) {
    return false;
  }

  const rendering::mesh::MeshCrosshairsGlyphInputs inputs{
    .showCrosshairsIn3D = renderData.m_showCrosshairsIn3D,
    .cameraFollowsCrosshairs = view.threeDState().m_viewPositionFollowsCrosshairs,
    .diameterVoxelDiagonals = renderData.m_crosshairs3DGlyphDiameterVoxelDiagonals,
    .voxelDiagonalWorld = glm::length(image->header().spacing()),
    .color = renderData.m_crosshairsColor};
  if (!rendering::mesh::shouldRenderMeshCrosshairsGlyph(inputs)) {
    return false;
  }

  const rendering::mesh::MeshHandle& handle = crosshairsSphereMeshHandle();
  if (!m_meshGpuStore.lookup(handle)) {
    if (!m_meshGpuStore.uploadOrReplace(rendering::mesh::makeSphereMesh(1.0f, 16, 32), handle)) {
      return false;
    }
  }

  renderables.push_back(rendering::mesh::makeSphereGlyphRenderable(
    handle,
    m_appData.state().worldCrosshairs().worldOrigin(),
    rendering::mesh::meshCrosshairsSphereGlyphStyle(inputs)));
  return true;
}

void Rendering::renderMeshCrosshairsForView(const View& view)
{
  std::vector<rendering::mesh::MeshRenderable> renderables;
  appendMeshCrosshairsRenderableForView(view, renderables);
  if (renderables.empty()) {
    return;
  }

  rendering::mesh::MeshScene scene;
  scene.setRenderables(std::move(renderables));

  const rendering::mesh::MeshRenderList list = rendering::mesh::buildRenderList(scene.renderables());
  drawMeshRenderListForView(view, list);
}
