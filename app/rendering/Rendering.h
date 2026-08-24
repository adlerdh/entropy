#pragma once

#include "common/Types.h"
#include "common/UuidRange.h"
#include "logic/camera/CameraTypes.h"
#include "rendering/PixelEdgeRenderer.h"
#include "rendering/ascii/AsciiRenderer.h"
#include "rendering/common/ShaderType.h"
#include "rendering/mesh/MeshCache.h"
#include "rendering/mesh/AmbientOcclusionResources.h"
#include "rendering/mesh/MeshExtractionQueue.h"
#include "rendering/mesh/MeshDdpResources.h"
#include "rendering/mesh/MeshGpuStore.h"
#include "rendering/mesh/MeshImagePlaneRenderList.h"
#include "rendering/mesh/MeshImagePlaneScene.h"
#include "rendering/mesh/MeshKeys.h"
#include "rendering/mesh/MeshRenderer.h"
#include "rendering/mesh/MeshResourceLifecycle.h"
#include "rendering/mesh/MeshShadowMapResources.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/containers/Uniforms.h"

#include <glm/fwd.hpp>
#include <uuid.h>

#include <chrono>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class AppData;
class GLBufferTexture;
class GLTexture;
class Image;
class View;

struct NVGcontext;

/**
 * @brief Top-level renderer that owns GPU resources and draws every view in the current layout.
 *
 * Rendering is the integration point between application state and the lower-level drawing helpers. It owns the
 * OpenGL shader programs, texture objects, NanoVG context, ASCII renderer, and pixel-edge renderer. Most persistent
 * render settings live in AppData/RenderData; this class translates those settings into current GPU state and issues
 * the draw calls for image slices, metrics, raycast isosurfaces, overlays, segmentations, annotations, landmarks, and
 * brush previews.
 *
 * The class is intentionally non-copyable through its OpenGL ownership. It should be initialized after an OpenGL
 * context exists and destroyed before the context is torn down.
 */
class Rendering
{
public:
  /**
   * @brief Construct the renderer and create process-local rendering helpers.
   *
   * The OpenGL context must already be current. The constructor creates the NanoVG context, logs OpenGL texture limits,
   * and compiles shader programs.
   *
   * @param appData Shared application state used to read images, views, settings, and render data.
   */
  explicit Rendering(AppData& appData);

  /**
   * @brief Destroy GPU queries and the NanoVG context owned by this renderer.
   */
  ~Rendering();

  /**
   * @brief Clear renderer-owned OpenGL bindings before UI and window teardown.
   *
   * The OpenGL context must still be current. This removes stale texture, sampler, program, framebuffer, and VAO
   * bindings so platform OpenGL drivers do not validate deleted or incomplete textures during shutdown.
   */
  void prepareForShutdown();

  /**
   * @brief Initialize the initial OpenGL state and texture resources.
   */
  void init();

  /**
   * @brief Create or refresh GPU textures for currently loaded images, deformation fields, segmentations, and labels.
   */
  void initTextures();

  using Clock = std::chrono::steady_clock;

  /**
   * @brief Sleep until the requested application frame interval has elapsed.
   *
   * Manual frame limiting is used when the application is configured for a target frame rate below the display rate.
   *
   * @param[in,out] lastFrameTime Time point of the last presented frame. Updated to the current frame time.
   */
  void framerateLimiter(std::chrono::time_point<Clock>& lastFrameTime);

  /**
   * @brief Draw the current application layout.
   */
  void render();

  /**
   * @brief Update sampler interpolation for all textures that belong to one image.
   *
   * @param imageUid Image whose active component and sampling settings should be applied.
   */
  void updateImageInterpolation(const uuids::uuid& imageUid);

  /**
   * @brief Update sampler interpolation for one image color map texture.
   *
   * @param colorMapIndex Index into RenderData::m_imageColorMapTextures.
   */
  void updateImageColorMapInterpolation(std::size_t colorMapIndex);

  /**
   * @brief Update image uniforms for several images.
   *
   * @param imageUids Images whose uniforms should be recomputed.
   */
  void updateImageUniforms(const uuid_range_t& imageUids);

