#include "logic/managers/AssemblyManager.h"
#include "logic/managers/DataManager.h"

#include "common/HZeeException.hpp"

#include "rendering/ShaderNames.h"
#include "rendering/assemblies/AnnotationAssembly.h"
#include "rendering/assemblies/CameraLabelAssembly.h"
#include "rendering/assemblies/CrosshairsAssembly.h"
#include "rendering/assemblies/ImageSliceAssembly.h"
#include "rendering/assemblies/LandmarkAssembly.h"
#include "rendering/assemblies/MeshAssembly.h"
#include "rendering/assemblies/SlideStackAssembly.h"
#include "rendering/common/MeshColorLayer.h"
#include "rendering/drawables/DrawableBase.h"
#include "rendering/drawables/Transformation.h"
#include "rendering/records/MeshGpuRecord.h"
#include "rendering/utility/CreateGLObjects.h"

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>

#include <boost/signals2.hpp>

#include <sstream>

namespace
{

static const glm::dvec3 sk_slideLmCylinderCenter{0.0, 0.0, 0.0};
static constexpr double sk_slideLmCylinderRadius = 1.0;
static constexpr double sk_slideLmCylinderHeight = 1.0;

} // namespace

const std::unordered_map<gui::ViewType, SceneType> AssemblyManager::smk_defaultViewTypeToSceneTypeMap
  = {
    {gui::ViewType::Image_Axial, SceneType::ReferenceImage2d},
    {gui::ViewType::Image_Coronal, SceneType::ReferenceImage2d},
    {gui::ViewType::Image_Sagittal, SceneType::ReferenceImage2d},
    {gui::ViewType::Image_3D, SceneType::ReferenceImage3d},

    {gui::ViewType::Image_Big3D, SceneType::ReferenceImage3d},

    {gui::ViewType::Stack_ActiveSlide, SceneType::SlideStack2d},
    {gui::ViewType::Stack_StackSide1, SceneType::SlideStack2d},
    {gui::ViewType::Stack_StackSide2, SceneType::SlideStack2d},
    {gui::ViewType::Stack_3D, SceneType::SlideStack3d},

    {gui::ViewType::Reg_ActiveSlide, SceneType::Registration_Slide2d},
    {gui::ViewType::Reg_RefImageAtSlide, SceneType::Registration_Image2d}
};

struct AssemblyManager::Impl
{
  Impl(
    DataManager& dataManager,
    ShaderProgramActivatorType shaderActivator,
    UniformsProviderType uniformsProvider,
    std::weak_ptr<BlankTextures> blankTextures,
    AllViewsUpdaterType allViewsUpdater,
    GetterType<float> slideStackHeightProvider,
    GetterType<glm::mat4> slideStackToWorldTxProvider,
    QuerierType<std::optional<std::pair<glm::mat4, glm::mat4> >, UID>
      refImageLandmarkGroupToWorldTxQuerier,
    QuerierType<std::optional<std::pair<glm::mat4, glm::mat4> >, UID>
      slideLandmarkGroupToWorldTxQuerier,
    QuerierType<DrawableScaling, UID> refImageLandmarkGroupScalingQuerier,
    QuerierType<DrawableScaling, UID> slideLandmarkGroupScalingQuerier,
    QuerierType<float, UID> slideAnnotationThicknessQuerier,
    QuerierType<bool, UID> activeSlideQuerier,
    GetterType<glm::mat4> activeSubjectToWorldProvider
  );

  void initialize();

  void updateCrosshairDimensions(const UID& imageUid);

  std::shared_ptr<DrawableBase> constructSceneRoot(const SceneType& sceneType);

  std::shared_ptr<DrawableBase> constructOverlayRoot(const SceneType& sceneType);

  void updateAllViews();

  DataManager& m_dataManager;

  CameraLabelAssembly m_cameraLabelAssembly;
  CrosshairsAssembly m_crosshairsAssembly;
  ImageSliceAssembly m_imageSliceAssembly;
  SlideStackAssembly m_slideStackAssembly;

  MeshAssembly m_isoSurfaceMeshAssembly;
  MeshAssembly m_labelMeshAssembly;

  LandmarkAssembly m_slideLandmarkAssembly;
  LandmarkAssembly m_refImageLandmarkAssembly;
  AnnotationAssembly m_slideAnnotationAssembly;

  AllViewsUpdaterType m_allViewsUpdater;

