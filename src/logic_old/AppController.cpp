#include "logic/AppController.h"

#include "common/HZeeException.hpp"

#include "logic/managers/ActionManager.h"
#include "logic/managers/AssemblyManager.h"
#include "logic/managers/ConnectionManager.h"
#include "logic/managers/DataManager.h"
#include "logic/managers/GuiManager.h"
#include "logic/managers/InteractionManager.h"
#include "logic/managers/LayoutManager.h"
#include "logic/managers/TransformationManager.h"

#include "logic/data/DataLoading.h"
#include "logic/ui/ImageDataUiMapper.h"
#include "logic/ui/ParcellationDataUiMapper.h"
#include "logic/ui/SlideStackDataUiMapper.h"
#include "logic/utility/DirectionMaps.h"

#include "rendering/utility/containers/BlankTextures.h"
#include "rendering/utility/containers/ShaderProgramContainer.h"
#include "rendering/utility/gl/GLVersionChecker.h"

/////// START INCLUDES FOR TESTING ////////
#include "logic/annotation/AnnotationHelper.h"
#include "logic/annotation/PlanarPolygon.h"
#include "logic/records/LandmarkGroupRecord.h"
#include "logic/records/SlideAnnotationRecord.h"
#include "logic/serialization/ProjectSerialization.h"
#include "rendering/utility/CreateGLObjects.h"
/////// END INCLUDES FOR TESTING ////////

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <QOpenGLContext>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace
{

static const std::string sk_glContextErrorMsg(
  "The global shared OpenGL context could not be made current."
);

/// @test This transformation is hard-coded for the Allen V1 dataset, which is oriented coronally.
/// The horizontal/vertical pixel dimensions correspond to R->L and S->I, respecively.
/// The stacking direction is A->P.
CoordinateFrame makeTestSlideStackFrame(const imageio::ImageHeader& header)
{
  static constexpr bool requireEqualAngles = true;

  CoordinateFrame stackFrame;

  stackFrame.setWorldOrigin(glm::vec3{header.m_origin});

  stackFrame.setFrameToWorldRotation(
    Directions::get(Directions::Cartesian::X),
    Directions::get(Directions::Anatomy::Left),
    Directions::get(Directions::Cartesian::Y),
    Directions::get(Directions::Anatomy::Inferior),
    requireEqualAngles
  );

  return stackFrame;
}

/// @test Make a polygon to test slide annotations
std::unique_ptr<PlanarPolygon> makeTestPolygon(const glm::vec2& center)
{
  auto polygon3 = std::make_unique<PlanarPolygon>();

  std::vector<glm::vec2> outerBoundary;

  for (int i = 0; i < 32; ++i)
  {
    float x = 0.5f * std::cos(2.0f * i * M_PI / 32.0f) + center.x;
    float y = 0.5f * std::sin(2.0f * i * M_PI / 32.0f) + center.y;
    outerBoundary.push_back(glm::vec2{x, y});
  }

  std::vector<glm::vec2> hole1;
  std::vector<glm::vec2> hole2;

  for (int i = 0; i < 8; ++i)
  {
    float x = 0.15f * std::cos(2.0f * i * M_PI / 8.0f) + center.x;
    float y = 0.15f * std::sin(2.0f * i * M_PI / 8.0f) + center.y;

    hole1.push_back(glm::vec2{x - 0.25f, y});
    hole2.push_back(glm::vec2{x + 0.25f, y});
  }

  auto p = std::make_unique<Polygon>();
  p->setOuterBoundary(outerBoundary);
  p->addHole(hole1);
  p->addHole(hole2);

  return p;
}

} // namespace

