#include "rendering_old/assemblies/SlideStackAssembly.h"

#include "rendering/utility/CreateGLObjects.h"
#include "rendering/utility/UnderlyingEnumType.h"
#include "rendering/utility/vtk/PolyDataGenerator.h"
#include "rendering_old/drawables/DynamicTransformation.h"
#include "rendering_old/drawables/slides/SlideBox.h"
#include "rendering_old/drawables/slides/SlideSlice.h"
#include "rendering_old/drawables/slides/SlideStackArrow.h"
#include "rendering_old/records/MeshGpuRecord.h"

#include "common/Exception.hpp"
#include "logic/camera/MathUtility.h"

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include <sstream>

namespace
{

//static constexpr float sk_arrow2dMasterOpacity( 1.0f );
static constexpr float sk_arrow3dMasterOpacity(1.0f);

// The 2D stack arrow scales its radius with the view zoom factor:
//static constexpr bool sk_arrow2dHasFixedRadius = false;

// The 3D stack arrow has a fixed radius, regardless of view zoom factor:
static constexpr bool sk_arrow3dHasFixedRadius = true;

// Thresholding is used for 2D and 3D slices
static constexpr bool sk_thresholding2d = true;
static constexpr bool sk_thresholding3d = true;

// Buffers are created ones and drawn many times
static const BufferUsagePattern sk_meshBufferUsage = BufferUsagePattern::StaticDraw;

} // namespace

SlideStackAssembly::SlideStackAssembly(
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  std::weak_ptr<BlankTextures> blankTextures,
  GetterType<float> stackHeightProvider,
  GetterType<glm::mat4> slideStackToWorldTxProvider,
  QuerierType<bool, uuids::uuid> activeSlideQuerier
)
  : m_shaderActivator(shaderProgramActivator)
  , m_uniformsProvider(uniformsProvider)
  , m_blankTextures(blankTextures)
  ,

  m_image3dRecord()
  , m_parcelRecord()
  , m_imageColorMapRecord()
  , m_labelTableRecord()
  ,

  m_slideStackHeightProvider(stackHeightProvider)
  , m_slideStackToWorldTxProvider(slideStackToWorldTxProvider)
  , m_activeSlideQuerier(activeSlideQuerier)
  ,

  m_root2dStackToWorldTx(nullptr)
  , m_root3dStackToWorldTx(nullptr)
  ,

  m_arrow2d(nullptr)
  , m_arrow3d(nullptr)
  ,

  m_coneMeshRecord(nullptr)
  , m_cylinderMeshRecord(nullptr)
  , m_sphereMeshRecord(nullptr)
  ,

  m_boxMeshRecord(nullptr)
  ,

  m_slides()
  , m_properties()
{
}

void SlideStackAssembly::initialize()
{
  static const glm::dvec3 sk_center{0.0, 0.5, 0.0};
  static constexpr double sk_radius = 1.0;
  static constexpr double sk_height = 1.0;

  // Convert unique to shared:

  m_coneMeshRecord = gpuhelper::createMeshGpuRecordFromVtkPolyData(
    vtkutils::generateCone(), MeshPrimitiveType::Triangles, sk_meshBufferUsage
  );

  m_cylinderMeshRecord = gpuhelper::createMeshGpuRecordFromVtkPolyData(
    vtkutils::generateCylinder(sk_center, sk_radius, sk_height),
    MeshPrimitiveType::Triangles,
    sk_meshBufferUsage
  );

  m_sphereMeshRecord = gpuhelper::createMeshGpuRecordFromVtkPolyData(
    vtkutils::generateSphere(), MeshPrimitiveType::Triangles, sk_meshBufferUsage
  );

  if (!m_coneMeshRecord || !m_cylinderMeshRecord || !m_sphereMeshRecord)
  {
    throw_debug("Null Slide Stack Arrow MeshGpuRecord");
  }

  m_boxMeshRecord = gpuhelper::createBoxMeshGpuRecord(BufferUsagePattern::StaticDraw);

  if (!m_boxMeshRecord)
  {
    throw_debug("Null MeshGPURecord");
  }

  std::ostringstream baseName;
  baseName << "SlideStackAssembly_#" << numCreated() << std::ends;

  //    m_arrow2d = std::make_shared<SlideStackArrow>(
  //                baseName.str() + "_arrow2d",
  //                m_shaderActivator, m_uniformsProvider, m_slideStackHeightProvider,
  //                m_coneMeshRecord, m_cylinderMeshRecord, m_sphereMeshRecord,
  //                sk_arrow2dHasFixedRadius );

  //    m_arrow2d->setMasterOpacityMultiplier( sk_arrow2dMasterOpacity );

  //    m_root2dStackToWorldTx->addChild( m_arrow2d );

  m_arrow3d = std::make_shared<SlideStackArrow>(
    baseName.str() + "_arrow3d",
    m_shaderActivator,
    m_uniformsProvider,
    m_slideStackHeightProvider,
    m_coneMeshRecord,
    m_cylinderMeshRecord,
    m_sphereMeshRecord,
    sk_arrow3dHasFixedRadius
  );

  m_arrow3d->setMasterOpacityMultiplier(sk_arrow3dMasterOpacity);

  m_root2dStackToWorldTx = std::make_shared<
    DynamicTransformation>(baseName.str() + "_root2d", m_slideStackToWorldTxProvider);

  m_root3dStackToWorldTx = std::make_shared<
    DynamicTransformation>(baseName.str() + "_root3d", m_slideStackToWorldTxProvider);

  m_root3dStackToWorldTx->addChild(m_arrow3d);

  updateStackRenderingProperties();
}

