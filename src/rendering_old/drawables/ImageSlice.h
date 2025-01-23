#ifndef IMAGE_SLICE_H
#define IMAGE_SLICE_H

#include "rendering/common/ShaderProviderType.h"
#include "rendering/drawables/DrawableBase.h"
#include "rendering/utility/math/SliceIntersector.h"
#include "rendering_old/common/MeshColorLayer.h"
#include "rendering_old/interfaces/ITexturable3D.h"

#include "common/ObjectCounter.hpp"

#include <memory>
#include <optional>

class BlankTextures;
class Line;
class MeshGpuRecord;
class TexturedMesh;

/// @todo Variable thickness over which to average image values
/// @todo Checkerboarding
/// @todo Cubic sampling

/// @note Number of textures that can be accessed by the fragment shader:
/// glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texture_units);
/// OpenGL 3.x defines the minimum number for the per-stage limit to be 16,
/// so hardware cannot have fewer than 16 textures-per-stage

class ImageSlice : public DrawableBase, public ITexturable3d, public ObjectCounter<ImageSlice>
{
public:
  ImageSlice(
    std::string name,
    ShaderProgramActivatorType shaderProgramActivator,
    UniformsProviderType uniformsProvider,
    std::weak_ptr<BlankTextures> blankTextures,
    std::weak_ptr<MeshGpuRecord> sliceMeshGpuRecord
  );

  ImageSlice(const ImageSlice&) = delete;
  ImageSlice& operator=(const ImageSlice&) = delete;

  ~ImageSlice() override = default;

  bool isOpaque() const override;

  DrawableOpacity opacityFlag() const override;

  void setImage3dRecord(std::weak_ptr<ImageRecord>) override;
  void setParcellationRecord(std::weak_ptr<ParcellationRecord>) override;
  void setImageColorMapRecord(std::weak_ptr<ImageColorMapRecord>) override;
  void setLabelTableRecord(std::weak_ptr<LabelTableRecord>) override;

  void setPositioningMethod(
    const intersection::PositioningMethod& method, const std::optional<glm::vec3>& p = std::nullopt
  );

  void setAlignmentMethod(
    const intersection::AlignmentMethod& method,
    const std::optional<glm::vec3>& worldNormal = std::nullopt
  );

  void setShowOutline(bool show);

  void setShowParcellation(bool show);

  void setUseAutoHiding(bool use);

  void setUseIntensityThresolding(bool use);

private:
  void setupChildren();

  void doUpdate(double time, const Viewport&, const camera::Camera&, const CoordinateFrame&) override;

  /// 3D image record being rendered in this slice
  std::weak_ptr<ImageRecord> m_image3dRecord;

  /// 3D parcellation image record being rendered in this slice
  std::weak_ptr<ParcellationRecord> m_parcelRecord;

  /// Mesh record of this slice
  std::weak_ptr<MeshGpuRecord> m_sliceMeshGpuRecord;

  /// Mesh Drawable for this slice
  std::shared_ptr<TexturedMesh> m_sliceMesh;

  /// Outline Drawable for this slice
  std::shared_ptr<Line> m_sliceOutline;

  /// Object for intersecting the view plane with the mesh
  SliceIntersector m_sliceIntersector;

  /// Flag that intersection vertices exist between the image the view plane
  //    bool m_intersectionsExist;
  glm::vec3 m_modelPlaneNormal;

  glm::mat4 m_clip_O_camera;
  glm::mat4 m_camera_O_world;

  bool m_cameraIsOrthographic;

  glm::vec3 m_worldCameraPos;
  glm::vec3 m_worldCameraDir;

  bool m_showOutline;
};

#endif // IMAGE_SLICE_H