  /**
   * @brief Update image uniforms for one image.
   *
   * @param imageUid Image whose uniforms should be recomputed.
   */
  void updateImageUniforms(const uuids::uuid& imageUid);

  /**
   * @brief Update uniforms shared by comparison and local metric shaders.
   */
  void updateMetricUniforms();

  /**
   * @brief Export the most recently rendered ASCII clipboard payload for one view.
   *
   * @param view View whose ASCII render payload should be exported.
   * @return Clipboard payload if the view has a current ASCII render, otherwise std::nullopt.
   */
  std::optional<ClipboardPayload> exportAsciiClipboardPayloadForView(const View& view);

  /**
   * @brief Pick the nearest mesh-rendered surface in one 3D view.
   *
   * This uses the renderer-owned mesh handles and CPU mesh cache. It returns no hit when the corresponding mesh has not
   * been extracted yet, allowing callers to fall back to non-mesh picking paths.
   *
   * @param view 3D view to pick in.
   * @param viewClipPos Click position in normalized view-clip coordinates.
   * @return Nearest mesh hit position in world coordinates, or empty for no hit.
   */
  std::optional<glm::vec3> pickNearestMeshWorldPositionForView(const View& view, const glm::vec2& viewClipPos);

  /**
   * @brief Upload one label color table to its GPU buffer texture.
   *
   * @param tableIndex Index into the application label color table collection.
   */
  void updateLabelColorTableTexture(size_t tableIndex);

  /**
   * @brief Upload a voxel subregion into an existing segmentation texture.
   *
   * @param segUid Segmentation image whose texture is being updated.
   * @param compType Native component type of the uploaded data.
   * @param startOffsetVoxel First voxel in the destination texture to update.
   * @param sizeInVoxels Size of the uploaded region in voxels.
   * @param data Pointer to tightly packed voxel data for the region.
   */
  void updateSegTexture(
    const uuids::uuid& segUid,
    const ComponentType& compType,
    const glm::uvec3& startOffsetVoxel,
    const glm::uvec3& sizeInVoxels,
    const void* data);

  /**
   * @brief Upload signed 64-bit segmentation data after converting it to the segmentation texture component type.
   *
   * @param segUid Segmentation image whose texture is being updated.
   * @param compType Destination texture component type.
   * @param startOffsetVoxel First voxel in the destination texture to update.
   * @param sizeInVoxels Size of the uploaded region in voxels.
   * @param data Pointer to tightly packed signed 64-bit source label values.
   */
  void updateSegTextureWithInt64Data(
    const uuids::uuid& segUid,
    const ComponentType& compType,
    const glm::uvec3& startOffsetVoxel,
    const glm::uvec3& sizeInVoxels,
    const int64_t* data);

  /**
   * @brief Upload the current segmentation brush preview mask and metadata.
   *
   * The preview texture may grow to hold the requested size. Existing capacity is reused when possible.
   *
   * @param imageUid Image being painted.
   * @param segUid Segmentation receiving paint edits.
   * @param compType Component type used for the preview upload.
   * @param sizeInVoxels Size of the preview mask in voxels.
   * @param voxel_T_world Transform from world coordinates to preview voxel coordinates.
   * @param color Brush preview color.
   * @param allowFill Whether the preview includes fill behavior rather than brush-only behavior.
   * @param data Pointer to tightly packed signed 64-bit preview mask values.
   */
  void updateBrushPreviewTexture(
    const uuids::uuid& imageUid,
    const uuids::uuid& segUid,
    const ComponentType& compType,
    const glm::uvec3& sizeInVoxels,
    const glm::mat4& voxel_T_world,
    const glm::vec4& color,
    bool allowFill,
    const int64_t* data);

  /**
   * @brief Clear all brush preview textures and associated preview metadata.
   */
  void clearBrushPreviewTextures();

  /**
   * @brief Hide brush preview overlays without releasing their texture capacity.
   */
  void hideBrushPreviewTextures();

