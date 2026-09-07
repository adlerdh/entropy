#include "rendering/Rendering.h"

#include "common/UuidUtility.h"
#include "logic/app/Data.h"
#include "logic/app/DataHelper.h"
#include "rendering/PrivateMethods.h"
#include "rendering/RenderResources.h"
#include "rendering/RenderSettings.h"
#include "rendering/mesh/MeshCrosshairsPolicy.h"
#include "rendering/mesh/MeshGeneration.h"
#include "rendering/mesh/MeshRenderableFactory.h"
#include "rendering/mesh/MeshScene.h"
#include "windowing/View.h"

#include <glm/geometric.hpp>
#include <array>
#include <optional>
#include <utility>
#include <vector>

namespace
{

const rendering::mesh::MeshHandle& crosshairsAxisMeshHandle()
{
  static const rendering::mesh::MeshHandle handle{.uid = generateRandomUuid(), .geometryVersion = 1};
  return handle;
}

} // namespace

bool Rendering::appendMeshCrosshairsRenderableForView(
  const View& view,
  std::vector<rendering::mesh::MeshRenderable>& renderables)
{
  const rendering::RenderSettings& renderSettings = m_appData.renderSettings();
  const std::optional<ImgSegPair> maybeImgSegPair = meshSceneImageForView(view);
  if (!maybeImgSegPair || !maybeImgSegPair->first) {
    return false;
  }

  const ImageSelection selection =
    view.visibleImages().empty() ? ImageSelection::AllLoadedImages : ImageSelection::VisibleImagesInView;
  const AABB<float> sceneBounds = data::computeWorldAABBoxEnclosingImages(m_appData, selection, &view);
  const float sceneDiagonalWorld = glm::length(sceneBounds.second - sceneBounds.first);

  const rendering::mesh::MeshCrosshairsGlyphInputs inputs{
    .showCrosshairsIn3D = renderSettings.m_showCrosshairsIn3D,
    .cameraFollowsCrosshairs = view.threeDState().m_viewPositionFollowsCrosshairs,
    .diameterScenePercent = renderSettings.m_crosshairs3DGlyphDiameterScenePercent,
    .lengthScenePercent = renderSettings.m_crosshairs3DGlyphLengthScenePercent,
    .sceneDiagonalWorld = sceneDiagonalWorld};
  if (!rendering::mesh::shouldRenderMeshCrosshairsGlyph(inputs)) {
    return false;
  }

  const rendering::mesh::MeshHandle& handle = crosshairsAxisMeshHandle();
  if (!m_meshResources.lookup(handle)) {
    const std::optional<rendering::mesh::MeshData> mesh = rendering::mesh::generateCrosshairsAxisMesh(0.15);
    if (!mesh || !m_meshResources.uploadOrReplace(*mesh, handle)) {
      return false;
    }
  }

  const rendering::mesh::MeshCrosshairsGlyphStyle style = rendering::mesh::meshCrosshairsGlyphStyle(inputs);
  const std::array transforms =
    rendering::mesh::meshCrosshairsAxisWorldTransforms(m_appData.state().worldCrosshairs().world_T_frame(), style);
  const std::array colors{
    glm::vec4{1.0f, 0.0f, 0.0f, 1.0f},
    glm::vec4{0.0f, 1.0f, 0.0f, 1.0f},
    glm::vec4{0.0f, 0.0f, 1.0f, 1.0f}};
  for (std::size_t axis = 0u; axis < transforms.size(); ++axis) {
    rendering::mesh::MeshMaterial material;
    material.baseColor = colors[axis];
    renderables.push_back(rendering::mesh::makeIsosurfaceRenderable(
      handle,
      transforms[axis],
      rendering::mesh::IsosurfaceMeshStyle{
        .material = material,
        .compositingMode = rendering::mesh::MeshCompositingMode::Opaque,
        .backfaceCulling = true,
        .visible = style.visible}));
  }
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
