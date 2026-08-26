#include "rendering/Rendering.h"

#include "common/CoordinateFrame.h"
#include "common/DirectionMaps.h"
#include "common/UuidUtility.h"
#include "image/Image.h"
#include "image/ImageHeader.h"
#include "image/ImageSettings.h"
#include "image/ImageTimeAxis.h"
#include "image/ImageTransformations.h"
#include "logic/app/Data.h"
#include "logic/app/State.h"
#include "logic/camera/Camera3DControls.h"
#include "logic/camera/CameraHelpers.h"
#include "rendering/PrivateMethods.h"
#include "rendering/RenderData.h"
#include "rendering/mesh/MeshCompositing.h"
#include "rendering/mesh/MeshData.h"
#include "rendering/mesh/MeshGpuStore.h"
#include "rendering/mesh/MeshHandle.h"
#include "rendering/mesh/MeshImagePlane.h"
#include "rendering/mesh/MeshImagePlaneRenderList.h"
#include "rendering/mesh/MeshImagePlaneRenderable.h"
#include "rendering/mesh/MeshImagePlaneScene.h"
#include "rendering/mesh/MeshMaterial.h"
#include "rendering/mesh/MeshRenderable.h"
#include "rendering/mesh/MeshRenderableFactory.h"
#include "rendering/mesh/MeshRenderList.h"
#include "rendering/mesh/MeshScene.h"
#include "rendering/utility/gl/GLBufferTypes.h"
#include "viewer/ViewTypes.h"
#include "windowing/View.h"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
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

std::array<glm::vec3, 8> transformedCorners(const std::array<glm::vec3, 8>& corners, const glm::mat4& transform)
{
  std::array<glm::vec3, 8> transformed{};
  std::ranges::transform(corners, transformed.begin(), [&transform](const glm::vec3& corner) {
    const glm::vec4 value = transform * glm::vec4{corner, 1.0f};
    return std::abs(value.w) > 1.0e-6f ? glm::vec3{value / value.w} : glm::vec3{value};
  });
  return transformed;
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

std::uint64_t geometryVersionForImageBox(const Image& image)
{
  std::size_t seed = 0;
  hashCombine(seed, image.geometryRevision());
  hashCombine(seed, 0x4b7d2a31u);
  return seed;
}

float imagePlaneBorderWidthWorld(const RenderData::ImageUniforms& uniforms) noexcept
{
  const glm::vec3 spacing = glm::abs(uniforms.voxelSpacing);
  const float minSpacing = std::min({spacing.x, spacing.y, spacing.z});
  return std::isfinite(minSpacing) && minSpacing > 0.0f ? 0.25f * minSpacing : 0.25f;
}

glm::vec3 meshPositionCenter(const rendering::mesh::MeshData& mesh) noexcept
{
  if (mesh.positions.empty()) {
    return glm::vec3{0.0f};
  }

  glm::vec3 sum{0.0f};
  for (const glm::vec3& position : mesh.positions) {
    sum += position;
  }
  return sum / static_cast<float>(mesh.positions.size());
}

} // namespace

std::size_t Rendering::MeshImagePlaneHandleKeyHash::operator()(const MeshImagePlaneHandleKey& key) const
{
  std::size_t seed = 0;
  hashCombine(seed, key.imageUid);
  hashCombine(seed, static_cast<int>(key.orientation));
  hashCombine(seed, key.imageBox);
  return seed;
}