  /**
   * @brief Upload a voxel subregion into one image component texture.
   *
   * @param imageUid Image whose component texture is being updated.
   * @param component Image component index to update.
   * @param compType Native component type of the uploaded data.
   * @param startOffsetVoxel First voxel in the destination texture to update.
   * @param sizeInVoxels Size of the uploaded region in voxels.
   * @param data Pointer to tightly packed voxel data for the region.
   */
  void updateImageTexture(
    const uuids::uuid& imageUid,
    uint32_t component,
    const ComponentType& compType,
    const glm::uvec3& startOffsetVoxel,
    const glm::uvec3& sizeInVoxels,
    const void* data);

  /**
   * @brief Create the GPU buffer texture for one label color table.
   *
   * @param labelTableUid Label color table to upload.
   * @return True when a new texture was created, false when it already existed or the table was invalid.
   */
  bool createLabelColorTableTexture(const uuids::uuid& labelTableUid);

  /**
   * @brief Remove the GPU texture and cached layout for a segmentation.
   *
   * @param segUid Segmentation whose GPU texture should be removed.
   * @return True when the texture existed and was removed.
   */
  bool removeSegTexture(const uuids::uuid& segUid);

  /**
   * @brief Return whether vector overlays are globally visible.
   */
  bool showVectorOverlays() const;

  /**
   * @brief Set global vector overlay visibility.
   *
   * @param show True to draw vector overlays, false to suppress them.
   */
  void setShowVectorOverlays(bool show);

private:
  /// Number of image slots rendered by metric and comparison shaders.
  static constexpr std::size_t NUM_METRIC_IMAGES = 2;

  /// Pair of optional image and segmentation ids passed into image and metric shader paths.
  using ImgSegPair = std::pair<std::optional<uuids::uuid>, std::optional<uuids::uuid>>;

  /// Ordered image/segmentation pairs currently bound for one image, metric, or raycast draw.
  using CurrentImages = std::vector<ImgSegPair>;

  /// Isosurface currently being edited and therefore rendered through the live raycast path.
  struct ActiveIsosurfaceEdit
  {
    ImgSegPair imageSegPair;   //!< Image/segmentation pair that owns the edited surface
    uuids::uuid isosurfaceUid; //!< Edited isosurface
  };

  /// Logical mesh handles keyed by their geometry-producing inputs.
  using MeshGeometryKey = rendering::mesh::MeshGeometryKey;
  using MeshGeometryKeyHash = rendering::mesh::MeshGeometryKeyHash;
  using MeshHandle = rendering::mesh::MeshHandle;
  using MeshHandleMap = rendering::mesh::MeshHandleMap;

  struct MeshImagePlaneHandleKey
  {
    uuids::uuid imageUid;                                        //!< Source image rendered on the plane
    rendering::mesh::MeshImagePlaneOrientation orientation = {}; //!< Orthogonal plane orientation
    bool imageBox = false;                                       //!< Whether the handle stores the full image box

    bool operator==(const MeshImagePlaneHandleKey&) const = default;
  };

  struct MeshImagePlaneHandleKeyHash
  {
    std::size_t operator()(const MeshImagePlaneHandleKey& key) const;
  };

  using MeshImagePlaneHandleMap = std::unordered_map<MeshImagePlaneHandleKey, MeshHandle, MeshImagePlaneHandleKeyHash>;

#include "rendering/PrivateMethods.h"

  /// Shared application state. Not owned; Rendering reads and updates render-facing state through this reference.
  AppData& m_appData;

  NVGcontext* m_nvg; //!< Owned NanoVG context for vector graphics

  /// ASCII post-processing pipeline for character atlas, framebuffers, render, and clipboard export.
  AsciiRenderer m_asciiRenderer;

  PixelEdgeRenderer m_pixelEdgeRenderer; //!< Pixel-space image-edge post-processing pipeline

  /// Shader programs for the normal 3D texture rendering path, keyed by render mode and interpolation variant.
  std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>> m_shaderPrograms;

  /// Shader programs for planar 2D fallback textures that exceed GL_MAX_3D_TEXTURE_SIZE but fit GL_MAX_TEXTURE_SIZE.
  std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>> m_shaderPrograms2D;

