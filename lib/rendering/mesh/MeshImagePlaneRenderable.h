#pragma once

#include "rendering/mesh/MeshHandle.h"
#include "rendering/mesh/MeshImagePlaneScene.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <uuid.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace rendering::mesh
{

/** Return the DDP-only ULP tie-break for an image layer and orthogonal plane orientation. */
constexpr uint32_t imagePlaneDdpDepthOrder(
  const std::size_t imageLayer,
  const MeshImagePlaneOrientation orientation) noexcept
{
  const std::size_t orientationLayer = [&]() constexpr {
    switch (orientation) {
      case MeshImagePlaneOrientation::Axial:
        return 0u;
      case MeshImagePlaneOrientation::Coronal:
        return 1u;
      case MeshImagePlaneOrientation::Sagittal:
        return 2u;
    }
    return 0u;
  }();
  constexpr std::size_t k_maxImageLayer = (static_cast<std::size_t>(std::numeric_limits<uint32_t>::max()) - 3u) / 3u;
  const std::size_t boundedImageLayer = imageLayer < k_maxImageLayer ? imageLayer : k_maxImageLayer;
  // Reserve zero for ordinary geometry. Every plane receives at least a one-ULP tie-break so a geometric
  // plane/surface intersection cannot collapse both fragments into one GL_MAX boundary layer.
  return static_cast<uint32_t>(3u * boundedImageLayer + orientationLayer + 1u);
}

/** Apply the same representable-float ordering used by the image-plane DDP shaders. */
constexpr float orderedImagePlaneDdpDepth(const float fragmentDepth, const uint32_t order) noexcept
{
  const float boundedDepth = fragmentDepth <= 0.0f ? 0.0f : (fragmentDepth >= 1.0f ? 1.0f : fragmentDepth);
  const uint32_t depthBits = std::bit_cast<uint32_t>(boundedDepth);
  return std::bit_cast<float>(order < depthBits ? depthBits - order : 0u);
}

/** Return border opacity gated by source-image visibility and modulated by the plane's view angle. */
constexpr float
imagePlaneBorderOpacity(const bool bordersVisible, const float imageOpacity, const float viewOpacity = 1.0f) noexcept
{
  return bordersVisible && imageOpacity > 0.0f && viewOpacity > 0.0f ? viewOpacity : 0.0f;
}

/**
 * @brief Image texture input sampled by a mesh image-plane renderable
 */
struct MeshImagePlaneTexture
{
  uuids::uuid imageUid;                       //!< Image that owns the sampled texture
  std::optional<uuids::uuid> segmentationUid; //!< Segmentation rendered on top of the image plane
  uint32_t component = 0;                     //!< Image component texture to sample
  uint32_t timePoint = 0;                     //!< Time point represented by the uploaded texture
};

/**
 * @brief Renderable for a textured image plane in a 3D mesh scene
 *
 * Image planes are intentionally separate from material-shaded `MeshRenderable` surfaces. They bind image textures and
 * use the image-sampling shader path, while ordinary meshes use material lighting shaders.
 */
struct MeshImagePlaneRenderable
{
  MeshHandle mesh;                          //!< Plane mesh with positions and image texture coordinates
  glm::mat4 world_T_mesh = glm::mat4{1.0f}; //!< Transform from mesh coordinates to world coordinates
  glm::vec3 centerWorld = glm::vec3{0.0f};  //!< Approximate center used for camera-depth sorting
  std::array<glm::vec3, 6> boundaryWorld{}; //!< Ordered perimeter vertices used for analytic border rendering
  uint32_t boundaryVertexCount = 0u;        //!< Number of valid perimeter vertices
  MeshImagePlaneOrientation orientation = MeshImagePlaneOrientation::Axial; //!< Orthogonal plane orientation
  MeshImagePlaneTexture texture;                                            //!< Image texture sampled by the plane
  uint32_t ddpDepthOrder = 0u;             //!< DDP-only ULP tie-break; larger values composite in front
  float opacityMultiplier = 1.0f;          //!< Additional opacity multiplier applied by the 3D view
  glm::vec4 borderColor = glm::vec4{0.0f}; //!< Premultiplied in the shader; zero alpha disables the analytic border
  float borderWidthPixels = 0.0f;          //!< Inward screen-space border width in device pixels
  bool shadingEnabled = true;              //!< Whether headlight shading is applied to the plane
  bool visible = true;                     //!< Whether the plane participates in image-plane draw lists
};

/**
 * @brief Build a textured image-plane renderable
 * @param mesh Uploaded or uploadable plane mesh handle
 * @param world_T_mesh Transform from mesh coordinates to world coordinates
 * @param centerWorld Approximate center used for camera-depth sorting
 * @param texture Image texture selection
 * @param opacityMultiplier Additional opacity multiplier applied by the 3D view
 * @param shadingEnabled Whether headlight shading is applied to the plane
 * @param visible Whether the image plane is visible
 * @param orientation Orthogonal plane orientation
 * @return Image-plane renderable
 */
MeshImagePlaneRenderable makeImagePlaneRenderable(
  const MeshHandle& mesh,
  const glm::mat4& world_T_mesh,
  const glm::vec3& centerWorld,
  const MeshImagePlaneTexture& texture,
  float opacityMultiplier = 1.0f,
  bool shadingEnabled = true,
  bool visible = true,
  MeshImagePlaneOrientation orientation = MeshImagePlaneOrientation::Axial);

/**
 * @brief Return whether an image-plane renderable has enough state to be drawn
 * @param renderable Image-plane renderable
 * @return True when the renderable is visible and references a non-nil image and mesh
 */
bool isDrawableImagePlaneRenderable(const MeshImagePlaneRenderable& renderable) noexcept;

} // namespace rendering::mesh
