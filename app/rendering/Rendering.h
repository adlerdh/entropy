#pragma once

#include "common/Types.h"
#include "common/UuidRange.h"
#include "logic/camera/CameraTypes.h"
#include "rendering/PixelEdgeRenderer.h"
#include "rendering/ascii/AsciiRenderer.h"
#include "rendering/common/ShaderProviderType.h"
#include "rendering/common/ShaderType.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/containers/Uniforms.h"

#include <glm/fwd.hpp>
#include <uuid.h>

#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

class AppData;
class GLBufferTexture;
class GLTexture;
class IDrawable;
class IRenderer;
class View;

struct NVGcontext;

/**
 * @brief Encapsulates all rendering
 * @todo Split this giant class apart
 */
class Rendering
{
public:
  Rendering(AppData&);
  ~Rendering();

  /// Initialization
  void init();

  /// Create image and segmentation textures
  void initTextures();

  /// @brief Framerate limiter
  /// Manual frame limiting can help if we want non-standard framerates (e.g., 30 FPS)
  /// @param[in, out] lastFrameTime Time point of last rendered frame.
  /// It is updated in this function.
  using Clock = std::chrono::steady_clock;
  void framerateLimiter(std::chrono::time_point<Clock>& lastFrameTime);

  /// Render the scene
  void render();

  /// Update all texture interpolation parameters for the active image component
  void updateImageInterpolation(const uuids::uuid& imageUid);

  /// Update the texture interpolation parameters for the given image color map
  void updateImageColorMapInterpolation(std::size_t cmapIndex);

  /// Update image uniforms after any settings have changed
  void updateImageUniforms(uuid_range_t imageUids);
  void updateImageUniforms(const uuids::uuid& imageUid);

  /// Update the metric uniforms after any settings have changed
  void updateMetricUniforms();

  /// Update a label color table texture
  void updateLabelColorTableTexture(size_t tableIndex);

  /// Updates the texture representation of a segmentation
  void updateSegTexture(
    const uuids::uuid& segUid,
    const ComponentType& compType,
    const glm::uvec3& startOffsetVoxel,
    const glm::uvec3& sizeInVoxels,
    const void* data);

  /// @todo Is this function needed?
  void updateSegTextureWithInt64Data(
    const uuids::uuid& segUid,
    const ComponentType& compType,
    const glm::uvec3& startOffsetVoxel,
    const glm::uvec3& sizeInVoxels,
    const int64_t* data);

  void updateBrushPreviewTexture(
    const uuids::uuid& imageUid,
    const uuids::uuid& segUid,
    const ComponentType& compType,
    const glm::uvec3& sizeInVoxels,
    const glm::mat4& voxel_T_world,
    const glm::vec4& color,
    bool allowFill,
    const int64_t* data);

  void clearBrushPreviewTextures();
  void hideBrushPreviewTextures();

  void updateImageTexture(
    const uuids::uuid& imageUid,
    uint32_t comp,
    const ComponentType& compType,
    const glm::uvec3& startOffsetVoxel,
    const glm::uvec3& sizeInVoxels,
    const void* data);

  bool createLabelColorTableTexture(const uuids::uuid& labelTableUid);

  bool removeSegTexture(const uuids::uuid& segUid);

  /// Get/set the overlay visibility
  bool showVectorOverlays() const;
  void setShowVectorOverlays(bool show);

private:
  // Number of images rendered per metric view
  static constexpr std::size_t NUM_METRIC_IMAGES = 2;

  using ImgSegPair = std::pair<std::optional<uuids::uuid>, std::optional<uuids::uuid>>;

  // Vector of current image/segmentation pairs rendered by image shaders
  using CurrentImages = std::vector<ImgSegPair>;

  void setupOpenGLState();

  void createShaderPrograms();
  bool createRaycastIsoProgram(GLShaderProgram& program, bool warped);

  /**
   * @brief Render data associated with images: image slices, segmentations, annotations, and
   * landmarks.
   */
  void renderImageData();

  /// Render all images/landmarks/annotations for a view
  void renderAllImagesForView(
    const View& view,
    const FrameBounds& miewportViewBounds,
    const glm::vec3& worldOffsetXhairs,
    bool renderLandmarkAndAnnotationOverlays = true,
    bool renderImageBorders = true,
    bool allowImagePostProcessing = true);
  void renderAllImageBordersForView(
    const View& view,
    const FrameBounds& miewportViewBounds,
    const glm::vec3& worldOffsetXhairs);
  void renderAllLandmarksForView(
    const View& view,
    const FrameBounds& miewportViewBounds,
    const glm::vec3& worldOffsetXhairs);
  void renderAllAnnotationsForView(
    const View& view,
    const FrameBounds& miewportViewBounds,
    const glm::vec3& worldOffsetXhairs);

  void renderVectorOverlays();

  /**
   * @brief Render shader-based warped-grid overlays for vector-field images in one view.
   *
   * @param view View receiving the overlay.
   * @param worldOffsetXhairs Crosshairs position including the view slice offset.
   * @param displayModeUniform Shader comparison-mode value for the current view.
   * @param sourceImages Original image ids to test for vector-field grid overlays.
   */
  void renderVectorWarpedGridOverlaysForView(
    const View& view,
    const glm::vec3& worldOffsetXhairs,
    int displayModeUniform,
    const CurrentImages& sourceImages);