  /// All scene roots
  std::unordered_map<SceneType, std::shared_ptr<DrawableBase> > m_rootDrawables;

  /// All scene overlay roots
  std::unordered_map<SceneType, std::shared_ptr<DrawableBase> > m_overlayDrawables;

  /// Current mapping from ViewType to SceneType
  std::unordered_map<gui::ViewType, SceneType> m_viewTypeToSceneType;

  /// Signal that image slice assembly rendering properties have changed
  boost::signals2::signal<void(const UID& imageUid, const ImageSliceAssemblyRenderingProperties&)>
    m_signalImageSliceAssemblyRenderingPropertiesChanged;

  /// Signal that iso-surface mesh assembly rendering properties have changed
  boost::signals2::signal<void(const MeshAssemblyRenderingProperties&)>
    m_signalIsoMeshAssemblyRenderingPropertiesChanged;

  /// Signal that label mesh assembly rendering properties have changed
  boost::signals2::signal<void(const MeshAssemblyRenderingProperties&)>
    m_signalLabelMeshAssemblyRenderingPropertiesChanged;

  /// Signal that slide assembly rendering properties have changed
  boost::signals2::signal<void(const SlideStackAssemblyRenderingProperties&)>
    m_signalSlideStackAssemblyRenderingPropertiesChanged;

  /// Functional for broadcasting that the transformations of slides have changed.
  /// The argument is a list of UIDs of slides whose stack_O_slide transformation matrix
  /// has changed.
  boost::signals2::signal<void(const std::list<UID>& slideUids)> m_signalSlideTransformationsChanged;
};

AssemblyManager::AssemblyManager(
  DataManager& dataManager,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  std::weak_ptr<BlankTextures> blankTextures
)
  : m_impl(std::make_unique<Impl>(
    dataManager,
    shaderProgramActivator,
    uniformsProvider,
    blankTextures,
    // yuck:
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
  ))
{
}

AssemblyManager::~AssemblyManager() = default;

void AssemblyManager::initializeGL()
{
  m_impl->initialize();
}

void AssemblyManager::setAllViewsUpdater(AllViewsUpdaterType updater)
{
  m_impl->m_allViewsUpdater = updater;
}

void AssemblyManager::setSlideStackHeightProvider(GetterType<float> provider)
{
  m_impl->m_slideStackAssembly.setSlideStackHeightProvider(provider);
}

void AssemblyManager::setRefImageLandmarkGroupToWorldTxQuerier(
  QuerierType<std::optional<std::pair<glm::mat4, glm::mat4> >, UID> querier
)
{
  m_impl->m_refImageLandmarkAssembly.setLandmarkGroupToWorldTxQuerier(querier);
}

void AssemblyManager::setSlideStackToWorldTxProvider(GetterType<glm::mat4> provider)
{
  m_impl->m_slideStackAssembly.setSlideStackToWorldTxProvider(provider);
}

void AssemblyManager::setSlideLandmarkGroupToWorldTxQuerier(
  QuerierType<std::optional<std::pair<glm::mat4, glm::mat4> >, UID> querier
)
{
  m_impl->m_slideLandmarkAssembly.setLandmarkGroupToWorldTxQuerier(querier);
}

void AssemblyManager::setSlideAnnotationToWorldTxQuerier(
  QuerierType<std::optional<std::pair<glm::mat4, glm::mat4> >, UID> querier
)
{
  m_impl->m_slideAnnotationAssembly.setAnnotationToWorldTxQuerier(querier);
}

void AssemblyManager::setRefImageLandmarkGroupScalingQuerier(QuerierType<DrawableScaling, UID> querier
)
{
  m_impl->m_refImageLandmarkAssembly.setLandmarkGroupScalingQuerier(querier);
}

void AssemblyManager::setSlideLandmarkGroupScalingQuerier(QuerierType<DrawableScaling, UID> querier)
{
  m_impl->m_slideLandmarkAssembly.setLandmarkGroupScalingQuerier(querier);
}

void AssemblyManager::setSlideAnnotationThicknessQuerier(
  QuerierType<std::optional<float>, UID> querier
)
{
  m_impl->m_slideAnnotationAssembly.setAnnotationThicknessQuerier(querier);
}

void AssemblyManager::setActiveSlideQuerier(QuerierType<bool, UID> querier)
{
  m_impl->m_slideStackAssembly.setActiveSlideQuerier(querier);
}