AppController::AppController(
  std::unique_ptr<ActionManager> actionManager,
  std::unique_ptr<AssemblyManager> assemblyManager,
  std::unique_ptr<ConnectionManager> connectionManager,
  std::unique_ptr<DataManager> dataManager,
  std::unique_ptr<GuiManager> guiManager,
  std::unique_ptr<InteractionManager> interactionManager,
  std::unique_ptr<LayoutManager> layoutManager,
  std::unique_ptr<TransformationManager> transformationManager,
  std::unique_ptr<ImageDataUiMapper> imageDataUiMapper,
  std::unique_ptr<ParcellationDataUiMapper> parcelDataUiMapper,
  std::unique_ptr<SlideStackDataUiMapper> slideDataUiMapper,
  std::unique_ptr<ShaderProgramContainer> shaderPrograms,
  std::shared_ptr<BlankTextures> blankTextures
)
  : m_actionManager(std::move(actionManager))
  , m_assemblyManager(std::move(assemblyManager))
  , m_connectionManager(std::move(connectionManager))
  , m_dataManager(std::move(dataManager))
  , m_guiManager(std::move(guiManager))
  , m_interactionManager(std::move(interactionManager))
  , m_layoutManager(std::move(layoutManager))
  , m_transformationManager(std::move(transformationManager))
  ,

  m_imageDataUiMapper(std::move(imageDataUiMapper))
  , m_parcelDataUiMapper(std::move(parcelDataUiMapper))
  , m_slideStackDataUiMapper(std::move(slideDataUiMapper))
  ,

  m_shaderPrograms(std::move(shaderPrograms))
  , m_blankTextures(std::move(blankTextures))
  ,

  m_globalContext(QOpenGLContext::globalShareContext())
{
  if (!m_actionManager || !m_assemblyManager || !m_connectionManager || !m_dataManager || !m_guiManager || !m_interactionManager || !m_layoutManager || !m_transformationManager || !m_imageDataUiMapper || !m_parcelDataUiMapper || !m_slideStackDataUiMapper || !m_shaderPrograms || !m_blankTextures)
  {
    throw_debug("Attempting to construct AppController with a null argument.")
  }

  if (!m_globalContext || !m_globalContext->isValid())
  {
    std::ostringstream ss;
    ss << "The global, shared OpenGL context is invalid." << std::ends;
    throw_debug(ss.str())
  }

  // Set the offscreen render surface format to match that of the global context.
  // We could also use the default format QSurfaceFormat::defaultFormat()
  m_surface.setFormat(m_globalContext->format());
  m_surface.create();

  initialize();

  createUiConnections();
}

AppController::~AppController() = default;

void AppController::initialize()
{
  if (!m_actionManager || !m_assemblyManager || !m_connectionManager || !m_dataManager || !m_guiManager || !m_interactionManager || !m_layoutManager || !m_transformationManager || !m_imageDataUiMapper || !m_parcelDataUiMapper || !m_slideStackDataUiMapper || !m_shaderPrograms || !m_blankTextures)
  {
    throw_debug("Attempting to initialize AppController with a null manager.")
  }

  if (m_globalContext && m_globalContext->makeCurrent(&m_surface))
  {
    // Initialization requiring OpenGL context goes here!!!

    // This object checks the OpenGL version and throws an exception
    // if it is below version 3.3:
    GLVersionChecker versionChecker;

    m_blankTextures->initializeGL();
    m_shaderPrograms->initializeGL();

    m_assemblyManager->initializeGL();
    m_guiManager->initializeGL();

    m_globalContext->doneCurrent();
  }
  else
  {
    throw_debug(sk_glContextErrorMsg)
  }

  m_connectionManager->createConnections();

  m_guiManager->setupMainWindow();

  /// @todo move to ConnectionManager:

  // Add the layout widgets to the MainWindow's view layout tab widget in order:
  m_guiManager->clearTabWidget();

  int tabIndex = 0;

  for (const UID& layoutUid : m_layoutManager->getOrderedLayoutUids())
  {
    const auto& layoutData = m_layoutManager->getLayoutTabData(layoutUid);

    m_guiManager
      ->insertViewLayoutTab(tabIndex++, layoutData.m_containerWidget, layoutData.m_displayName);
  }
}