  void renderOneImage(
    const View& view,
    const glm::vec3& worldOffsetXhairs,
    GLShaderProgram& program,
    const CurrentImages& I,
    bool showEdges);

  void renderOneImage_overlays(
    const View& view,
    const FrameBounds& miewportViewBounds,
    const glm::vec3& worldOffsetXhairs,
    const CurrentImages& I,
    bool renderLandmarkAndAnnotationOverlays,
    bool renderImageBorders);

  void renderBrushPreview(const View& view, const glm::vec3& worldOffsetXhairs, const ImgSegPair& imgSegPair);

  void volumeRenderOneImage(
    const View& view,
    GLShaderProgram& program,
    const glm::mat4& texture_T_world,
    const CurrentImages& I);

  // Bind/unbind textures for images, segmentations, and image color maps
  std::list<std::reference_wrapper<GLTexture>> bindScalarImageTextures(const ImgSegPair& P);
  std::list<std::reference_wrapper<GLTexture>> bindColorImageTextures(const ImgSegPair& P);
  std::list<std::reference_wrapper<GLTexture>> bindSegTextures(const ImgSegPair& P);
  std::list<std::reference_wrapper<GLTexture>> bindDeformationTextures(const uuids::uuid& defUid);
  std::list<std::reference_wrapper<GLTexture>> bindDeformationTextures(
    const uuids::uuid& defUid,
    const Uniforms::SamplerIndexVectorType& samplers);
  void unbindTextures(const std::list<std::reference_wrapper<GLTexture>>& textures);

  bool ensureDeformationTexture(const uuids::uuid& defUid);
  std::optional<uuids::uuid> activeRenderableDeformationUid(const uuids::uuid& imageUid);
  void setDeformationUniforms(
    GLShaderProgram& program,
    const uuids::uuid& imageUid,
    const uuids::uuid& defUid,
    const glm::mat4& sampleTex_T_world) const;
  void setMetricDeformationUniforms(
    GLShaderProgram& program,
    std::size_t slot,
    const uuids::uuid& imageUid,
    const uuids::uuid& defUid,
    const glm::mat4& sampleTex_T_world) const;

  // Bind/unbind metric images and color map
  std::list<std::reference_wrapper<GLTexture>> bindMetricImageTextures(
    const CurrentImages& I,
    const ViewRenderMode& metricType);

  // Bind/unbind buffer textures (e.g. label color tables)
  std::list<std::reference_wrapper<GLBufferTexture>> bindSegBufferTextures(const ImgSegPair& p);
  void unbindBufferTextures(const std::list<std::reference_wrapper<GLBufferTexture>>& textures);

  // Get current image and segmentation UIDs to render in the metric shaders
  CurrentImages getImageAndSegUidsForMetricShaders(const std::list<uuids::uuid>& metricImageUids) const;

  // Get current image and segmentation UIDs to render in the image shaders
  CurrentImages getImageAndSegUidsForImageShaders(const std::list<uuids::uuid>& imageUids) const;

  float raycastSamplingFactorForCurrentFrame();
  void updateAdaptiveRaycastSampling();
  void recordCompletedRaycastFrame(double renderFrameSeconds);
  bool beginRaycastTiming();
  void endRaycastTiming(bool timingActive);

  AppData& m_appData;

  // NanoVG context for vector graphics (owned by this class)
  NVGcontext* m_nvg;

  // ASCII post-processing pipeline (atlas, FBOs, per-frame render)
  AsciiRenderer m_asciiRenderer;

  // Pixel-space image-edge post-processing pipeline
  PixelEdgeRenderer m_pixelEdgeRenderer;

  std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>> m_shaderPrograms;
  std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>> m_shaderPrograms2D;

  GLShaderProgram m_raycastIsoProgram;
  GLShaderProgram m_raycastIsoWarpedProgram;

  unsigned int m_raycastTimerQuery;
  bool m_raycastTimerQueryPending;
  bool m_raycastRenderedThisFrame;
  bool m_adaptiveRaycastInitialized;
  bool m_adaptiveRaycastWasEnabled;
  float m_lastManualRaycastSamplingFactor;
  float m_adaptiveRaycastSamplingFactor;
  double m_smoothedRaycastSeconds;
  double m_smoothedFrameSeconds;
  double m_adaptiveFrameWindowSeconds;
  int m_adaptiveFrameWindowCount;

  /// Is the application done loading images?
  bool m_isAppDoneLoadingImages;

  bool m_showOverlays;

  void updateIsosurfaceDataFor3d(AppData& appData, const uuids::uuid& imageUid);

#if 0
  using DrawableProviderType = std::function<IDrawable*()>;

  std::unique_ptr<IRenderer> m_renderer;
  std::unique_ptr<ShaderProgramContainer> m_shaderPrograms;
  std::unique_ptr<MeshRecord> m_meshRecord;
  std::shared_ptr<MeshGpuRecord> m_cylinderGpuMeshRecord;
  std::unique_ptr<BasicMesh> m_basicMesh;

  ShaderProgramActivatorType m_shaderActivator;
  UniformsProviderType m_uniformsProvider;
  DrawableProviderType m_rootDrawableProvider;
  DrawableProviderType m_overlayDrawableProvider;
#endif
};