void AssemblyManager::setActiveSubjectToWorldProvider(GetterType<std::optional<glm::mat4> > provider)
{
  m_impl->m_cameraLabelAssembly.setActiveSubjectToWorldProvider(provider);
}

void AssemblyManager::setLabelMeshSubjectToWorldTxQuerier(
  QuerierType<std::optional<glm::mat4>, UID> querier
)
{
  m_impl->m_labelMeshAssembly.setMeshSubjectToWorldTxQuerier(querier);
}

void AssemblyManager::setIsoSurfaceMeshSubjectToWorldTxQuerier(
  QuerierType<std::optional<glm::mat4>, UID> querier
)
{
  m_impl->m_isoSurfaceMeshAssembly.setMeshSubjectToWorldTxQuerier(querier);
}

void AssemblyManager::setSceneType(const gui::ViewType& viewType, const SceneType& sceneType)
{
  m_impl->m_viewTypeToSceneType[viewType] = sceneType;
}

SceneType AssemblyManager::getSceneType(const gui::ViewType& viewType) const
{
  auto it = m_impl->m_viewTypeToSceneType.find(viewType);
  if (std::end(m_impl->m_viewTypeToSceneType) != it)
  {
    return it->second;
  }
  return SceneType::None;
}

void AssemblyManager::updateImages(
  const UID& imageUid, const UID& parcelUid, const UID& imageColorMapUid, const UID& labelTableUid
)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  auto imageRecord = m_impl->m_dataManager.imageRecord(imageUid);
  auto parcelRecord = m_impl->m_dataManager.parcellationRecord(parcelUid);

  // Update image slices, meshes, and slides with the new image and parcellation records:
  m_impl->m_imageSliceAssembly.setImage3dRecord(imageRecord);
  m_impl->m_imageSliceAssembly.setParcellationRecord(parcelRecord);

  m_impl->m_labelMeshAssembly.setImage3dRecord(imageRecord);
  m_impl->m_labelMeshAssembly.setParcellationRecord(parcelRecord);

  m_impl->m_isoSurfaceMeshAssembly.setImage3dRecord(imageRecord);
  m_impl->m_isoSurfaceMeshAssembly.setParcellationRecord(parcelRecord);

  m_impl->m_slideStackAssembly.setImage3dRecord(imageRecord);
  m_impl->m_slideStackAssembly.setParcellationRecord(parcelRecord);

  static constexpr bool sk_render = false;
  updateImageColorMap(imageColorMapUid, sk_render);
  updateLabelColorTable(labelTableUid, sk_render);

  updateRefImageLandmarkGroups(imageUid);

  // Now update the arrow and crosshair dimensions according to the
  // image bounding box size and voxel spacing
  m_impl->updateCrosshairDimensions(imageUid);

  m_impl->updateAllViews();

  m_impl->m_signalImageSliceAssemblyRenderingPropertiesChanged(
    imageUid, m_impl->m_imageSliceAssembly.getRenderingProperties()
  );
}

void AssemblyManager::updateIsoSurfaceMeshes(uid_range_t meshUids)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  m_impl->m_isoSurfaceMeshAssembly.clearMeshes();

  for (const auto& uid : meshUids)
  {
    auto record = m_impl->m_dataManager.isoMeshRecord(uid);
    m_impl->m_isoSurfaceMeshAssembly.addMesh(uid, record);
  }

  m_impl->updateAllViews();
}

void AssemblyManager::updateLabelMeshes(uid_range_t meshUids, const UID& labelTableUid)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  m_impl->m_labelMeshAssembly.clearMeshes();

  for (const auto& uid : meshUids)
  {
    auto record = m_impl->m_dataManager.labelMeshRecord(uid);
    m_impl->m_labelMeshAssembly.addMesh(uid, record);
  }

  updateLabelColorTable(labelTableUid, false);

  m_impl->updateAllViews();
}

void AssemblyManager::updateSlideStack(uid_range_t slideUids)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  m_impl->m_slideStackAssembly.clearSlides();

  for (const auto& uid : slideUids)
  {
    auto record = m_impl->m_dataManager.slideRecord(uid);
    m_impl->m_slideStackAssembly.addSlide(uid, record);
  }

  updateSlideLandmarkGroups(slideUids);

  m_impl->updateAllViews();

  m_impl->m_signalSlideStackAssemblyRenderingPropertiesChanged(
    m_impl->m_slideStackAssembly.getRenderingProperties()
  );
}