void AppController::createUiConnections()
{
  using namespace std::placeholders;

  // Publish changes to UI:
  m_imageDataUiMapper->setImageSelectionsPublisher_msgToUi(
    std::bind(&GuiManager::sendImageSelectionsToUi, m_guiManager.get(), _1)
  );
  m_imageDataUiMapper->setImageColorMapsPublisher_msgToUi(
    std::bind(&GuiManager::sendImageColorMapsToUi, m_guiManager.get(), _1)
  );
  m_imageDataUiMapper->setImagePropertiesPartialPublisher_msgToUi(
    std::bind(&GuiManager::sendImagePropertiesPartialToUi, m_guiManager.get(), _1)
  );
  m_imageDataUiMapper->setImagePropertiesCompletePublisher_msgToUi(
    std::bind(&GuiManager::sendImagePropertiesCompleteToUi, m_guiManager.get(), _1)
  );
  m_imageDataUiMapper->setImageTransformationPublisher_msgToUi(
    std::bind(&GuiManager::sendImageTransformationToUi, m_guiManager.get(), _1)
  );

  m_parcelDataUiMapper->setParcellationSelectionsPublisher_msgToUi(
    std::bind(&GuiManager::sendParcellationSelectionsToUi, m_guiManager.get(), _1)
  );
  m_parcelDataUiMapper->setParcellationPropertiesPartialPublisher_msgToUi(
    std::bind(&GuiManager::sendParcellationPropertiesPartialToUi, m_guiManager.get(), _1)
  );
  m_parcelDataUiMapper->setParcellationPropertiesCompletePublisher_msgToUi(
    std::bind(&GuiManager::sendParcellationPropertiesCompleteToUi, m_guiManager.get(), _1)
  );
  m_parcelDataUiMapper->setParcellationLabelsCompletePublisher_msgToUi(
    std::bind(&GuiManager::sendParcellationLabelsCompleteToUi, m_guiManager.get(), _1)
  );

  m_slideStackDataUiMapper->setSlideStackPartialPublisher_msgToUi(
    std::bind(&GuiManager::sendSlideStackPartialToUi, m_guiManager.get(), _1)
  );
  m_slideStackDataUiMapper->setSlideStackCompletePublisher_msgToUi(
    std::bind(&GuiManager::sendSlideStackCompleteToUi, m_guiManager.get(), _1)
  );
  m_slideStackDataUiMapper->setActiveSlidePublisher_msgToUi(
    std::bind(&GuiManager::sendActiveSlideToUi, m_guiManager.get(), _1)
  );
  m_slideStackDataUiMapper->setSlideCommonPropertiesPartialPublisher_msgToUi(
    std::bind(&GuiManager::sendSlideCommonPropertiesPartialToUi, m_guiManager.get(), _1)
  );
  m_slideStackDataUiMapper->setSlideCommonPropertiesCompletePublisher_msgToUi(
    std::bind(&GuiManager::sendSlideCommonPropertiesCompleteToUi, m_guiManager.get(), _1)
  );
  m_slideStackDataUiMapper->setSlideHeaderCompletePublisher_msgToUi(
    std::bind(&GuiManager::setSlideHeaderCompleteToUi, m_guiManager.get(), _1)
  );
  m_slideStackDataUiMapper->setSlideViewDataCompletePublisher_msgToUi(
    std::bind(&GuiManager::setSlideViewDataCompleteToUi, m_guiManager.get(), _1)
  );
  m_slideStackDataUiMapper->setSlideViewDataPartialPublisher_msgToUi(
    std::bind(&GuiManager::setSlideViewDataPartialToUi, m_guiManager.get(), _1)
  );
  m_slideStackDataUiMapper->setSlideTxDataCompletePublisher_msgToUi(
    std::bind(&GuiManager::setSlideTxDataCompleteToUi, m_guiManager.get(), _1)
  );
  m_slideStackDataUiMapper->setSlideTxDataPartialPublisher_msgToUi(
    std::bind(&GuiManager::setSlideTxDataPartialToUi, m_guiManager.get(), _1)
  );

  // Publish changes from UI:
  m_guiManager->setImageSelectionsPublisher(
    std::bind(&ImageDataUiMapper::setImageSelections_msgFromUi, m_imageDataUiMapper.get(), _1)
  );
  m_guiManager->setImagePropertiesPartialPublisher(
    std::bind(&ImageDataUiMapper::setImagePropertiesPartial_msgFromUi, m_imageDataUiMapper.get(), _1)
  );
  m_guiManager->setImageTransformationPublisher(
    std::bind(&ImageDataUiMapper::setImageTransformation_msgFromUi, m_imageDataUiMapper.get(), _1)
  );

  m_guiManager->setParcellationSelectionsPublisher(std::bind(
    &ParcellationDataUiMapper::setParcellationSelections_fromUi, m_parcelDataUiMapper.get(), _1
  ));
  m_guiManager->setParcellationPropertiesPartialPublisher(std::bind(
    &ParcellationDataUiMapper::setParcellationPropertiesPartial_fromUi, m_parcelDataUiMapper.get(), _1
  ));
  m_guiManager->setParcellationLabelsPartialPublisher(std::bind(
    &ParcellationDataUiMapper::setParcellationLabelsPartial_fromUi, m_parcelDataUiMapper.get(), _1
  ));

  m_guiManager->setSlideStackPartialPublisher(
    std::bind(&SlideStackDataUiMapper::setSlideStackPartial_fromUi, m_slideStackDataUiMapper.get(), _1)
  );
  m_guiManager->setSlideStackOrderPublisher(
    std::bind(&SlideStackDataUiMapper::setSlideStackOrder_fromUi, m_slideStackDataUiMapper.get(), _1)
  );
  m_guiManager->setActiveSlidePublisher(
    std::bind(&SlideStackDataUiMapper::setActiveSlide_fromUi, m_slideStackDataUiMapper.get(), _1)
  );
  m_guiManager->setSlideCommonPropertiesPartialPublisher(std::bind(
    &SlideStackDataUiMapper::setSlideCommonPropertiesPartial_fromUi,
    m_slideStackDataUiMapper.get(),
    _1
  ));
  m_guiManager->setSlideHeaderPartialPublisher(std::bind(
    &SlideStackDataUiMapper::setSlideHeaderPartial_fromUi, m_slideStackDataUiMapper.get(), _1
  ));
  m_guiManager->setSlideViewDataPartialPublisher(std::bind(
    &SlideStackDataUiMapper::setSlideViewDataPartial_fromUi, m_slideStackDataUiMapper.get(), _1
  ));
  m_guiManager->setSlideTxDataPartialPublisher(std::bind(
    &SlideStackDataUiMapper::setSlideTxDataPartial_fromUi, m_slideStackDataUiMapper.get(), _1
  ));
  m_guiManager->setMoveToSlidePublisher(
    std::bind(&SlideStackDataUiMapper::setMoveToSlide_fromUi, m_slideStackDataUiMapper.get(), _1)
  );

  // Respond to requests of UI for data:
  m_guiManager->setImageSelectionsResponder(
    std::bind(&ImageDataUiMapper::getImageSelections_msgToUi, m_imageDataUiMapper.get())
  );
  m_guiManager->setImageColorMapsResponder(
    std::bind(&ImageDataUiMapper::getImageColorMaps_msgToUi, m_imageDataUiMapper.get())
  );
  m_guiManager->setImagePropertiesCompleteResponder(
    std::bind(&ImageDataUiMapper::getImagePropertiesComplete_msgToUi, m_imageDataUiMapper.get(), _1)
  );
  m_guiManager->setImageHeaderResponder(
    std::bind(&ImageDataUiMapper::getImageHeader_msgToUi, m_imageDataUiMapper.get(), _1)
  );
  m_guiManager->setImageTransformationResponder(
    std::bind(&ImageDataUiMapper::getImageTransformation_msgToUi, m_imageDataUiMapper.get(), _1)
  );

  m_guiManager->setParcellationSelectionsResponder(
    std::bind(&ParcellationDataUiMapper::getParcellationSelections_msgToUi, m_parcelDataUiMapper.get())
  );
  m_guiManager->setParcellationPropertiesCompleteResponder(std::bind(
    &ParcellationDataUiMapper::getParcellationPropertiesComplete_msgToUi,
    m_parcelDataUiMapper.get(),
    _1
  ));
  m_guiManager->setParcellationLabelsCompleteResponder(std::bind(
    &ParcellationDataUiMapper::getParcellationLabelsComplete_msgToUi, m_parcelDataUiMapper.get(), _1
  ));
  m_guiManager->setParcellationHeaderResponder(
    std::bind(&ParcellationDataUiMapper::getParcellationHeader_msgToUi, m_parcelDataUiMapper.get(), _1)
  );

  m_guiManager->setSlideStackCompleteResponder(
    std::bind(&SlideStackDataUiMapper::getSlideStackComplete_msgToUi, m_slideStackDataUiMapper.get())
  );
  m_guiManager->setActiveSlideResponder(
    std::bind(&SlideStackDataUiMapper::getActiveSlide_msgToUi, m_slideStackDataUiMapper.get())
  );
  m_guiManager->setSlideCommonPropertiesCompleteResponder(std::bind(
    &SlideStackDataUiMapper::getSlideCommonPropertiesComplete_msgToUi, m_slideStackDataUiMapper.get()
  ));
  m_guiManager->setSlideHeaderCompleteResponder(std::bind(
    &SlideStackDataUiMapper::getSlideHeaderComplete_msgToUi, m_slideStackDataUiMapper.get(), _1
  ));
  m_guiManager->setSlideViewDataCompleteResponder(std::bind(
    &SlideStackDataUiMapper::getSlideViewDataComplete_msgToUi, m_slideStackDataUiMapper.get(), _1
  ));
  m_guiManager->setSlideTxDataCompleteResponder(std::bind(
    &SlideStackDataUiMapper::getSlideTxDataComplete_msgToUi, m_slideStackDataUiMapper.get(), _1
  ));
}