  GLShaderProgram m_raycastIsoProgram; //!< Raycast isosurface shader for unwarped scalar image volumes

  /// Raycast isosurface shader variant that samples an inverse deformation field before sampling the scalar image.
  GLShaderProgram m_raycastIsoWarpedProgram;

  GLShaderProgram m_meshProgram; //!< Basic lit mesh shader program

  GLShaderProgram m_meshShadowDepthProgram; //!< Depth-only mesh shader for shadow-map generation

  GLShaderProgram m_meshAmbientOcclusionGeometryProgram; //!< Mesh normal/depth shader for screen-space AO

  GLShaderProgram m_meshAmbientOcclusionResolveProgram; //!< Full-screen mesh AO resolve shader

  GLShaderProgram m_meshAmbientOcclusionFilterProgram; //!< Edge-preserving mesh AO filter shader

  GLShaderProgram m_meshImagePlaneGrayLinearProgram; //!< Mesh image-plane shader for scalar 3D textures

  GLShaderProgram m_meshImagePlaneGrayLinearTexture2DProgram; //!< Mesh image-plane shader for scalar 2D textures

  GLShaderProgram m_meshImagePlaneIsoContourProgram; //!< Mesh image-plane isocontour shader for scalar 3D textures

  GLShaderProgram
    m_meshImagePlaneIsoContourTexture2DProgram; //!< Mesh image-plane isocontour shader for scalar 2D textures

  GLShaderProgram m_meshImagePlaneDdpInitProgram; //!< Mesh image-plane DDP depth initialization shader for 3D textures

  GLShaderProgram
    m_meshImagePlaneDdpInitTexture2DProgram; //!< Mesh image-plane DDP depth initialization shader for 2D textures

  GLShaderProgram m_meshImagePlaneDdpPeelProgram; //!< Mesh image-plane DDP peeling shader for 3D textures

  GLShaderProgram m_meshImagePlaneDdpPeelTexture2DProgram; //!< Mesh image-plane DDP peeling shader for 2D textures

  GLShaderProgram m_meshDdpInitProgram; //!< Mesh DDP attachment initialization shader

  GLShaderProgram m_meshDdpPeelProgram; //!< Mesh DDP per-layer peeling shader

  GLShaderProgram m_meshDdpBackBlendProgram; //!< Mesh DDP back-color accumulation shader

  GLShaderProgram m_meshDdpResolveProgram; //!< Mesh DDP final front/back resolve shader

  rendering::mesh::MeshRenderer m_meshRenderer; //!< Stateless uploaded-mesh draw helper

  rendering::mesh::MeshDdpResources m_meshDdpResources; //!< OpenGL attachments reserved for mesh DDP passes

  rendering::mesh::MeshShadowMapResources m_meshShadowMapResources; //!< OpenGL attachments for future mesh shadows

  rendering::mesh::MeshAmbientOcclusionResources m_meshAmbientOcclusionResources; //!< OpenGL attachments for mesh AO

  rendering::mesh::MeshCache m_meshCpuCache; //!< CPU-side extracted mesh cache

  rendering::mesh::MeshExtractionQueue m_meshExtractionQueue; //!< Background CPU mesh extraction queue

  rendering::mesh::MeshGpuStore m_meshGpuStore; //!< Uploaded mesh buffers for the current OpenGL context

  MeshHandleMap m_meshHandles; //!< Stable logical mesh handles keyed by geometry-producing inputs

  MeshImagePlaneHandleMap m_meshImagePlaneHandles; //!< Stable handles for dynamic 3D image-plane meshes

  bool m_isAppDoneLoadingImages; //!< True once the application has finished the startup/image-loading phase

  bool m_showOverlays; //!< Global runtime overlay switch used by the view overlay cycling actions

  /// Refresh the CPU-side isosurface arrays consumed by the 3D raycast shader for one image.
  void updateIsosurfaceDataFor3d(
    AppData& appData,
    const uuids::uuid& imageUid,
    const std::optional<uuids::uuid>& onlyIsosurfaceUid = std::nullopt);
};