void AssemblyManager::updatedSlideTransformations(const std::list<UID>& slideUids)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  /// @todo Put in connectionmanager!!
  m_impl->m_signalSlideTransformationsChanged(slideUids); // Signal transformation change

  m_impl->updateAllViews();
}

void AssemblyManager::updateImageColorMap(const UID& colorMapUid, bool updateViews)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  auto weakMapRecord = m_impl->m_dataManager.imageColorMapRecord(colorMapUid);

  m_impl->m_imageSliceAssembly.setImageColorMapRecord(weakMapRecord);
  m_impl->m_isoSurfaceMeshAssembly.setImageColorMapRecord(weakMapRecord);
  m_impl->m_labelMeshAssembly.setImageColorMapRecord(weakMapRecord);
  m_impl->m_slideStackAssembly.setImageColorMapRecord(weakMapRecord);

  if (updateViews)
  {
    m_impl->updateAllViews();
  }
}

void AssemblyManager::updateLabelColorTable(const UID& colorTableUid, bool updateViews)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  auto weakTableRecord = m_impl->m_dataManager.labelTableRecord(colorTableUid);

  m_impl->m_imageSliceAssembly.setLabelTableRecord(weakTableRecord);
  m_impl->m_isoSurfaceMeshAssembly.setLabelTableRecord(weakTableRecord);
  m_impl->m_labelMeshAssembly.setLabelTableRecord(weakTableRecord);
  m_impl->m_slideStackAssembly.setLabelTableRecord(weakTableRecord);

  if (updateViews)
  {
    m_impl->updateAllViews();
  }
}

void AssemblyManager::updateRefImageLandmarkGroups(const UID& imageUid)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  auto lmGroupUids = m_impl->m_dataManager.landmarkGroupUids_of_image(imageUid);

  for (const auto& lmGroupUid : lmGroupUids)
  {
    auto lmGroupRecord = m_impl->m_dataManager.refImageLandmarkGroupRecord(lmGroupUid);
    m_impl->m_refImageLandmarkAssembly.addLandmarkGroup(lmGroupRecord);
  }

  m_impl->updateAllViews();
}

void AssemblyManager::updateSlideLandmarkGroups(uid_range_t slideUids)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  for (const auto& slideUid : slideUids)
  {
    auto lmGroupUids = m_impl->m_dataManager.landmarkGroupUids_of_slide(slideUid);

    for (const auto& lmGroupUid : lmGroupUids)
    {
      auto lmGroupRecord = m_impl->m_dataManager.slideLandmarkGroupRecord(lmGroupUid);
      m_impl->m_slideLandmarkAssembly.addLandmarkGroup(lmGroupRecord);
    }
  }

  m_impl->updateAllViews();
}

void AssemblyManager::updateSlideAnnotations(uid_range_t slideUids)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  // Loop over all slides
  for (const auto& slideUid : slideUids)
  {
    const auto annotUids = m_impl->m_dataManager.annotationUids_of_slide(slideUid);

    // Loop over all annotations for each slide
    for (const auto& annotUid : annotUids)
    {
      auto annotRecord = m_impl->m_dataManager.slideAnnotationRecord(annotUid);
      m_impl->m_slideAnnotationAssembly.setAnnotation(annotRecord);
    }
  }

  m_impl->updateAllViews();
}

std::weak_ptr<IDrawable> AssemblyManager::getRootDrawable(const gui::ViewType& viewType)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  const SceneType sceneType = getSceneType(viewType);

  auto it = m_impl->m_rootDrawables.find(sceneType);
  if (std::end(m_impl->m_rootDrawables) != it)
  {
    return it->second;
  }

  // Since it does not yet exist, create and hold on to the shared pointer for this view type
  auto sceneRoot = m_impl->constructSceneRoot(sceneType);
  m_impl->m_rootDrawables.emplace(sceneType, sceneRoot);

  return sceneRoot;
}

std::weak_ptr<IDrawable> AssemblyManager::getOverlayRootDrawable(const gui::ViewType& viewType)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  const SceneType sceneType = getSceneType(viewType);

  auto it = m_impl->m_overlayDrawables.find(sceneType);
  if (std::end(m_impl->m_overlayDrawables) != it)
  {
    return it->second;
  }

  // Since it does not yet exist, create and hold on to the shared pointer for this view type
  auto root = m_impl->constructOverlayRoot(sceneType);
  m_impl->m_overlayDrawables.emplace(sceneType, root);

  return root;
}

