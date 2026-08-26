#include "rendering/Rendering.h"

#include "common/UuidUtility.h"
#include "image/Image.h"
#include "logic/app/Data.h"
#include "rendering/PrivateMethods.h"
#include "rendering/RenderData.h"
#include "rendering/mesh/MeshCrosshairsPolicy.h"
#include "rendering/mesh/MeshGeneration.h"
#include "rendering/mesh/MeshRenderableFactory.h"
#include "rendering/mesh/MeshScene.h"
#include "windowing/View.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
  const RenderData& renderData = m_appData.renderData();
  const std::optional<ImgSegPair> maybeImgSegPair = meshSceneImageForView(view);
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
    .lengthVoxelDiagonals = renderData.m_crosshairs3DGlyphLengthVoxelDiagonals,
    .voxelDiagonalWorld = glm::length(image->header().spacing())};
  if (!rendering::mesh::shouldRenderMeshCrosshairsGlyph(inputs)) {
    return false;
  }

  const rendering::mesh::MeshHandle& handle = crosshairsAxisMeshHandle();
  if (!m_meshGpuStore.lookup(handle)) {
    const std::optional<rendering::mesh::MeshData> mesh = rendering::mesh::generateCrosshairsAxisMesh(0.15);
    if (!mesh || !m_meshGpuStore.uploadOrReplace(*mesh, handle)) {
      return false;
    }
  }

  const rendering::mesh::MeshCrosshairsGlyphStyle style = rendering::mesh::meshCrosshairsGlyphStyle(inputs);
  const glm::vec3 center = m_appData.state().worldCrosshairs().worldOrigin();
  const glm::mat4 base = glm::translate(glm::mat4{1.0f}, center);
  const std::array rotations{
    glm::rotate(glm::mat4{1.0f}, glm::half_pi<float>(), glm::vec3{0.0f, 1.0f, 0.0f}),
    glm::rotate(glm::mat4{1.0f}, -glm::half_pi<float>(), glm::vec3{1.0f, 0.0f, 0.0f}),
    glm::mat4{1.0f}};
  const std::array colors{
    glm::vec4{1.0f, 0.0f, 0.0f, 1.0f},
    glm::vec4{0.0f, 1.0f, 0.0f, 1.0f},
    glm::vec4{0.0f, 0.0f, 1.0f, 1.0f}};
  const glm::mat4 scale =
    glm::scale(glm::mat4{1.0f}, glm::vec3{style.radiusWorld, style.radiusWorld, style.halfLengthWorld});
  for (std::size_t axis = 0u; axis < rotations.size(); ++axis) {
    rendering::mesh::MeshMaterial material;
    material.baseColor = colors[axis];
    renderables.push_back(rendering::mesh::makeIsosurfaceRenderable(
      handle,
      base * rotations[axis] * scale,
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
