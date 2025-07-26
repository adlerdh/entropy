#pragma once

#include "common/Types.h"
#include "common/UuidRange.h"
#include "logic/camera/CameraTypes.h"
#include "rendering/common/ShaderProviderType.h"
#include "rendering/common/ShaderType.h"
#include "rendering/utility/gl/GLShaderProgram.h"

#include <glm/fwd.hpp>
#include <uuid.h>

#include <functional>
#include <list>
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

  void setupOpenGlState();

  void createShaderPrograms();
  bool createRaycastIsoSurfaceProgram(GLShaderProgram& program);

  void renderImageData();
  void renderVectorOverlays();

  void renderOneImage(const View& view, const glm::vec3& worldOffsetXhairs,
                      GLShaderProgram& program, const CurrentImages& I, bool showEdges);

  void renderOneImage_overlays(const View& view, const FrameBounds& miewportViewBounds,
                               const glm::vec3& worldOffsetXhairs, const CurrentImages& I);

  void volumeRenderOneImage(const View& view, GLShaderProgram& program, const CurrentImages& I);

  void renderAllImages(const View& view, const FrameBounds& miewportViewBounds, const glm::vec3& worldOffsetXhairs);
  void renderAllLandmarks(const View& view, const FrameBounds& miewportViewBounds, const glm::vec3& worldOffsetXhairs);
  void renderAllAnnotations(const View& view, const FrameBounds& miewportViewBounds, const glm::vec3& worldOffsetXhairs);

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

  std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>> m_shaderPrograms;

  GLShaderProgram m_raycastIsoSurfaceProgram;

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