void AssemblyManager::setSlideStackMasterOpacityMultiplier(float opacity)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  m_impl->m_slideStackAssembly.setMasterOpacityMultiplier(opacity);
  m_impl->updateAllViews();
}

void AssemblyManager::setSlideStackImage3dLayerOpacity(float opacity)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  m_impl->m_slideStackAssembly.setImage3dLayerOpacityMultiplier(opacity);
  m_impl->updateAllViews();
}

void AssemblyManager::setSlideStackVisibleIn2dViews(bool visible)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  m_impl->m_slideStackAssembly.setVisibleIn2dViews(visible);
  m_impl->updateAllViews();
}

void AssemblyManager::setSlideStackVisibleIn3dViews(bool visible)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  m_impl->m_slideStackAssembly.setVisibleIn3dViews(visible);
  m_impl->updateAllViews();
}

void AssemblyManager::setActiveSlideViewShows2dSlides(bool show2d)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  m_impl->m_slideStackAssembly.setActiveSlideViewShows2dSlides(show2d);

  // Change the scene type of the Stack_ActiveSlide view type according to whether
  // or not slides are shown as 2D slices or as 3D boxes
  m_impl->m_viewTypeToSceneType[gui::ViewType::Stack_ActiveSlide] = (show2d)
                                                                      ? SceneType::SlideStack2d
                                                                      : SceneType::SlideStack3d;

  m_impl->updateAllViews();
}

void AssemblyManager::setLabelMeshMasterOpacity(float opacity)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  m_impl->m_labelMeshAssembly.setMasterOpacityMultiplier(opacity);
  m_impl->updateAllViews();
}

void AssemblyManager::setIsoMeshMasterOpacity(float opacity)
{
  if (!m_impl)
  {
    throw_debug("Null implementation")
  }

  m_impl->m_isoSurfaceMeshAssembly.setMasterOpacityMultiplier(opacity);
  m_impl->updateAllViews();
}

void AssemblyManager::setImageSlicesVisibleIn2dViews(bool visible)
{
  m_impl->m_imageSliceAssembly.setVisibleIn2dViews(visible);
}

void AssemblyManager::setImageSlicesVisibleIn3dViews(bool visible)
{
  m_impl->m_imageSliceAssembly.setVisibleIn3dViews(visible);
}

void AssemblyManager::setImageSlicesAutoHiding(bool useAutoHiding)
{
  m_impl->m_imageSliceAssembly.setUseAutoHidingMode(useAutoHiding);

  /// @todo Can this cause ringing?
  //    if ( auto activeImageUID = m_impl->m_dataManager.activeImageUID() )
  //    {
  //        m_impl->m_signalImageSlicePropertiesChanged( *activeImageUID, m_impl->m_imageSliceProperties );
  //    }
}

void AssemblyManager::setParcellationVisibleIn2dViews(bool visible)
{
  m_impl->m_imageSliceAssembly.showShowParcellationIn2dViews(visible);
}

void AssemblyManager::setParcellationVisibleIn3dViews(bool visible)
{
  m_impl->m_imageSliceAssembly.showShowParcellationIn3dViews(visible);
}

void AssemblyManager::setIsoMeshesVisibleIn2dViews(bool visible)
{
  m_impl->m_isoSurfaceMeshAssembly.setShowIn2dViews(visible);
}

void AssemblyManager::setIsoMeshesVisibleIn3dViews(bool visible)
{
  m_impl->m_isoSurfaceMeshAssembly.setShowIn3dViews(visible);
}

void AssemblyManager::setLabelMeshesVisibleIn2dViews(bool visible)
{
  m_impl->m_labelMeshAssembly.setShowIn2dViews(visible);
}

void AssemblyManager::setLabelMeshesVisibleIn3dViews(bool visible)
{
  if (visible != m_impl->m_labelMeshAssembly.getRenderingProperties().m_visibleIn3dViews)
  {
    m_impl->m_labelMeshAssembly.setShowIn3dViews(visible);
  }
}

void AssemblyManager::setIsoMeshesUseXrayMode(bool useXrayMode)
{
  m_impl->m_isoSurfaceMeshAssembly.setUseXrayMode(useXrayMode);
}

