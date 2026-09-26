#include "rendering/Rendering.h"

#include "common/UuidUtility.h"
#include "image/Image.h"
#include "logic/annotation/LandmarkGroup.h"
#include "logic/app/Data.h"
#include "logic/app/DataHelper.h"
#include "logic/app/DeformationWarp.h"
#include "rendering/PrivateMethods.h"
#include "rendering/mesh/MeshGlyphs.h"
#include "rendering/mesh/MeshLandmarkPolicy.h"
#include "rendering/mesh/MeshPrimitives.h"
#include "rendering/mesh/MeshScene.h"
#include "windowing/View.h"

#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include <vector>

namespace
{

const rendering::mesh::MeshHandle& landmarkSphereMeshHandle()
{
  static const rendering::mesh::MeshHandle handle{.uid = generateRandomUuid(), .geometryVersion = 1};
  return handle;
}

glm::mat4 world_T_landmarkSpace(const Image& image, const LandmarkGroup& group)
{
  return group.getInVoxelSpace() ? image.transformations().worldDef_T_pixel()
                                 : image.transformations().worldDef_T_subject();
}

} // namespace

void Rendering::renderMeshLandmarksForView(const View& view)
{
  if (view.visibleImages().empty()) {
    return;
  }
  const AABB<float> sceneBounds =
    data::computeWorldAABBoxEnclosingImages(m_appData, ImageSelection::VisibleImagesInView, &view);
  const float sceneDiagonalWorld = glm::length(sceneBounds.second - sceneBounds.first);
  const float radiusScenePercent = m_appData.renderSettings().m_landmarkSphereRadiusScenePercent;
  if (sceneDiagonalWorld <= 0.0f || radiusScenePercent <= 0.0f) {
    return;
  }

  const rendering::mesh::MeshHandle& handle = landmarkSphereMeshHandle();
  std::vector<rendering::mesh::MeshRenderable> renderables;

  for (const uuids::uuid& imageUid : view.visibleImages()) {
    const Image* image = m_appData.image(imageUid);
    if (!image) {
      continue;
    }

    for (const auto& groupUid : m_appData.imageToLandmarkGroupUids(imageUid)) {
      const LandmarkGroup* group = m_appData.landmarkGroup(groupUid);
      if (!group) {
        spdlog::error("Null landmark group {} assigned to image {}", groupUid, imageUid);
        continue;
      }

      const glm::mat4 world_T_landmark = world_T_landmarkSpace(*image, *group);
      for (const auto& [unusedIndex, point] : group->getPoints()) {
        (void)unusedIndex;
        const rendering::mesh::MeshLandmarkGlyphInputs inputs{
          .groupVisible = group->getVisibility(),
          .pointVisible = point.getVisibility(),
          .groupColorOverride = group->getColorOverride(),
          .groupOpacity = group->getOpacity(),
          .radiusScenePercent = radiusScenePercent,
          .sceneDiagonalWorld = sceneDiagonalWorld,
          .groupColor = group->getColor(),
          .pointColor = point.getColor()};
        if (!rendering::mesh::shouldRenderMeshLandmarkGlyph(inputs)) {
          continue;
        }

        const glm::vec4 worldPoint = world_T_landmark * glm::vec4{point.getPosition(), 1.0f};
        const glm::vec4 displayWorldPoint =
          deformation_warp::forwardWarpDisplayWorldPosition(m_appData, imageUid, worldPoint);
        const glm::vec3 centerWorld = glm::vec3{displayWorldPoint / displayWorldPoint.w};

        renderables.push_back(rendering::mesh::makeSphereGlyphRenderable(
          handle,
          centerWorld,
          rendering::mesh::meshLandmarkSphereGlyphStyle(inputs)));
      }
    }
  }

  if (renderables.empty()) {
    return;
  }
  if (
    !m_meshResources.lookup(handle) &&
    !m_meshResources.uploadOrReplace(rendering::mesh::makeSphereMesh(1.0f, 12, 24), handle))
  {
    return;
  }

  rendering::mesh::MeshScene scene;
  scene.setRenderables(std::move(renderables));
  const rendering::mesh::MeshRenderList list = rendering::mesh::buildRenderList(scene.renderables());
  drawMeshRenderListForView(view, list);
}