void AppController::showMainWindow()
{
  if (!m_guiManager)
  {
    throw_debug("Unable ot show main window: null GuiManager")
  }

  // Show main window and update its widgets with current property values and rendering
  m_guiManager->showMainWindow();
  m_guiManager->updateAllViewWidgets();
  m_guiManager->updateAllDockWidgets();
}

void AppController::loadProject(serialize::HZeeProject project)
{
  if (!m_dataManager || !m_transformationManager || !m_actionManager)
  {
    throw_debug("Unable to set project: null manager")
  }

  // Load images and set their display settings and transformations
  size_t imageCounter = 0;

  for (const auto& image : project.m_refImages)
  {
    if (const auto imageUid = m_actionManager->loadImage(image.m_fileName, std::nullopt))
    {
      auto imageRec = m_dataManager->imageRecord(*imageUid).lock();

      if (imageRec && imageRec->cpuData())
      {
        auto I = imageRec->cpuData();
        I->setWorldSubjectOrigin(image.m_world_T_subject.worldOrigin());
        I->setSubjectToWorldRotation(image.m_world_T_subject.world_O_frame_rotation());

        const auto& S = image.m_displaySettings;

        /// @todo Set for all image components
        if (S.m_displayName)
          I->setDisplayName(*S.m_displayName);
        if (S.m_opacity)
          I->setOpacity(0, *S.m_opacity);
        if (S.m_window)
          I->setWindowWidth(0, *S.m_window);
        if (S.m_level)
          I->setLevel(0, *S.m_level);
        if (S.m_thresholdLow)
          I->setThresholdLow(0, *S.m_thresholdLow);
        if (S.m_thresholdHigh)
          I->setThresholdHigh(0, *S.m_thresholdHigh);
        if (S.m_interpolationMode)
          I->setInterpolationMode(0, *S.m_interpolationMode);
      }

      // Set active image
      if (imageCounter == project.m_activeRefImage)
      {
        m_dataManager->setActiveImageUid(*imageUid);
      }
    }

    ++imageCounter;
  }

  // Load parcellations and set their display settings and transformations
  size_t parcelCounter = 0;

  for (const auto& parcel : project.m_parcellations)
  {
    if (const auto parcelUid = m_actionManager->loadParcellation(parcel.m_fileName, std::nullopt))
    {
      auto parcelRec = m_dataManager->parcellationRecord(*parcelUid).lock();

      if (parcelRec && parcelRec->cpuData())
      {
        auto P = parcelRec->cpuData();
        P->setWorldSubjectOrigin(parcel.m_world_T_subject.worldOrigin());
        P->setSubjectToWorldRotation(parcel.m_world_T_subject.world_O_frame_rotation());

        const auto& S = parcel.m_displaySettings;

        if (S.m_displayName)
          P->setDisplayName(*S.m_displayName);
        if (S.m_opacity)
          P->setOpacity(0, *S.m_opacity);
        if (S.m_window)
          P->setWindowWidth(0, *S.m_window);
        if (S.m_level)
          P->setLevel(0, *S.m_level);
        if (S.m_thresholdLow)
          P->setThresholdLow(0, *S.m_thresholdLow);
        if (S.m_thresholdHigh)
          P->setThresholdHigh(0, *S.m_thresholdHigh);
        if (S.m_interpolationMode)
          P->setInterpolationMode(0, *S.m_interpolationMode);
      }

      // Set active parcellation
      if (project.m_activeParcellation && (parcelCounter == *project.m_activeParcellation))
      {
        m_dataManager->setActiveParcellationUid(*parcelUid);
      }
    }

    ++parcelCounter;
  }

  // Load images and set their properties and transformations
  for (const auto& slide : project.m_slides)
  {
    if ( const auto slideUid = m_actionManager->loadSlide(
                 slide.m_fileName, slide.m_slideStack_T_slide.autoTranslateToTopOfStack() ) )
    {
      auto slideRec = m_dataManager->slideRecord(*slideUid).lock();

      if (slideRec && slideRec->cpuData())
      {
        auto* S = slideRec->cpuData();

        // Prior to over-writing the displayName (which gets set on slide loading),
        // let's save it off. After loading properties, put back the saved display name
        // if the new one is empty.
        const auto savedDisplayName = S->properties().displayName();
        S->setProperties(slide.m_properties);
        if (S->properties().displayName().empty())
          S->properties().setDisplayName(savedDisplayName);
      }
    }
  }

  // Set slide stack transformation:
  m_transformationManager->stageSlideStackFrame(project.m_world_T_slideStack);
  m_transformationManager->commitSlideStackFrame();

  // Update all visual assemblies, since data has changed:
  m_actionManager->updateAllAssemblies();

  // Hold on to the project, so that it can be modified and saved again:
  m_dataManager->setProject(std::move(project));
}