void AssemblyManager::setIsoMeshesXrayPower(float xrayPower)
{
  if (glm::epsilonNotEqual(
        static_cast<float>(m_impl->m_isoSurfaceMeshAssembly.getRenderingProperties().m_xrayPower),
        xrayPower,
        glm::epsilon<float>()
      ))
  {
    m_impl->m_isoSurfaceMeshAssembly.setXrayPower(xrayPower);
  }
}

void AssemblyManager::setLabelMeshesUseXrayMode(bool useXrayMode)
{
  m_impl->m_labelMeshAssembly.setUseXrayMode(useXrayMode);
}

void AssemblyManager::setLabelMeshesXrayPower(float xrayPower)
{
  if (glm::epsilonNotEqual(
        static_cast<float>(m_impl->m_labelMeshAssembly.getRenderingProperties().m_xrayPower),
        xrayPower,
        glm::epsilon<float>()
      ))
  {
    m_impl->m_labelMeshAssembly.setXrayPower(xrayPower);
  }
}

const LandmarkAssemblyRenderingProperties& AssemblyManager::getRefImageLandmarkRenderingProperties(
) const
{
  return m_impl->m_refImageLandmarkAssembly.getRenderingProperties();
}

const LandmarkAssemblyRenderingProperties& AssemblyManager::getSlideLandmarkRenderingProperties() const
{
  return m_impl->m_slideLandmarkAssembly.getRenderingProperties();
}

const AnnotationAssemblyRenderingProperties& AssemblyManager::getSlideAnnotationRenderingProperties(
) const
{
  return m_impl->m_slideAnnotationAssembly.getRenderingProperties();
}

const ImageSliceAssemblyRenderingProperties& AssemblyManager::getImageSliceRenderingProperties() const
{
  return m_impl->m_imageSliceAssembly.getRenderingProperties();
}

const MeshAssemblyRenderingProperties& AssemblyManager::getIsoMeshRenderingProperties() const
{
  return m_impl->m_isoSurfaceMeshAssembly.getRenderingProperties();
}

const MeshAssemblyRenderingProperties& AssemblyManager::getLabelMeshRenderingProperties() const
{
  return m_impl->m_labelMeshAssembly.getRenderingProperties();
}

const SlideStackAssemblyRenderingProperties& AssemblyManager::getSlideRenderingProperties() const
{
  return m_impl->m_slideStackAssembly.getRenderingProperties();
}

void AssemblyManager::connectToImageSliceAssemblyRenderingPropertiesChangedSignal(
  std::function<void(const UID& imageUid, const ImageSliceAssemblyRenderingProperties&)> slot
)
{
  m_impl->m_signalImageSliceAssemblyRenderingPropertiesChanged.connect(slot);
}

void AssemblyManager::connectToIsoMeshAssemblyRenderingPropertiesChangedSignal(
  std::function<void(const MeshAssemblyRenderingProperties&)> slot
)
{
  m_impl->m_signalIsoMeshAssemblyRenderingPropertiesChanged.connect(slot);
}

void AssemblyManager::connectToLabelMeshAssemblyRenderingPropertiesChangedSignal(
  std::function<void(const MeshAssemblyRenderingProperties&)> slot
)
{
  m_impl->m_signalLabelMeshAssemblyRenderingPropertiesChanged.connect(slot);
}

void AssemblyManager::connectToSlideStackAssemblyRenderingPropertiesChangedSignal(
  std::function<void(const SlideStackAssemblyRenderingProperties&)> slot
)
{
  m_impl->m_signalSlideStackAssemblyRenderingPropertiesChanged.connect(slot);
}

void AssemblyManager::connectToSlideTransformationsChangedSignal(
  std::function<void(const std::list<UID>& slideUids)> slot
)
{
  m_impl->m_signalSlideTransformationsChanged.connect(slot);
}

/////////////////////////////////////////////////////////////////////////////

