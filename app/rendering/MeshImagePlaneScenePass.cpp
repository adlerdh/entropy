#include "rendering/Rendering.h"

#include "common/UuidUtility.h"
#include "image/Image.h"
#include "logic/app/Data.h"
#include "rendering/PrivateMethods.h"
#include "rendering/RenderData.h"
#include "rendering/mesh/MeshImagePlaneRenderList.h"
#include "rendering/mesh/MeshImagePlaneRenderable.h"
#include "rendering/mesh/MeshImagePlaneScene.h"
#include "rendering/mesh/MeshScene.h"
#include "rendering/utility/gl/GLBufferTypes.h"
#include "windowing/View.h"

#include <glm/vec3.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace
{

template<typename Value>
void hashCombine(std::size_t& seed, const Value& value)
{
  seed ^= std::hash<Value>{}(value) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
}

void hashVec3(std::size_t& seed, const glm::vec3& value)
{
  hashCombine(seed, value.x);
  hashCombine(seed, value.y);
  hashCombine(seed, value.z);
}

std::uint64_t geometryVersionForImagePlane(
  const Image& image,
  const rendering::mesh::MeshImagePlaneOrientation orientation,
  const glm::vec3& worldCrosshairs)
{
  std::size_t seed = 0;
  hashCombine(seed, image.geometryRevision());
  hashCombine(seed, static_cast<int>(orientation));
  hashVec3(seed, worldCrosshairs);
  return seed;
}

} // namespace

std::size_t Rendering::MeshImagePlaneHandleKeyHash::operator()(const MeshImagePlaneHandleKey& key) const
{
  std::size_t seed = 0;
  hashCombine(seed, key.imageUid);
  hashCombine(seed, static_cast<int>(key.orientation));
  return seed;
}

void Rendering::renderMeshImagePlanesForView(const View& view)
{
  if (ViewType::ThreeD != view.viewType() || !view.threeDState().m_showImagePlanes) {
    return;
  }

  const std::optional<ImgSegPair> maybeImgSegPair = raycastImageForView(view);
  if (!maybeImgSegPair || !maybeImgSegPair->first) {
    return;
  }

  const uuids::uuid& imageUid = *maybeImgSegPair->first;
  const Image* image = m_appData.image(imageUid);
  if (!image) {
    return;
  }

  const auto uniformsIt = m_appData.renderData().m_uniforms.find(imageUid);
  if (std::end(m_appData.renderData().m_uniforms) == uniformsIt) {
    return;
  }

  static const std::vector<rendering::mesh::MeshImagePlaneOrientation> sk_orientations{
    rendering::mesh::MeshImagePlaneOrientation::Axial,
    rendering::mesh::MeshImagePlaneOrientation::Coronal,
    rendering::mesh::MeshImagePlaneOrientation::Sagittal};

  const glm::vec3 worldCrosshairs = m_appData.state().worldCrosshairs().worldOrigin();
  const rendering::mesh::MeshImagePlaneSceneInputs inputs{
    .worldCrosshairs = worldCrosshairs,
    .world_T_pixel = image->transformations().worldDef_T_pixel(),
    .pixel_T_world = image->transformations().pixel_T_worldDef(),
    .texture_T_world = uniformsIt->second.imgTexture_T_world,
    .pixelBoxCorners = image->header().pixelBBoxCorners(),
    .orientations = sk_orientations};
  const std::vector<rendering::mesh::MeshImagePlaneSceneMesh> meshes =
    rendering::mesh::buildOrthogonalImagePlaneSceneMeshes(inputs);

  std::vector<rendering::mesh::MeshImagePlaneRenderable> renderables;
  renderables.reserve(meshes.size());

  const ImageSettings& settings = image->settings();
  const uint32_t activeComponent = settings.activeComponent();
  const uint32_t activeTimePoint = image->timeAxis().clamp(settings.activeTimePoint());

  for (const rendering::mesh::MeshImagePlaneSceneMesh& mesh : meshes) {
    const MeshImagePlaneHandleKey handleKey{.imageUid = imageUid, .orientation = mesh.orientation};
    auto [handleIt, inserted] = m_meshImagePlaneHandles.emplace(
      handleKey,
      rendering::mesh::MeshHandle{.uid = generateRandomUuid(), .geometryVersion = 0});
    (void)inserted;

    rendering::mesh::MeshHandle handle = handleIt->second;
    handle.geometryVersion = geometryVersionForImagePlane(*image, mesh.orientation, worldCrosshairs);

    if (!m_meshGpuStore.lookup(handle)) {
      if (!m_meshGpuStore.uploadOrReplace(mesh.mesh, handle, BufferUsagePattern::DynamicDraw)) {
        continue;
      }
      handleIt->second = handle;
    }

    renderables.push_back(rendering::mesh::makeImagePlaneRenderable(
      handle,
      glm::mat4{1.0f},
      rendering::mesh::MeshImagePlaneTexture{
        .imageUid = imageUid,
        .component = activeComponent,
        .timePoint = activeTimePoint},
      true));
  }

  if (renderables.empty()) {
    return;
  }

  rendering::mesh::MeshScene scene;
  scene.setImagePlaneRenderables(std::move(renderables));
  const rendering::mesh::MeshImagePlaneRenderList list =
    rendering::mesh::buildImagePlaneRenderList(scene.imagePlaneRenderables());
  drawMeshImagePlaneRenderListForView(view, list);
}
