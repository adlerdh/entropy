#pragma once

#include "common/Types.h"
#include "rendering/TextureLayout.h"
#include "rendering/gl/VertexAttributeInfo.h"
#include "rendering/gl/VertexIndicesInfo.h"
#include "rendering/gl/GLBufferObject.h"
#include "rendering/gl/GLBufferTexture.h"
#include "rendering/gl/GLTexture.h"
#include "rendering/gl/GLVertexArrayObject.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <uuid.h>

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace rendering
{

/**
 * @brief Context-bound GPU resources and upload staging data.
 *
 * This object contains no user preferences or project settings. It must be constructed and
 * destroyed while its OpenGL context is current and used only from that context's render thread.
 */
struct RenderResources
{
  using PlanarTextureLayout = rendering::PlanarTextureLayout;

  /** Shared indexed quad geometry used by image-space passes. */
  struct Quad
  {
    Quad();

    VertexAttributeInfo m_positionsInfo;
    VertexIndicesInfo m_indicesInfo;
    GLBufferObject m_positionsObject;
    GLBufferObject m_indicesObject;
    GLVertexArrayObject m_vao;
    GLVertexArrayObject::IndexedDrawParams m_vaoParams;
  };

  /** Shared indexed circle geometry used by circular overlays. */
  struct Circle
  {
    Circle();

    VertexAttributeInfo m_positionsInfo;
    VertexIndicesInfo m_indicesInfo;
    GLBufferObject m_positionsObject;
    GLBufferObject m_indicesObject;
    GLVertexArrayObject m_vao;
    GLVertexArrayObject::IndexedDrawParams m_vaoParams;
  };

  /** Transient CPU staging and GPU texture state for a segmentation brush preview. */
  struct BrushPreview
  {
    bool visible = false;
    uuids::uuid segUid;
    uuids::uuid imageUid;
    ComponentType componentType = ComponentType::UInt8;
    glm::uvec3 size{1};
    glm::uvec3 textureCapacity{0};
    glm::mat4 texture_T_world{1.0f};
    glm::mat4 voxel_T_world{1.0f};
    glm::vec4 color{1.0f};
    bool allowFill = true;
    std::vector<uint8_t> dataU8;
    std::vector<uint16_t> dataU16;
    std::vector<uint32_t> dataU32;
    std::optional<GLTexture> texture;
  };

  RenderResources();

  /** Release resources derived from the currently loaded project while preserving reusable primitive geometry. */
  void clearLoadedData();

  /** Release textures and brush previews associated with an image or deformation field. */
  void removeImage(const uuids::uuid& imageUid);

  /** Release textures and brush previews associated with a segmentation. */
  void removeSegmentation(const uuids::uuid& segmentationUid);

  bool hasSegmentation(const uuids::uuid& segmentationUid) const noexcept;

  Quad m_quad;
  Circle m_circle;
  std::unordered_map<uuids::uuid, std::vector<GLTexture>> m_imageTextures;
  std::unordered_map<uuids::uuid, PlanarTextureLayout> m_imageTextureLayouts;
  std::unordered_map<uuids::uuid, std::unordered_map<uint32_t, GLTexture>> m_distanceMapTextures;
  std::unordered_map<uuids::uuid, GLTexture> m_segTextures;
  std::unordered_map<uuids::uuid, PlanarTextureLayout> m_segTextureLayouts;
  std::unordered_map<uuids::uuid, BrushPreview> m_brushPreviews;
  std::unordered_map<uuids::uuid, GLBufferTexture> m_labelBufferTextures;
  std::unordered_map<uuids::uuid, GLTexture> m_colormapTextures;
  GLTexture m_blankImageBlackTransparentTexture;
  GLTexture m_blankImageBlackTransparentTexture2D;
  GLTexture m_blankImageWhiteOpaqueTexture;
  GLTexture m_blankSegTexture;
  GLTexture m_blankSegTexture2D;
  GLTexture m_blankDistMapTexture;
};

} // namespace rendering
