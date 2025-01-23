#include "rendering_old/assemblies/ImageSliceAssembly.h"
#include "rendering/utility/CreateGLObjects.h"
#include "rendering_old/ShaderNames.h"
#include "rendering_old/common/MeshColorLayer.h"
#include "rendering_old/drawables/ImageSlice.h"
#include "rendering_old/drawables/Transformation.h"
#include "rendering_old/records/MeshGpuRecord.h"

#include "common/DirectionMaps.h"
#include "common/Exception.hpp"

#include <sstream>

namespace
{

// 2D slices are pickable; 3D slices are not:
static constexpr bool sk_pickable2d(true);
static constexpr bool sk_pickable3d(false);

// Thresholding applies to 2D and 3D image slices:
static constexpr bool sk_intensityThresholding2d(true);
static constexpr bool sk_intensityThresholding3d(true);

/**
 * @brief Construct and return a blank ImageSlice drawable that renders as an intersection of the
 * 3D image with the view plane.
 *
 * @param[in] name Name of the drawable
 * @param[in] shaderProgramActivator
 * @param[in] uniformsProvider
 * @param[in] blankTextures Blank textures container
 * @param[in] meshGpuRecord Mesh record of the image slice
 * @param[in] masterOpacityMultiplier
 * @param[in] showOutline
 */
std::shared_ptr<ImageSlice> createPlanarSliceDrawable(
  const std::string& name,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  std::weak_ptr<BlankTextures> blankTextures,
  std::shared_ptr<MeshGpuRecord> meshGpuRecord,
  float masterOpacityMultiplier,
  bool showOutline
)
{
  auto slice = std::make_shared<
    ImageSlice>(name, shaderProgramActivator, uniformsProvider, blankTextures, meshGpuRecord);

  slice->setPositioningMethod(intersection::PositioningMethod::FrameOrigin);
  slice->setAlignmentMethod(intersection::AlignmentMethod::CameraZ);
  slice->setShowOutline(showOutline);
  slice->setUseIntensityThresolding(sk_intensityThresholding2d);
  slice->setPickable(sk_pickable2d);
  slice->setUseAutoHiding(false); // do not auto-hide 2D slices
  slice->setMasterOpacityMultiplier(masterOpacityMultiplier);

  return slice;
}

/**
 * @brief Construct and return a trio of blank ImageSlice drawables that render
 * perpendicular to each other. The slices are aligned to the X, Y, and Z normal
 * vectors of the crosshairs frame of reference
 *
 * @param[in] baseName Base name of the drawables
 * @param[in] shaderProgramActivator
 * @param[in] uniformsProvider
 * @param[in] blankTextures Blank textures container
 * @param[in] meshGpuRecords Mesh records of the image slices
 * @param[in] masterOpacityMultiplier
 * @param[in] useAutoHidingMode
 * @param[in] showOutline
 */
std::array<std::shared_ptr<ImageSlice>, 3> createTriaxialSliceDrawables(
  const std::string& baseName,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  std::weak_ptr<BlankTextures> blankTextures,
  const std::array<std::shared_ptr<MeshGpuRecord>, 3>& meshGpuRecords,
  float masterOpacityMultiplier,
  bool useAutoHidingMode,
  bool showOutline
)
{
  std::array<std::shared_ptr<ImageSlice>, 3> slices;

  for (uint32_t i = 0; i < 3; ++i)
  {
    std::ostringstream name;
    name << baseName << i << std::ends;

    slices[i] = std::make_shared<ImageSlice>(
      name.str(), shaderProgramActivator, uniformsProvider, blankTextures, meshGpuRecords[i]
    );

    slices[i]->setPositioningMethod(intersection::PositioningMethod::FrameOrigin);
    slices[i]->setShowOutline(showOutline);
    slices[i]->setUseIntensityThresolding(sk_intensityThresholding3d);
    slices[i]->setPickable(sk_pickable3d);
    slices[i]->setUseAutoHiding(useAutoHidingMode);
    slices[i]->setMasterOpacityMultiplier(masterOpacityMultiplier);
  }

  // By default, align the slices to the X, Y, and Z planes
  slices[0]->setAlignmentMethod(intersection::AlignmentMethod::FrameX);
  slices[1]->setAlignmentMethod(intersection::AlignmentMethod::FrameY);
  slices[2]->setAlignmentMethod(intersection::AlignmentMethod::FrameZ);

  return slices;
}

} // namespace