std::weak_ptr<DrawableBase> SlideStackAssembly::getRoot(const SceneType& type)
{
  switch (type)
  {
  case SceneType::ReferenceImage2d:
  case SceneType::SlideStack2d:
  case SceneType::Registration_Image2d:
  case SceneType::Registration_Slide2d:
  {
    return std::static_pointer_cast<DrawableBase>(m_root2dStackToWorldTx);
  }
  case SceneType::ReferenceImage3d:
  case SceneType::SlideStack3d:
  {
    return std::static_pointer_cast<DrawableBase>(m_root3dStackToWorldTx);
  }
  default:
  case SceneType::None:
  {
    return {};
  }
  }

  return {};
}

void SlideStackAssembly::addSlide(const uuids::uuid& uid, std::weak_ptr<SlideRecord> slideRecord)
{
  const auto itr = m_slides.find(uid);
  if (std::end(m_slides) != itr)
  {
    // Remove slide if it is already in collection
    removeSlide(uid);
  }

  // No slide with this UID exists, so create one

  std::ostringstream sliceName;
  std::ostringstream boxName;

  sliceName << "SlideSlice@" << uid << std::ends;
  boxName << "SlideBox@" << uid << std::ends;

  std::shared_ptr<MeshGpuRecord> sliceMeshGpuRecord = gpuhelper::createSliceMeshGpuRecord(
    BufferUsagePattern::DynamicDraw
  );

  if (!sliceMeshGpuRecord)
  {
    throw_debug("Null MeshGPURecord");
  }

  auto getImage3dLayerOpacity = [this](void)
  { return getRenderingProperties().m_image3dLayerOpacity; };

  auto slideSlice = std::make_shared<SlideSlice>(
    sliceName.str(),
    m_shaderActivator,
    m_uniformsProvider,
    m_blankTextures,
    sliceMeshGpuRecord,
    slideRecord,
    m_activeSlideQuerier,
    getImage3dLayerOpacity
  );

  auto slideBox = std::make_shared<SlideBox>(
    boxName.str(),
    m_shaderActivator,
    m_uniformsProvider,
    m_blankTextures,
    m_boxMeshRecord,
    slideRecord,
    m_activeSlideQuerier,
    getImage3dLayerOpacity
  );

  slideSlice->setImage3dRecord(m_image3dRecord);
  slideSlice->setParcellationRecord(m_parcelRecord);
  slideSlice->setImageColorMapRecord(m_imageColorMapRecord);
  slideSlice->setLabelTableRecord(m_labelTableRecord);

  slideBox->setImage3dRecord(m_image3dRecord);
  slideBox->setParcellationRecord(m_parcelRecord);
  slideBox->setImageColorMapRecord(m_imageColorMapRecord);
  slideBox->setLabelTableRecord(m_labelTableRecord);

  m_slides.emplace(uid, SlideSliceAndBox{sliceMeshGpuRecord, slideSlice, slideBox});

  // Add slide to Drawables trees
  if (m_root2dStackToWorldTx)
  {
    m_root2dStackToWorldTx->addChild(slideSlice);
  }

  if (m_root3dStackToWorldTx)
  {
    m_root3dStackToWorldTx->addChild(slideBox);
  }

  updateStackRenderingProperties();
}

void SlideStackAssembly::removeSlide(const uuids::uuid& uid)
{
  const auto itr = m_slides.find(uid);
  if (std::end(m_slides) == itr)
  {
    /// @todo Log
    std::cerr << "Error: slide with UID " << uid << " not found in collection." << std::endl;
    return;
  }

  // Remove slide from roots
  if (auto s = itr->second.m_slideSlice)
  {
    if (m_root2dStackToWorldTx)
    {
      m_root2dStackToWorldTx->removeChild(*s);
    }
  }

  if (auto s = itr->second.m_slideBox)
  {
    if (m_root3dStackToWorldTx)
    {
      m_root3dStackToWorldTx->removeChild(*s);
    }
  }

  m_slides.erase(uid);

  updateStackRenderingProperties();
}

void SlideStackAssembly::clearSlides()
{
  for (auto& slide : m_slides)
  {
    if (auto s = slide.second.m_slideSlice)
    {
      m_root2dStackToWorldTx->removeChild(*s);
    }

    if (auto s = slide.second.m_slideBox)
    {
      m_root3dStackToWorldTx->removeChild(*s);
    }
  }

  m_slides.clear();

  updateStackRenderingProperties();
}

void SlideStackAssembly::setMasterOpacityMultiplier(float multiplier)
{
  m_properties.m_masterOpacityMultiplier = multiplier;
  updateStackRenderingProperties();
}