std::vector<rendering::mesh::MeshImagePlaneRenderable> Rendering::collectMeshImagePlaneRenderablesForView(
  const View& view,
  std::vector<rendering::mesh::MeshRenderable>& borderRenderables)
{
  const bool showImagePlanes = m_appData.renderData().m_showImagePlanesIn3D && view.threeDState().m_showImagePlanes;
  const bool showImageBox = m_appData.renderData().m_raycastBackgroundEdgeBrighteningEnabled;
  const bool showImagePlaneBorders =
    m_appData.renderData().m_globalSliceIntersectionParams.renderInactiveImageViewIntersections;
  if (ViewType::ThreeD != view.viewType() || (!showImagePlanes && !showImageBox)) {
    return {};
  }

  const CurrentImages imageSegPairs = meshSceneImagesForView(view);
  if (imageSegPairs.empty()) {
    return {};
  }

  static const std::vector<rendering::mesh::MeshImagePlaneOrientation> sk_orientations{
    rendering::mesh::MeshImagePlaneOrientation::Axial,
    rendering::mesh::MeshImagePlaneOrientation::Coronal,
    rendering::mesh::MeshImagePlaneOrientation::Sagittal};

  const glm::vec3 worldCrosshairs = m_appData.state().worldCrosshairs().worldOrigin();
  const glm::vec3 viewDirectionWorld = helper::worldDirection(view.threeDCamera(), Directions::View::Back);

  std::vector<rendering::mesh::MeshImagePlaneRenderable> renderables;
  renderables.reserve(3u * imageSegPairs.size());
  borderRenderables.reserve(3u * imageSegPairs.size());

  const auto uploadImagePlaneMesh =
    [this](
      const rendering::mesh::MeshData& mesh,
      const MeshImagePlaneHandleKey& handleKey,
      const std::uint64_t geometryVersion) -> std::optional<rendering::mesh::MeshHandle> {
    auto [handleIt, inserted] = m_meshImagePlaneHandles.emplace(
      handleKey,
      rendering::mesh::MeshHandle{.uid = generateRandomUuid(), .geometryVersion = 0});
    (void)inserted;

    rendering::mesh::MeshHandle handle = handleIt->second;
    handle.geometryVersion = geometryVersion;
    if (!m_meshGpuStore.lookup(handle)) {
      if (!m_meshGpuStore.uploadOrReplace(mesh, handle, BufferUsagePattern::DynamicDraw)) {
        return std::nullopt;
      }
      handleIt->second = handle;
    }

    return handle;
  };

  std::size_t imageLayer = 0u;
  for (const ImgSegPair& imgSegPair : imageSegPairs) {
    if (!imgSegPair.first) {
      continue;
    }

    const uuids::uuid& imageUid = *imgSegPair.first;
    const Image* image = m_appData.image(imageUid);
    if (!image) {
      continue;
    }

    const auto uniformsIt = m_appData.renderData().m_uniforms.find(imageUid);
    if (std::end(m_appData.renderData().m_uniforms) == uniformsIt) {
      continue;
    }

    const std::array<glm::vec3, 8> worldCorners =
      transformedCorners(image->header().pixelBBoxCorners(), image->transformations().worldDef_T_pixel());

    const rendering::mesh::MeshImagePlaneSceneInputs inputs{
      .worldCrosshairs = worldCrosshairs,
      .world_T_pixel = image->transformations().worldDef_T_pixel(),
      .pixel_T_world = image->transformations().pixel_T_worldDef(),
      .texture_T_world = uniformsIt->second.imgTexture_T_world,
      .pixelBoxCorners = image->header().pixelBBoxCorners(),
      .orientations = sk_orientations,
      .borderWidthWorld = 0.0f};
    const std::vector<rendering::mesh::MeshImagePlaneSceneMesh> meshes =
      rendering::mesh::buildOrthogonalImagePlaneSceneMeshes(inputs);

    const ImageSettings& settings = image->settings();
    const uint32_t activeComponent = settings.activeComponent();
    const uint32_t activeTimePoint = image->timeAxis().clamp(settings.activeTimePoint());

    const auto appendBorderRenderable = [&borderRenderables, &settings](
                                          const rendering::mesh::MeshHandle& borderHandle,
                                          const glm::mat4& world_T_border,
                                          const float opacity) {
      rendering::mesh::MeshMaterial borderMaterial;
      borderMaterial.baseColor = glm::vec4{settings.borderColor(), opacity};
      borderMaterial.shadingModel = rendering::mesh::MeshShadingModel::Unlit;
      const rendering::mesh::MeshCompositingMode compositingMode =
        opacity >= 0.999f ? rendering::mesh::MeshCompositingMode::Opaque
                          : rendering::mesh::MeshCompositingMode::AlphaOverDdp;
      rendering::mesh::MeshRenderable borderRenderable = rendering::mesh::makeIsosurfaceRenderable(
        borderHandle,
        world_T_border,
        rendering::mesh::IsosurfaceMeshStyle{
          .material = borderMaterial,
          .compositingMode = compositingMode,
          .visible = opacity > 0.0f});
      borderRenderable.castsShadow = false;
      borderRenderables.push_back(std::move(borderRenderable));
    };

    if (showImagePlanes) {
      for (const rendering::mesh::MeshImagePlaneSceneMesh& mesh : meshes) {
        const std::uint64_t geometryVersion = geometryVersionForImagePlane(*image, mesh.orientation, worldCrosshairs);
        const float opacityMultiplier = m_appData.renderData().m_modulateImagePlaneOpacityWithViewAngle
                                          ? rendering::mesh::imagePlaneViewOpacityMultiplier(
                                              rendering::mesh::imagePlaneWorldNormal(mesh.orientation),
                                              viewDirectionWorld)
                                          : 1.0f;

        const std::optional<rendering::mesh::MeshHandle> handle = uploadImagePlaneMesh(
          mesh.mesh,
          MeshImagePlaneHandleKey{.imageUid = imageUid, .orientation = mesh.orientation},
          geometryVersion);
        if (!handle) {
          continue;
        }

        rendering::mesh::MeshImagePlaneRenderable renderable = rendering::mesh::makeImagePlaneRenderable(
          *handle,
          glm::mat4{1.0f},
          meshPositionCenter(mesh.mesh),
          rendering::mesh::MeshImagePlaneTexture{
            .imageUid = imageUid,
            .segmentationUid =
              m_appData.renderData().m_showSegmentationsOnImagePlanesIn3D ? imgSegPair.second : std::nullopt,
            .component = activeComponent,
            .timePoint = activeTimePoint},
          opacityMultiplier,
          m_appData.renderData().m_shadeImagePlanesIn3D,
          true,
          mesh.orientation);
        // Image selections are bottom layer first, just as in the 2D views. Coincident planes must have distinct DDP
        // depths because draw order alone cannot order fragments that share exactly the same depth bound.
        renderable.ddpDepthBias = rendering::mesh::imagePlaneDdpDepthBias(imageLayer, mesh.orientation);
        renderable.boundaryVertexCount = static_cast<uint32_t>(
          std::min<std::size_t>(renderable.boundaryWorld.size(), mesh.mesh.positions.size() - 1u));
        for (uint32_t i = 0u; i < renderable.boundaryVertexCount; ++i) {
          renderable.boundaryWorld[i] = mesh.mesh.positions[i + 1u];
        }
        const float viewModulatedBorderOpacity = rendering::mesh::imagePlaneBorderOpacity(
          showImagePlaneBorders,
          uniformsIt->second.imgOpacity,
          opacityMultiplier);
        renderable.borderColor = glm::vec4{settings.borderColor(), viewModulatedBorderOpacity};
        renderable.borderWidthPixels = viewModulatedBorderOpacity > 0.0f ? 1.0f : 0.0f;
        renderables.push_back(std::move(renderable));
      }
    }

    if (showImageBox && uniformsIt->second.imgOpacity > 0.0f) {
      const std::optional<rendering::mesh::MeshData> boxMesh =
        rendering::mesh::makeImageBoxBorderMesh(worldCorners, imagePlaneBorderWidthWorld(uniformsIt->second));
      if (boxMesh) {
        const std::optional<rendering::mesh::MeshHandle> boxHandle = uploadImagePlaneMesh(
          *boxMesh,
          MeshImagePlaneHandleKey{.imageUid = imageUid, .imageBox = true},
          geometryVersionForImageBox(*image));
        if (boxHandle) {
          appendBorderRenderable(*boxHandle, glm::mat4{1.0f}, 1.0f);
        }
      }
    }

    ++imageLayer;
  }

  return renderables;
}

void Rendering::renderMeshImagePlanesForView(const View& view)
{
  std::vector<rendering::mesh::MeshRenderable> borderRenderables;
  std::vector<rendering::mesh::MeshImagePlaneRenderable> imagePlaneRenderables =
    collectMeshImagePlaneRenderablesForView(view, borderRenderables);

  rendering::mesh::MeshScene imagePlaneScene;
  imagePlaneScene.setImagePlaneRenderables(std::move(imagePlaneRenderables));
  const rendering::mesh::MeshImagePlaneRenderList imagePlaneList =
    rendering::mesh::buildImagePlaneRenderList(imagePlaneScene.imagePlaneRenderables());

  rendering::mesh::MeshScene borderScene;
  borderScene.setRenderables(std::move(borderRenderables));
  const rendering::mesh::MeshRenderList borderList = rendering::mesh::buildRenderList(borderScene.renderables());

  if (imagePlaneList.imagePlanes.empty() && rendering::mesh::visibleRenderableCount(borderList) == 0u) {
    return;
  }

  drawMeshRenderListForView(view, borderList, &imagePlaneList);
}