ImageSliceAssembly::ImageSliceAssembly(
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  std::weak_ptr<BlankTextures> blankTextures
)
  : m_shaderActivator(shaderProgramActivator)
  , m_uniformsProvider(uniformsProvider)
  , m_blankTextures(blankTextures)
  ,

  m_root2d(nullptr)
  , m_meshGpuRecord2d(nullptr)
  , m_planarSlice(nullptr)
  ,

  m_root3d(nullptr)
  , m_meshGpuRecords3d({nullptr, nullptr, nullptr})
  , m_triaxialSlices({nullptr, nullptr, nullptr})
  ,

  m_properties()
{
}

void ImageSliceAssembly::initialize()
{
  m_meshGpuRecord2d = gpuhelper::createSliceMeshGpuRecord();

  m_meshGpuRecords3d[0] = gpuhelper::createSliceMeshGpuRecord();
  m_meshGpuRecords3d[1] = gpuhelper::createSliceMeshGpuRecord();
  m_meshGpuRecords3d[2] = gpuhelper::createSliceMeshGpuRecord();

  if (!m_meshGpuRecord2d || !m_meshGpuRecords3d[0] || !m_meshGpuRecords3d[1] || !m_meshGpuRecords3d[2])
  {
    throw_debug("Null slice MeshGPURecord: Cannot initialize ImageSliceAssembly");
  }

  std::ostringstream name;
  name << "ImageSliceAssembly_#" << numCreated() << std::ends;

  m_root2d = std::make_shared<Transformation>(name.str() + "_root2d", glm::mat4{1.0f});

  m_planarSlice = createPlanarSliceDrawable(
    name.str() + "_planarSlice",
    m_shaderActivator,
    m_uniformsProvider,
    m_blankTextures,
    m_meshGpuRecord2d,
    m_properties.m_masterOpacityMultiplier,
    m_properties.m_showOutline
  );

  m_root2d->addChild(m_planarSlice);

  m_root3d = std::make_shared<Transformation>(name.str() + "_root3d", glm::mat4{1.0f});

  m_triaxialSlices = createTriaxialSliceDrawables(
    name.str() + "_triaxialSlice_",
    m_shaderActivator,
    m_uniformsProvider,
    m_blankTextures,
    m_meshGpuRecords3d,
    m_properties.m_masterOpacityMultiplier,
    m_properties.m_useAutoHidingMode,
    m_properties.m_showOutline
  );

  for (auto& slice : m_triaxialSlices)
  {
    m_root3d->addChild(slice);
  }
}

std::weak_ptr<DrawableBase> ImageSliceAssembly::getRoot(const SceneType& type)
{
  switch (type)
  {
  case SceneType::ReferenceImage2d:
  case SceneType::SlideStack2d:
  case SceneType::Registration_Image2d:
  case SceneType::Registration_Slide2d:
  {
    return std::static_pointer_cast<DrawableBase>(m_root2d);
  }
  case SceneType::ReferenceImage3d:
  case SceneType::SlideStack3d:
  {
    return std::static_pointer_cast<DrawableBase>(m_root3d);
  }
  default:
  case SceneType::None:
  {
    return {};
  }
  }

  return {};
}

void ImageSliceAssembly::setImage3dRecord(std::weak_ptr<ImageRecord> record)
{
  if (m_planarSlice)
  {
    m_planarSlice->setImage3dRecord(record);
  }

  for (auto& slice : m_triaxialSlices)
  {
    if (slice)
    {
      slice->setImage3dRecord(record);
    }
  }
}

void ImageSliceAssembly::setParcellationRecord(std::weak_ptr<ParcellationRecord> record)
{
  if (m_planarSlice)
  {
    m_planarSlice->setParcellationRecord(record);
  }

  for (auto slice : m_triaxialSlices)
  {
    if (slice)
    {
      slice->setParcellationRecord(record);
    }
  }
}