void AppController::generateIsoSurfaceMesh(double isoValue)
{
  if (!m_actionManager)
  {
    throw_debug("Unable to generate isosurface mesh: null ActionManager")
  }

  m_actionManager->generateIsoSurfaceMesh(isoValue);
}

void AppController::generateLabelMeshes()
{
  if (!m_actionManager)
  {
    throw_debug("Unable to generate label meshes: null ActionManager")
  }

  m_actionManager->generateLabelMeshes();
}

void AppController::setupCamerasAndCrosshairsForImage()
{
  if (!m_actionManager)
  {
    throw_debug("Unable to set up cameras: null ActionManager")
  }

  m_actionManager->setupCamerasAndCrosshairsForImage();
}

void AppController::loadBuiltInImageColorMaps(const std::vector<std::string>& colorMapFileNames)
{
  if (!m_dataManager)
  {
    return;
  }

  if (m_globalContext && m_globalContext->makeCurrent(&m_surface))
  {
    // First, load the default greyscale color map,
    // which is not provided as a CSV file in the resources directory.
    data::loadDefaultGreyscaleColorMap(*m_dataManager);

    // Next, load all color maps from the resources directory
    for (const auto& name : colorMapFileNames)
    {
      //            std::cout << "Loading built-in color map from file " << name << std::endl;
      data::loadImageColorMap(*m_dataManager, name);
    }

    m_globalContext->doneCurrent();
  }
  else
  {
    throw_debug(sk_glContextErrorMsg)
  }
}

