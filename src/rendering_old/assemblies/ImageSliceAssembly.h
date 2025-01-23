#ifndef IMAGE_SLICE_ASSEMBLY_H
#define IMAGE_SLICE_ASSEMBLY_H

#include "rendering/common/ShaderProviderType.h"
#include "rendering_old/assemblies/RenderingProperties.h"
#include "rendering_old/interfaces/IDrawableAssembly.h"
#include "rendering_old/interfaces/ITexturable3D.h"

#include "common/ObjectCounter.hpp"

#include <array>
#include <memory>

class BlankTextures;
class ImageSlice;
class MeshGpuRecord;
class Transformation;

/**
 * @brief This class creates assemblies of 3D image slices. There are two kinds of assemblies:
 * one for 2D scenes and one for 3D scenes. The assembly for 2D scenes consists of a single
 * ImageSlice at the intersction of the 3D image with the view plane. (The view plane is defined
 * by the view normal and the crosshairs position.) The assembly for 3D scenes consists of a trio of
 * ImageSlices that are mutually perpendicular and aligned with the axes of the crosshairs
 * frame of reference.
 *
 * This class is the owner of the ImageSlice drawables and their associated mesh records.
 */
class ImageSliceAssembly final : public IDrawableAssembly,
                                 public ITexturable3d,
                                 public ObjectCounter<ImageSliceAssembly>
{
public:
  explicit ImageSliceAssembly(ShaderProgramActivatorType, UniformsProviderType, std::weak_ptr<BlankTextures>);

  ~ImageSliceAssembly() override = default;

  void initialize() override;

  std::weak_ptr<DrawableBase> getRoot(const SceneType&) override;

  void setImage3dRecord(std::weak_ptr<ImageRecord>) override;
  void setParcellationRecord(std::weak_ptr<ParcellationRecord>) override;
  void setImageColorMapRecord(std::weak_ptr<ImageColorMapRecord>) override;
  void setLabelTableRecord(std::weak_ptr<LabelTableRecord>) override;

  void setVisibleIn2dViews(bool visible);
  void setVisibleIn3dViews(bool visible);

  void setMasterOpacity(float opacity);

  void setUseAutoHidingMode(bool use);

  void setShowOutline(bool show);

  void showShowParcellationIn2dViews(bool show);
  void showShowParcellationIn3dViews(bool show);

  void setPickable2d(bool set);
  void setPickable3d(bool set);

  const ImageSliceAssemblyRenderingProperties& getRenderingProperties() const;

private:
  ShaderProgramActivatorType m_shaderActivator;
  UniformsProviderType m_uniformsProvider;
  std::weak_ptr<BlankTextures> m_blankTextures;

  // Objects for 2D scenes, which contain one slice intersecting (parallel to) the view plane
  std::shared_ptr<Transformation> m_root2d;
  std::shared_ptr<MeshGpuRecord> m_meshGpuRecord2d;
  std::shared_ptr<ImageSlice> m_planarSlice;

  // Objects for 3D scenes, which contain three slices with normal vectors independent of the
  // view plane normal
  std::shared_ptr<Transformation> m_root3d;
  std::array<std::shared_ptr<MeshGpuRecord>, 3> m_meshGpuRecords3d;
  std::array<std::shared_ptr<ImageSlice>, 3> m_triaxialSlices;

  ImageSliceAssemblyRenderingProperties m_properties;
};

#endif // IMAGE_SLICE_ASSEMBLY_H