void ImageSliceAssembly::setImageColorMapRecord(std::weak_ptr<ImageColorMapRecord> record)
{
  if (m_planarSlice)
  {
    m_planarSlice->setImageColorMapRecord(record);
  }

  for (auto slice : m_triaxialSlices)
  {
    if (slice)
    {
      slice->setImageColorMapRecord(record);
    }
  }
}

void ImageSliceAssembly::setLabelTableRecord(std::weak_ptr<LabelTableRecord> record)
{
  if (m_planarSlice)
  {
    m_planarSlice->setLabelTableRecord(record);
  }

  for (auto slice : m_triaxialSlices)
  {
    if (slice)
    {
      slice->setLabelTableRecord(record);
    }
  }
}

void ImageSliceAssembly::setShowOutline(bool show)
{
  if (show == m_properties.m_showOutline)
  {
    return;
  }

  m_properties.m_showOutline = show;

  if (m_planarSlice)
  {
    m_planarSlice->setShowOutline(m_properties.m_showOutline);
  }

  for (auto slice : m_triaxialSlices)
  {
    if (slice)
    {
      slice->setShowOutline(m_properties.m_showOutline);
    }
  }
}

void ImageSliceAssembly::showShowParcellationIn2dViews(bool show)
{
  if (show == m_properties.m_showParcellationIn2dViews)
  {
    return;
  }

  m_properties.m_showParcellationIn2dViews = show;

  if (m_planarSlice)
  {
    m_planarSlice->setShowParcellation(m_properties.m_showParcellationIn2dViews);
  }
}

void ImageSliceAssembly::showShowParcellationIn3dViews(bool show)
{
  if (show == m_properties.m_showParcellationIn3dViews)
  {
    return;
  }

  m_properties.m_showParcellationIn3dViews = show;

  for (auto slice : m_triaxialSlices)
  {
    if (slice)
    {
      slice->setShowParcellation(m_properties.m_showParcellationIn3dViews);
    }
  }
}

void ImageSliceAssembly::setUseAutoHidingMode(bool set)
{
  if (set == m_properties.m_useAutoHidingMode)
  {
    return;
  }

  m_properties.m_useAutoHidingMode = set;

  for (auto slice : m_triaxialSlices)
  {
    if (slice)
    {
      slice->setUseAutoHiding(m_properties.m_useAutoHidingMode);
    }
  }
}

void ImageSliceAssembly::setPickable2d(bool pickable)
{
  if (m_planarSlice)
  {
    m_planarSlice->setPickable(pickable);
  }
}

void ImageSliceAssembly::setPickable3d(bool pickable)
{
  for (auto slice : m_triaxialSlices)
  {
    if (slice)
    {
      slice->setPickable(pickable);
    }
  }
}

void ImageSliceAssembly::setVisibleIn2dViews(bool visible)
{
  m_properties.m_visibleIn2dViews = visible;

  if (m_planarSlice)
  {
    m_planarSlice->setEnabled(m_properties.m_visibleIn2dViews);
  }
}

void ImageSliceAssembly::setVisibleIn3dViews(bool visible)
{
  m_properties.m_visibleIn3dViews = visible;

  for (auto slice : m_triaxialSlices)
  {
    if (slice)
    {
      slice->setEnabled(m_properties.m_visibleIn3dViews);
    }
  }
}

void ImageSliceAssembly::setMasterOpacity(float multiplier)
{
  m_properties.m_masterOpacityMultiplier = multiplier;

  if (m_planarSlice)
  {
    m_planarSlice->setMasterOpacityMultiplier(m_properties.m_masterOpacityMultiplier);
  }

  for (auto slice : m_triaxialSlices)
  {
    if (slice)
    {
      slice->setMasterOpacityMultiplier(m_properties.m_masterOpacityMultiplier);
    }
  }
}

const ImageSliceAssemblyRenderingProperties& ImageSliceAssembly::getRenderingProperties() const
{
  return m_properties;
}
