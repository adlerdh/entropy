#pragma once

#include "common/Types.h"
#include "common/UuidRange.h"
#include "logic/camera/CameraTypes.h"
#include "rendering/AsciiAtlas.h"
#include "rendering/AsciiAtlasBaker.h"
#include "rendering/common/ShaderProviderType.h"
#include "rendering/common/ShaderType.h"
#include "rendering/utility/gl/GLFrameBufferObject.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/gl/GLVertexArrayObject.h"

#include <glm/fwd.hpp>
#include <glm/vec2.hpp>
#include <uuid.h>

#include <functional>
#include <list>
#include <optional>
#include <utility>
#include <vector>

class AppData;
class GLBufferTexture;
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

  /// Build (or rebuild) the ASCII glyph atlas from the embedded Cousine font
  void buildAsciiAtlas();

  /// Allocate/reallocate the scene FBO color attachment when the device size changes
  void ensureSceneFboSize(glm::ivec2 deviceSize);

  /// Allocate/reallocate the cell-mean FBO when cell size or viewport changes
  void ensureAsciiCellFbo(glm::ivec2 viewSizeDevPx, glm::vec2 cellSizePxDev);

  void renderImageDataAscii();

  bool createRaycastIsoProgram(GLShaderProgram& program);

  /**
   * @brief Render data associated with images: image slices, segmentations, annotations, and landmarks.
   */
  void renderImageData();

  /// Render all images/landmarks/annotations for a view
  void renderAllImagesForView(const View& view, const FrameBounds& miewportViewBounds, const glm::vec3& worldOffsetXhairs);
  void renderAllLandmarksForView(const View& view, const FrameBounds& miewportViewBounds, const glm::vec3& worldOffsetXhairs);
  void renderAllAnnotationsForView(const View& view, const FrameBounds& miewportViewBounds, const glm::vec3& worldOffsetXhairs);

  void renderVectorOverlays();

  void renderOneImage(const View& view, const glm::vec3& worldOffsetXhairs,
                      GLShaderProgram& program, const CurrentImages& I, bool showEdges);

  void renderOneImage_overlays(const View& view, const FrameBounds& miewportViewBounds,
                               const glm::vec3& worldOffsetXhairs, const CurrentImages& I);

  void volumeRenderOneImage(const View& view, GLShaderProgram& program, const CurrentImages& I);

  // Bind/unbind textures for images, segmentations, and image color maps
  std::list<std::reference_wrapper<GLTexture>> bindScalarImageTextures(const ImgSegPair& P);
  std::list<std::reference_wrapper<GLTexture>> bindColorImageTextures(const ImgSegPair& P);
  std::list<std::reference_wrapper<GLTexture>> bindSegTextures(const ImgSegPair& P);
  void unbindTextures(const std::list<std::reference_wrapper<GLTexture> >& textures);

  // Bind/unbind metric images and color map
  std::list<std::reference_wrapper<GLTexture>> bindMetricImageTextures(
    const CurrentImages& I, const ViewRenderMode& metricType);

  // Bind/unbind buffer textures (e.g. label color tables)
  std::list<std::reference_wrapper<GLBufferTexture>> bindSegBufferTextures(const ImgSegPair& p);
  void unbindBufferTextures(const std::list<std::reference_wrapper<GLBufferTexture> >& textures);

  // Get current image and segmentation UIDs to render in the metric shaders
  CurrentImages getImageAndSegUidsForMetricShaders(const std::list<uuids::uuid>& metricImageUids) const;

  // Get current image and segmentation UIDs to render in the image shaders
  CurrentImages getImageAndSegUidsForImageShaders(const std::list<uuids::uuid>& imageUids) const;

  AppData& m_appData;

  // NanoVG context for vector graphics (owned by this class)
  NVGcontext* m_nvg;

  // ASCII glyph atlas, built once at init from the embedded Cousine-Regular.ttf
  AsciiAtlas m_asciiAtlas;

  // FBO and color attachment for the ASCII post-process pass
  GLFrameBufferObject m_sceneFbo;
  std::optional<GLTexture> m_sceneColorTex;
  glm::ivec2 m_sceneFboSize{0, 0};
  GLVertexArrayObject m_asciiPostVao;

  // FBO and color attachment for the ASCII cell-mean downsample pass (Pass 1.5)
  std::optional<GLTexture>       m_asciiCellMeanTex;
  std::optional<GLFrameBufferObject> m_asciiCellMeanFbo;
  glm::ivec2                     m_asciiCellMeanTexSize{0, 0};

  // 256x1 R8 LUT texture: maps luminance bin -> normalized glyph index
  // Rebuilt whenever cellPxDev changes
  std::optional<GLTexture> m_asciiLumLutTex;
  glm::vec2                m_asciiLumLutCellPx{0.0f, 0.0f};

  // Spatial glyph matching state (m_asciiSpatialMode lives in RenderData, readable by UI)
  std::optional<GLTexture>      m_asciiCellRegionsTex;        // RGBA16F, regions 0–3
  std::optional<GLTexture>      m_asciiCellRegionsTexB;       // RG16F,   regions 4–5
  std::vector<glm::vec4>        m_glyphProfilesPackedA;       // regions 0–3, N entries
  std::vector<glm::vec4>        m_glyphProfilesPackedB;       // regions 4–5 in .xy, .zw=0
  glm::vec2                     m_asciiSpatialProfileCellPx{-1.0f, -1.0f};
  std::vector<int>              m_glyphRankToIndex;          // rank -> glyph index, padded to kMaxGlyphs
  float                         m_asciiSpatialDensityWindow{3.0f};
  std::array<float, kSpatialK>  m_glyphRegionMax{};
  float                         m_lastUploadedExponent{-1.0f};
  std::vector<GlyphProfile>     m_glyphProfilesNormalized;

  std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>> m_shaderPrograms;

  GLShaderProgram m_raycastIsoProgram;

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