void SlideStackAssembly::setImage3dLayerOpacityMultiplier(float multiplier)
{
  m_properties.m_image3dLayerOpacity = multiplier;
  updateStackRenderingProperties();
}

void SlideStackAssembly::setPickable(bool pickable)
{
  m_properties.m_pickable = pickable;
  updateStackRenderingProperties();
}

void SlideStackAssembly::setVisibleIn2dViews(bool visible)
{
  m_properties.m_visibleIn2dViews = visible;
  updateStackRenderingProperties();
}

void SlideStackAssembly::setVisibleIn3dViews(bool visible)
{
  m_properties.m_visibleIn3dViews = visible;
  updateStackRenderingProperties();
}

void SlideStackAssembly::setActiveSlideViewShows2dSlides(bool show2d)
{
  m_properties.m_activeSlideViewShows2dSlides = show2d;
  updateStackRenderingProperties();
}

void SlideStackAssembly::setImage3dRecord(std::weak_ptr<ImageRecord> record)
{
  m_image3dRecord = record;

  for (auto& s : m_slides)
  {
    if (s.second.m_slideSlice)
    {
      s.second.m_slideSlice->setImage3dRecord(record);
    }

    if (s.second.m_slideBox)
    {
      s.second.m_slideBox->setImage3dRecord(record);
    }
  }
}

void SlideStackAssembly::setParcellationRecord(std::weak_ptr<ParcellationRecord> record)
{
  m_parcelRecord = record;

  for (auto& s : m_slides)
  {
    if (s.second.m_slideSlice)
    {
      s.second.m_slideSlice->setParcellationRecord(record);
    }

    if (s.second.m_slideBox)
    {
      s.second.m_slideBox->setParcellationRecord(record);
    }
  }
}

void SlideStackAssembly::setImageColorMapRecord(std::weak_ptr<ImageColorMapRecord> record)
{
  m_imageColorMapRecord = record;

  for (auto& s : m_slides)
  {
    if (s.second.m_slideSlice)
    {
      s.second.m_slideSlice->setImageColorMapRecord(record);
    }

    if (s.second.m_slideBox)
    {
      s.second.m_slideBox->setImageColorMapRecord(record);
    }
  }
}

void SlideStackAssembly::setLabelTableRecord(std::weak_ptr<LabelTableRecord> record)
{
  m_labelTableRecord = record;

  for (auto& s : m_slides)
  {
    if (s.second.m_slideSlice)
    {
      s.second.m_slideSlice->setLabelTableRecord(record);
    }

    if (s.second.m_slideBox)
    {
      s.second.m_slideBox->setLabelTableRecord(record);
    }
  }
}

void SlideStackAssembly::setArrowRadius(float radius)
{
  if (m_arrow2d)
  {
    m_arrow2d->setRadius(radius);
  }

  if (m_arrow3d)
  {
    m_arrow3d->setRadius(radius);
  }
}

void SlideStackAssembly::setSlideStackHeightProvider(GetterType<float> provider)
{
  m_slideStackHeightProvider = provider;

  if (m_arrow2d)
  {
    m_arrow2d->setSlideStackHeightProvider(provider);
  }

  if (m_arrow3d)
  {
    m_arrow3d->setSlideStackHeightProvider(provider);
  }
}

void SlideStackAssembly::setSlideStackToWorldTxProvider(GetterType<glm::mat4> provider)
{
  m_slideStackToWorldTxProvider = provider;

  if (m_root2dStackToWorldTx)
  {
    m_root2dStackToWorldTx->setMatrixProvider(m_slideStackToWorldTxProvider);
  }

  if (m_root3dStackToWorldTx)
  {
    m_root3dStackToWorldTx->setMatrixProvider(m_slideStackToWorldTxProvider);
  }
}

void SlideStackAssembly::setActiveSlideQuerier(QuerierType<bool, uuids::uuid> querier)
{
  m_activeSlideQuerier = querier;
}

const SlideStackAssemblyRenderingProperties& SlideStackAssembly::getRenderingProperties() const
{
  return m_properties;
}

void SlideStackAssembly::updateStackRenderingProperties()
{
  for (auto& s : m_slides)
  {
    if (auto slide = s.second.m_slideSlice)
    {
      slide->setMasterOpacityMultiplier(m_properties.m_masterOpacityMultiplier);
      slide->setPickable(m_properties.m_pickable);
      slide->setUseIntensityThresolding(sk_thresholding2d);
    }

    if (auto slide = s.second.m_slideBox)
    {
      slide->setMasterOpacityMultiplier(m_properties.m_masterOpacityMultiplier);
      slide->setPickable(m_properties.m_pickable);
      slide->setUseIntensityThresolding(sk_thresholding3d);
    }
  }

  if (m_root2dStackToWorldTx)
  {
    m_root2dStackToWorldTx->setEnabled(m_properties.m_visibleIn2dViews);
  }

  if (m_root3dStackToWorldTx)
  {
    m_root3dStackToWorldTx->setEnabled(m_properties.m_visibleIn3dViews);
  }
}