void AppController::testTransformFeedback()
{
  m_actionManager->transformFeedback();
}

void AppController::testAlignSlideStackToActiveImage()
{
  if (!m_transformationManager || !m_dataManager)
  {
    return;
  }

  auto activeImageRecord = m_dataManager->activeImageRecord().lock();
  if (!activeImageRecord)
  {
    return;
  }

  auto cpuRecord = activeImageRecord->cpuData();
  if (!cpuRecord)
  {
    return;
  }

  auto frame = makeTestSlideStackFrame(cpuRecord->header());

  m_transformationManager->stageSlideStackFrame(std::move(frame));
  m_transformationManager->commitSlideStackFrame();
}

void AppController::testCreateRefImageLandmark()
{
  using PointRecordType = PointRecord<glm::vec3>;

  PointList<PointRecordType> pointList1;
  pointList1.appendPoint(PointRecordType{glm::vec3{0.0f}});
  pointList1.appendPoint(PointRecordType{glm::vec3{0.5f}});
  pointList1.appendPoint(PointRecordType{glm::vec3{1.0f}});

  auto cpuRecord1 = std::make_unique<LandmarkGroupCpuRecord>();
  cpuRecord1->setPoints(std::move(pointList1));
  cpuRecord1->setColor(glm::vec3{0.5f, 0.5f, 1.0f});

  auto testLandmarkGroupRecord1 = std::make_shared<
    LandmarkGroupRecord>(std::move(cpuRecord1), std::make_unique<EmptyGpuRecord>());

  PointList<PointRecordType> pointList2;
  pointList2.appendPoint(PointRecordType{glm::vec3{1.5f}});
  pointList2.appendPoint(PointRecordType{glm::vec3{2.0f}});
  pointList2.appendPoint(PointRecordType{glm::vec3{2.5f}});

  auto cpuRecord2 = std::make_unique<LandmarkGroupCpuRecord>();
  cpuRecord2->setPoints(std::move(pointList2));
  cpuRecord2->setColor(glm::vec3{1.0f, 0.5f, 0.5f});

  auto testLandmarkGroupRecord2 = std::make_shared<
    LandmarkGroupRecord>(std::move(cpuRecord2), std::make_unique<EmptyGpuRecord>());

  if (const auto activeImageUid = m_dataManager->activeImageUid())
  {
    const auto group1Uid
      = m_dataManager->insertRefImageLandmarkGroupRecord(*activeImageUid, testLandmarkGroupRecord1);

    const auto group2Uid
      = m_dataManager->insertRefImageLandmarkGroupRecord(*activeImageUid, testLandmarkGroupRecord2);

    if (!group1Uid || !group2Uid)
    {
      std::cerr << "Error inserting reference image landmarks" << std::endl;
    }
  }

  m_actionManager->updateLandmarkAssemblies();
}

