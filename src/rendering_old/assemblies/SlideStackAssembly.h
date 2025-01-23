#ifndef SLIDE_STACK_ASEEMBLY_H
#define SLIDE_STACK_ASEEMBLY_H

#include "rendering/common/ShaderProviderType.h"
#include "rendering_old/assemblies/RenderingProperties.h"
#include "rendering_old/common/MeshColorLayer.h"
#include "rendering_old/interfaces/IDrawableAssembly.h"
#include "rendering_old/interfaces/ITexturable3D.h"

#include "common/ObjectCounter.hpp"
#include "common/PublicTypes.h"

#include "logic_old/records/SlideRecord.h"

#include <uuid.h>

#include <unordered_map>

class BlankTextures;
class DynamicTransformation;
class MeshGpuRecord;
class SlideBox;
class SlideSlice;
class SlideStackArrow;

class SlideStackAssembly final : public IDrawableAssembly,
                                 public ITexturable3d,
                                 public ObjectCounter<SlideStackAssembly>
{
public:
  explicit SlideStackAssembly(
    ShaderProgramActivatorType,
    UniformsProviderType,
    std::weak_ptr<BlankTextures> blankTextures,
    GetterType<float> stackHeightProvider,
    GetterType<glm::mat4> slideStackToWorldTxProvider,
    QuerierType<bool, uuids::uuid> activeSlideQuerier
  );

  ~SlideStackAssembly() override = default;

  void initialize() override;

  std::weak_ptr<DrawableBase> getRoot(const SceneType&) override;

  void setImage3dRecord(std::weak_ptr<ImageRecord>) override;
  void setParcellationRecord(std::weak_ptr<ParcellationRecord>) override;
  void setImageColorMapRecord(std::weak_ptr<ImageColorMapRecord>) override;
  void setLabelTableRecord(std::weak_ptr<LabelTableRecord>) override;

  void addSlide(const uuids::uuid& uid, std::weak_ptr<SlideRecord>);
  void removeSlide(const uuids::uuid& uid);
  void clearSlides();

  void setMasterOpacityMultiplier(float multiplier);
  void setImage3dLayerOpacityMultiplier(float multiplier);

  void setPickable(bool pickable);

  void setVisibleIn2dViews(bool visible);
  void setVisibleIn3dViews(bool visible);

  void setActiveSlideViewShows2dSlides(bool show2d);

  void setArrowRadius(float radius);

  void setSlideStackHeightProvider(GetterType<float>);
  void setSlideStackToWorldTxProvider(GetterType<glm::mat4>);
  void setActiveSlideQuerier(QuerierType<bool, uuids::uuid>);

  const SlideStackAssemblyRenderingProperties& getRenderingProperties() const;

private:
  /// Data for rendering a single slide in 2D and 3D views.
  struct SlideSliceAndBox
  {
    /// Mesh record ofthe polygon used for 2D slices of slides.
    /// Each SlideSlice is different and needs its own MeshRecord.
    std::shared_ptr<MeshGpuRecord> m_sliceMeshGpuRecord = nullptr;

    /// Slide rendered in 2D views is a slice.
    std::shared_ptr<SlideSlice> m_slideSlice = nullptr;

    /// Slide rendered in 3D views is a box.
    std::shared_ptr<SlideBox> m_slideBox = nullptr;
  };

  void updateStackRenderingProperties();

  ShaderProgramActivatorType m_shaderActivator;
  UniformsProviderType m_uniformsProvider;
  std::weak_ptr<BlankTextures> m_blankTextures;

  std::weak_ptr<ImageRecord> m_image3dRecord;
  std::weak_ptr<ParcellationRecord> m_parcelRecord;
  std::weak_ptr<ImageColorMapRecord> m_imageColorMapRecord;
  std::weak_ptr<LabelTableRecord> m_labelTableRecord;

  /// Function that provides the height of the Slide Stack in World space
  GetterType<float> m_slideStackHeightProvider;

  /// Function that provides the matrix transformation from Slide Stack to World space
  GetterType<glm::mat4> m_slideStackToWorldTxProvider;

  /// Function that returns true iff the provided UID is for the active slide
  QuerierType<bool, uuids::uuid> m_activeSlideQuerier;

  /// Root drawables for the 2D version of the slide stack
  std::shared_ptr<DynamicTransformation> m_root2dStackToWorldTx;

  /// Root drawables for the 3D version of the slide stack
  std::shared_ptr<DynamicTransformation> m_root3dStackToWorldTx;

  /// Slide stack arrow for 2D views
  std::shared_ptr<SlideStackArrow> m_arrow2d;

  /// Slide stack arrow for 3D views
  std::shared_ptr<SlideStackArrow> m_arrow3d;

  /// Mesh GPU records for the cone, cylinder, and sphere that make up the slide stack arrow
  std::shared_ptr<MeshGpuRecord> m_coneMeshRecord;
  std::shared_ptr<MeshGpuRecord> m_cylinderMeshRecord;
  std::shared_ptr<MeshGpuRecord> m_sphereMeshRecord;

  /// Mesh record of the box used for 3D slides
  std::shared_ptr<MeshGpuRecord> m_boxMeshRecord;

  std::unordered_map<uuids::uuid, SlideSliceAndBox> m_slides;

  SlideStackAssemblyRenderingProperties m_properties;
};

#endif // SLIDE_STACK_ASEEMBLY_H