AssemblyManager::Impl::Impl(
  DataManager& dataManager,
  ShaderProgramActivatorType shaderActivator,
  UniformsProviderType uniformsProvider,
  std::weak_ptr<BlankTextures> blankTextures,
  AllViewsUpdaterType allViewsUpdater,
  GetterType<float> slideStackHeightProvider,
  GetterType<glm::mat4> slideStackToWorldTxProvider,
  QuerierType<std::optional<std::pair<glm::mat4, glm::mat4> >, UID>
    refImageLandmarkGroupToWorldTxQuerier,
  QuerierType<std::optional<std::pair<glm::mat4, glm::mat4> >, UID>
    slideLandmarkGroupToSlideStackTxQuerier,
  QuerierType<DrawableScaling, UID> refImageLandmarkGroupScalingQuerier,
  QuerierType<DrawableScaling, UID> slideLandmarkGroupScalingQuerier,
  QuerierType<float, UID> slideAnnotationThicknessQuerier,
  QuerierType<bool, UID> activeSlideQuerier,
  GetterType<glm::mat4> activeSubjectToWorldProvider
)
  : m_dataManager(dataManager)
  ,

  m_cameraLabelAssembly(shaderActivator, uniformsProvider, activeSubjectToWorldProvider)
  ,

  m_crosshairsAssembly(shaderActivator, uniformsProvider)
  ,

  m_imageSliceAssembly(shaderActivator, uniformsProvider, blankTextures)
  ,

  m_slideStackAssembly(
    shaderActivator,
    uniformsProvider,
    blankTextures,
    slideStackHeightProvider,
    slideStackToWorldTxProvider,
    activeSlideQuerier
  )
  ,

  m_isoSurfaceMeshAssembly(shaderActivator, uniformsProvider, blankTextures)
  ,

  m_labelMeshAssembly(shaderActivator, uniformsProvider, blankTextures)
  ,

  m_slideLandmarkAssembly(
    shaderActivator,
    uniformsProvider,

    []()
    {
      return gpuhelper::createCylinderMeshGpuRecord(
        sk_slideLmCylinderCenter, sk_slideLmCylinderRadius, sk_slideLmCylinderHeight
      );
    },

    slideLandmarkGroupToSlideStackTxQuerier,
    slideLandmarkGroupScalingQuerier
  )
  ,

  m_refImageLandmarkAssembly(
    shaderActivator,
    uniformsProvider,

    []() { return gpuhelper::createSphereMeshGpuRecord(); },

    refImageLandmarkGroupToWorldTxQuerier,
    refImageLandmarkGroupScalingQuerier
  )
  ,

  m_slideAnnotationAssembly(
    shaderActivator,
    uniformsProvider,
    slideLandmarkGroupToSlideStackTxQuerier,
    slideAnnotationThicknessQuerier
  )
  ,

  m_allViewsUpdater(allViewsUpdater)
  ,

  m_rootDrawables()
  ,

  m_viewTypeToSceneType(smk_defaultViewTypeToSceneTypeMap)
{
}

void AssemblyManager::Impl::initialize()
{
  m_cameraLabelAssembly.initialize();
  m_crosshairsAssembly.initialize();
  m_imageSliceAssembly.initialize();
  m_slideStackAssembly.initialize();
  m_isoSurfaceMeshAssembly.initialize();
  m_labelMeshAssembly.initialize();
  m_refImageLandmarkAssembly.initialize();
  m_slideLandmarkAssembly.initialize();
  m_slideAnnotationAssembly.initialize();
}

/// @todo This should be done automatically on render via callback!
void AssemblyManager::Impl::updateCrosshairDimensions(const UID& imageUid)
{
  auto imageRecord = m_dataManager.imageRecord(imageUid).lock();
  if (!imageRecord || !imageRecord->cpuData())
  {
    std::cerr << "Null record for image " << imageUid << std::endl;
    return;
  }

  const auto& header = imageRecord->cpuData()->header();
  const float boxSize = static_cast<float>(glm::compMax(header.m_boundingBoxSize));
  const float voxelSize = static_cast<float>(glm::length(header.m_spacing));

  m_crosshairsAssembly.setCrosshairs2dLength(boxSize);
  m_crosshairsAssembly.setCrosshairs3dLength(0.05f * boxSize); //!< @todo This might still be too big
  m_slideStackAssembly.setArrowRadius(2.0f * voxelSize);

  updateAllViews();
}

