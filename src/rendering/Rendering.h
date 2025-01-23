#ifndef RENDERING_H
#define RENDERING_H

#include "common/Types.h"
#include "common/UuidRange.h"

#include "logic/camera/CameraTypes.h"
#include "logic/records/MeshRecord.h"

#include "rendering/common/ShaderProviderType.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering_old/drawables/BasicMesh.h"
#include "rendering_old/utility/containers/ShaderProgramContainer.h"

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

  /**
     * @brief Updates the texture representation of a segmentation
     * @param segUid
     * @param compType
     * @param startOffsetVoxel
     * @param sizeInVoxels
     * @param data
     */
  void updateSegTexture(
    const uuids::uuid& segUid,
    const ComponentType& compType,
    const glm::uvec3& startOffsetVoxel,
    const glm::uvec3& sizeInVoxels,
    const void* data
  );

  /// @todo Is this function needed?
  void updateSegTextureWithInt64Data(
    const uuids::uuid& segUid,
    const ComponentType& compType,
    const glm::uvec3& startOffsetVoxel,
    const glm::uvec3& sizeInVoxels,
    const int64_t* data
  );

  void updateImageTexture(
    const uuids::uuid& imageUid,
    uint32_t comp,
    const ComponentType& compType,
    const glm::uvec3& startOffsetVoxel,
    const glm::uvec3& sizeInVoxels,
    const void* data
  );

  bool createLabelColorTableTexture(const uuids::uuid& labelTableUid);

  bool removeSegTexture(const uuids::uuid& segUid);

  /// Get/set the overlay visibility
  bool showVectorOverlays() const;
  void setShowVectorOverlays(bool show);

private:
  // Number of images rendered per metric view
  static constexpr std::size_t NUM_METRIC_IMAGES = 2;

  using ImgSegPair = std::pair<std::optional<uuids::uuid>, std::optional<uuids::uuid> >;

  // Vector of current image/segmentation pairs rendered by image shaders
  using CurrentImages = std::vector<ImgSegPair>;

  void setupOpenGlState();

  void createShaderPrograms();

  bool createCrossCorrelationProgram(GLShaderProgram& program);
  bool createDifferenceProgram(GLShaderProgram& program);
  bool createEdgeProgram(GLShaderProgram& program);
  bool createImageProgram(GLShaderProgram& program);
  bool createImageRgbaProgram(GLShaderProgram& program);
  bool createXrayProgram(GLShaderProgram& program);
  bool createOverlayProgram(GLShaderProgram& program);
  bool createRaycastIsoSurfaceProgram(GLShaderProgram& program);
  bool createSimpleProgram(GLShaderProgram& program);

  void renderImageData();
  void renderOverlays();
  void renderVectorOverlays();

  void renderOneImage(
    const View& view,
    const FrameBounds& miewportViewBounds,
    const glm::vec3& worldOffsetXhairs,
    GLShaderProgram& program,
    const CurrentImages& I,
    bool showEdges
  );

  void volumeRenderOneImage(const View& view, GLShaderProgram& program, const CurrentImages& I);

  void renderAllImages(
    const View& view, const FrameBounds& miewportViewBounds, const glm::vec3& worldOffsetXhairs
  );

  void renderAllLandmarks(
    const View& view, const FrameBounds& miewportViewBounds, const glm::vec3& worldOffsetXhairs
  );

  void renderAllAnnotations(
    const View& view, const FrameBounds& miewportViewBounds, const glm::vec3& worldOffsetXhairs
  );

  // Bind/unbind images, segmentations, and color maps
  std::list<std::reference_wrapper<GLTexture> > bindImageTextures(const ImgSegPair& P);
  void unbindTextures(const std::list<std::reference_wrapper<GLTexture> >& textures);

  // Bind/unbind images, segmentations, and color maps
  std::list<std::reference_wrapper<GLTexture> > bindMetricImageTextures(
    const CurrentImages& I, const camera::ViewRenderMode& metricType
  );

  // Bind/unbind label tables
  std::list<std::reference_wrapper<GLBufferTexture> > bindBufferTextures(const CurrentImages& I);
  void unbindBufferTextures(const std::list<std::reference_wrapper<GLBufferTexture> >& textures);

  // Get current image and segmentation UIDs to render in the metric shaders
  CurrentImages getImageAndSegUidsForMetricShaders(const std::list<uuids::uuid>& metricImageUids
  ) const;

  // Get current image and segmentation UIDs to render in the image shaders
  CurrentImages getImageAndSegUidsForImageShaders(const std::list<uuids::uuid>& imageUids) const;

  AppData& m_appData;

  // NanoVG context for vector graphics (owned by this class)
  NVGcontext* m_nvg;

  GLShaderProgram m_crossCorrelationProgram;
  GLShaderProgram m_differenceProgram;
  GLShaderProgram m_edgeProgram;
  GLShaderProgram m_imageProgram;
  GLShaderProgram m_imageRgbaProgram;
  GLShaderProgram m_xrayProgram;
  GLShaderProgram m_overlayProgram;
  GLShaderProgram m_raycastIsoSurfaceProgram;
  GLShaderProgram m_simpleProgram;

  // Samplers for metric shaders:
  static const Uniforms::SamplerIndexVectorType msk_imgTexSamplers;        // pair of images
  static const Uniforms::SamplerIndexVectorType msk_segTexSamplers;        // pair of segmentations
  static const Uniforms::SamplerIndexVectorType msk_labelTableTexSamplers; // pair of label tables
  static const Uniforms::SamplerIndexVectorType msk_imgCmapTexSamplers; // pair of image colormaps
  static const Uniforms::SamplerIndexType msk_metricCmapTexSampler;     // one colormap

  // Samplers for image shaders:
  static const Uniforms::SamplerIndexType msk_imgTexSampler;            // one image
  static const Uniforms::SamplerIndexVectorType msk_imgRgbaTexSamplers; // Four (RGBA) images
  static const Uniforms::SamplerIndexType msk_segTexSampler;            // one segmentation
  static const Uniforms::SamplerIndexType msk_imgCmapTexSampler;        // one image colormap
  static const Uniforms::SamplerIndexType msk_labelTableTexSampler;     // one label table

  // Samplers for volume rendering shader:
  static const Uniforms::SamplerIndexType msk_jumpTexSampler; // distance map texture

  /// Is the application done loading images?
  bool m_isAppDoneLoadingImages;

  bool m_showOverlays;

  void updateIsosurfaceDataFor2d(AppData& appData, const uuids::uuid& imageUid);
  void updateIsosurfaceDataFor3d(AppData& appData, const uuids::uuid& imageUid);

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
};

#endif // RENDERING_H