void AppController::testCreateSlideLandmark()
{
  using PointType = PointRecord<glm::vec3>;
  PointList<PointType> pointList;

  for (int i = 0; i < 6; ++i)
  {
    float x = 0.25f * std::cos(2.0f * i * M_PI / 6.0f) + 0.5f;
    float y = 0.25f * std::sin(2.0f * i * M_PI / 6.0f) + 0.5f;
    pointList.appendPoint(PointType{glm::vec3{x, y, 0.5f}});
  }

  auto cpuRecord = std::make_unique<LandmarkGroupCpuRecord>();
  cpuRecord->setPoints(std::move(pointList));
  cpuRecord->setColor(glm::vec3{0.8f, 0.2f, 0.1f});

  auto testPointLandmarkRecord
    = std::make_shared<LandmarkGroupRecord>(std::move(cpuRecord), std::make_unique<EmptyGpuRecord>());

  if (auto activeSlideUid = m_dataManager->activeSlideUid())
  {
    std::cout << "Inserting slide landmark group for slide " << *activeSlideUid << std::endl;

    const auto groupUid
      = m_dataManager->insertSlideLandmarkGroupRecord(*activeSlideUid, testPointLandmarkRecord);

    if (!groupUid)
    {
      std::cerr << "Error inserting slide landmark" << std::endl;
    }
  }

  m_actionManager->updateLandmarkAssemblies();
}