std::shared_ptr<DrawableBase> AssemblyManager::Impl::constructSceneRoot(const SceneType& sceneType)
{
  /// @todo add SceneType to this name
  auto sceneRoot = std::make_shared<Transformation>("AssemblyManager_sceneRoot");

  switch (sceneType)
  {
  case SceneType::ReferenceImage2d:
  {
    sceneRoot->addChild(m_imageSliceAssembly.getRoot(sceneType));
    sceneRoot->addChild(m_slideStackAssembly.getRoot(sceneType));

    sceneRoot->addChild(m_refImageLandmarkAssembly.getRoot(sceneType));
    sceneRoot->addChild(m_slideLandmarkAssembly.getRoot(sceneType));
    sceneRoot->addChild(m_slideAnnotationAssembly.getRoot(sceneType));

    sceneRoot->addChild(m_labelMeshAssembly.getRoot(sceneType));
    sceneRoot->addChild(m_isoSurfaceMeshAssembly.getRoot(sceneType));
    break;
  }
  case SceneType::ReferenceImage3d:
  {
    sceneRoot->addChild(m_imageSliceAssembly.getRoot(sceneType));
    sceneRoot->addChild(m_slideStackAssembly.getRoot(sceneType));

    sceneRoot->addChild(m_refImageLandmarkAssembly.getRoot(sceneType));
    sceneRoot->addChild(m_slideLandmarkAssembly.getRoot(sceneType));
    sceneRoot->addChild(m_slideAnnotationAssembly.getRoot(sceneType));

    sceneRoot->addChild(m_labelMeshAssembly.getRoot(sceneType));
    sceneRoot->addChild(m_isoSurfaceMeshAssembly.getRoot(sceneType));
    break;
  }
  case SceneType::SlideStack2d:
  {
    sceneRoot->addChild(m_imageSliceAssembly.getRoot(sceneType));
    sceneRoot->addChild(m_slideStackAssembly.getRoot(sceneType));

    sceneRoot->addChild(m_refImageLandmarkAssembly.getRoot(sceneType));
    sceneRoot->addChild(m_slideLandmarkAssembly.getRoot(sceneType));
    sceneRoot->addChild(m_slideAnnotationAssembly.getRoot(sceneType));

    sceneRoot->addChild(m_labelMeshAssembly.getRoot(sceneType));
    sceneRoot->addChild(m_isoSurfaceMeshAssembly.getRoot(sceneType));
    break;
  }
  case SceneType::SlideStack3d:
  {
    sceneRoot->addChild(m_slideStackAssembly.getRoot(sceneType));

    sceneRoot->addChild(m_slideLandmarkAssembly.getRoot(sceneType));
    sceneRoot->addChild(m_slideAnnotationAssembly.getRoot(sceneType));

    sceneRoot->addChild(m_labelMeshAssembly.getRoot(sceneType));
    sceneRoot->addChild(m_isoSurfaceMeshAssembly.getRoot(sceneType));
    break;
  }
  case SceneType::Registration_Image2d:
  {
    sceneRoot->addChild(m_imageSliceAssembly.getRoot(sceneType));
    sceneRoot->addChild(m_refImageLandmarkAssembly.getRoot(sceneType));
    break;
  }
  case SceneType::Registration_Slide2d:
  {
    sceneRoot->addChild(m_slideStackAssembly.getRoot(sceneType));

    sceneRoot->addChild(m_slideLandmarkAssembly.getRoot(sceneType));
    sceneRoot->addChild(m_slideAnnotationAssembly.getRoot(sceneType));
    break;
  }
  case SceneType::None:
  {
    return sceneRoot;
  }
  }

  // Add crosshairs to all scenes:
  sceneRoot->addChild(m_crosshairsAssembly.getRoot(sceneType));

  return sceneRoot;
}

std::shared_ptr<DrawableBase> AssemblyManager::Impl::constructOverlayRoot(const SceneType& sceneType)
{
  auto overlayRoot = std::make_shared<Transformation>("AssemblyManager_overlayRoot");

  switch (sceneType)
  {
  case SceneType::ReferenceImage2d:
  case SceneType::ReferenceImage3d:
  case SceneType::SlideStack2d:
  case SceneType::SlideStack3d:
  case SceneType::Registration_Image2d:
  case SceneType::Registration_Slide2d:
  {
    overlayRoot->addChild(m_cameraLabelAssembly.getRoot(sceneType));
    break;
  }
  case SceneType::None:
  {
    break;
  }
  }

  return overlayRoot;
}

void AssemblyManager::Impl::updateAllViews()
{
  if (m_allViewsUpdater)
  {
    m_allViewsUpdater();
  }
}