void AppController::testCreateSlideAnnotation()
{
  auto polygon1 = makeTestPolygon(glm::vec2{0.25f, 0.5f});
  auto polygon2 = makeTestPolygon(glm::vec2{0.45f, 0.5f});
  auto polygon3 = makeTestPolygon(glm::vec2{0.65f, 0.5f});

  triangulatePolygon(*polygon1);
  triangulatePolygon(*polygon2);
  triangulatePolygon(*polygon3);

  std::unique_ptr<SlideAnnotationGpuRecord> annot1GpuRecord;
  std::unique_ptr<SlideAnnotationGpuRecord> annot2GpuRecord;
  std::unique_ptr<SlideAnnotationGpuRecord> annot3GpuRecord;

  if (m_globalContext && m_globalContext->makeCurrent(&m_surface))
  {
    annot1GpuRecord = gpuhelper::createSlideAnnotationGpuRecord(*polygon1);
    annot2GpuRecord = gpuhelper::createSlideAnnotationGpuRecord(*polygon2);
    annot3GpuRecord = gpuhelper::createSlideAnnotationGpuRecord(*polygon3);

    m_globalContext->doneCurrent();
  }
  else
  {
    throw_debug(sk_glContextErrorMsg)
  }

  std::unique_ptr<SlideAnnotationCpuRecord> annot1CpuRecord
    = std::make_unique<SlideAnnotationCpuRecord>(std::move(polygon1));

  std::unique_ptr<SlideAnnotationCpuRecord> annot2CpuRecord
    = std::make_unique<SlideAnnotationCpuRecord>(std::move(polygon2));

  std::unique_ptr<SlideAnnotationCpuRecord> annot3CpuRecord
    = std::make_unique<SlideAnnotationCpuRecord>(std::move(polygon3));

  annot1CpuRecord->setColor(glm::vec3{1.0f, 0.2f, 0.1f});
  annot2CpuRecord->setColor(glm::vec3{0.2f, 1.0f, 0.1f});
  annot3CpuRecord->setColor(glm::vec3{0.2f, 0.1f, 1.0f});

  annot1CpuRecord->setOpacity(1.0f);
  annot2CpuRecord->setOpacity(1.0f);
  annot3CpuRecord->setOpacity(1.0f);

  auto annot1Record
    = std::make_shared<SlideAnnotationRecord>(std::move(annot1CpuRecord), std::move(annot1GpuRecord));

  auto annot2Record
    = std::make_shared<SlideAnnotationRecord>(std::move(annot2CpuRecord), std::move(annot2GpuRecord));

  auto annot3Record
    = std::make_shared<SlideAnnotationRecord>(std::move(annot3CpuRecord), std::move(annot3GpuRecord));

  if (auto activeSlideUid = m_dataManager->activeSlideUid())
  {
    const auto annot1Uid = m_dataManager->insertSlideAnnotationRecord(*activeSlideUid, annot1Record);
    const auto annot2Uid = m_dataManager->insertSlideAnnotationRecord(*activeSlideUid, annot2Record);
    const auto annot3Uid = m_dataManager->insertSlideAnnotationRecord(*activeSlideUid, annot3Record);

    if (!annot1Uid || !annot2Uid || !annot3Uid)
    {
      std::cerr << "Error inserting slide annotations" << std::endl;
    }

    // Test layer change
    setUniqueSlideAnnotationLayers(*m_dataManager);
    //        changeSlideAnnotationLayering( *m_dataManager, *annot1Uid, LayerChangeType::Backwards );
  }

  m_actionManager->updateAnnotationAssemblies();
}
