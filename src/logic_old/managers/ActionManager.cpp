#include "logic/managers/ActionManager.h"
#include "logic/managers/AssemblyManager.h"
#include "logic/managers/DataManager.h"
#include "logic/managers/GuiManager.h"
#include "logic/managers/InteractionManager.h"

#include "logic/camera/Camera.h"
#include "logic/data/DataHelper.h"
#include "logic/data/DataLoading.h"
#include "logic/records/ImageRecord.h"
#include "logic/records/LabelTableRecord.h"
#include "logic/records/SlideRecord.h"
#include "logic/serialization/ProjectSerialization.h"

#include "logic/interaction/CameraInteractionHandler.h"
#include "logic/interaction/CrosshairsInteractionHandler.h"
#include "logic/interaction/InteractionModes.h"
#include "logic/interaction/InteractionPack.h"
#include "logic/interaction/RefImageInteractionHandler.h"
#include "logic/interaction/SlideInteractionHandler.h"
#include "logic/interaction/StackInteractionHandler.h"
#include "logic/interaction/WindowLevelInteractionHandler.h"

#include "common/HZeeException.hpp"
#include "common/Utility.hpp"

#include "gui/view/ViewWidget.h"

#include "imageio/HZeeTypes.hpp"
#include "imageio/util/MathFuncs.hpp"
#include "slideio/SlideHelper.h"

#include "rendering/computers/Polygonizer.h"
#include "rendering/utility/math/MathUtility.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_precision.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <boost/format.hpp>

#include <QOpenGLContext>
#include <QOpenGLWidget>

#include <optional>
#include <sstream>

namespace
{

static const std::string sk_glContextErrorMsg(
  "The global shared OpenGL context could not be made current."
);

} // namespace

ActionManager::ActionManager(
  GetterType<view_type_range_t> viewUidAndTypeProvider,
  ShaderProgramActivatorType shaderProgramActivator,
  UniformsProviderType uniformsProvider,
  AssemblyManager& assemblyManager,
  DataManager& dataManager,
  GuiManager& guiManager,
  InteractionManager& interactionManager
)
  : m_globalContext(QOpenGLContext::globalShareContext())
  ,

  m_viewUidAndTypeProvider(viewUidAndTypeProvider)
  , m_shaderProgramActivator(shaderProgramActivator)
  , m_uniformsProvider(uniformsProvider)
  ,

  m_assemblyManager(assemblyManager)
  , m_dataManager(dataManager)
  , m_guiManager(guiManager)
  , m_interactionManager(interactionManager)
  ,

  m_slideStackFrameProvider(nullptr)
  , m_crosshairsFrameProvider(nullptr)
  , m_crosshairsFrameChangedBroadcaster(nullptr)
  , m_crosshairsFrameChangedDoneBroadcaster(nullptr)
{
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
}

ActionManager::~ActionManager() = default;

void ActionManager::setSlideStackFrameProvider(GetterType<CoordinateFrame> provider)
{
  m_slideStackFrameProvider = provider;
}

void ActionManager::setCrosshairsFrameProvider(GetterType<CoordinateFrame> provider)
{
  m_crosshairsFrameProvider = provider;
}

void ActionManager::setCrosshairsFrameChangedBroadcaster(
  SetterType<const CoordinateFrame&> broadcaster
)
{
  m_crosshairsFrameChangedBroadcaster = broadcaster;
}

void ActionManager::setCrosshairsFrameChangeDoneBroadcaster(
  SetterType<const CoordinateFrame&> broadcaster
)
{
  m_crosshairsFrameChangedDoneBroadcaster = broadcaster;
}

/// @todo This should be done by pull when ever update occurs
void ActionManager::updateWorldPositionStatus()
{
  static constexpr uint32_t sk_compIndex = 0;
  static const glm::u64vec3 sk_minIndex(0);

  if (!m_crosshairsFrameProvider)
  {
    return;
  }

  const glm::vec4 worldPos{m_crosshairsFrameProvider().worldOrigin(), 1.0f};

  // Get the position in image Subject space
  auto getImageSubjectPosition = [&worldPos](const imageio::ImageCpuRecord& record) -> glm::vec3
  {
    const glm::vec4 subjectPos = record.transformations().subject_O_world() * worldPos;
    return glm::vec3{subjectPos / subjectPos.w};
  };

  // Get the pixel value at the given position in an image. The value is returned as a
  // double precision floating point. If the world position is not inside the image domain,
  // then std::nullopt is returned.

  auto getImagePixelValue = [&worldPos](const imageio::ImageCpuRecord& record
                            ) -> std::optional<double>
  {
    const glm::vec4 pixelPos4 = record.transformations().pixel_O_world() * worldPos;
    const glm::vec3 pixelPos{pixelPos4 / pixelPos4.w};

    const glm::u64vec3 pixelIndex = glm::round(pixelPos);

    if ( glm::all( glm::greaterThanEqual( pixelIndex, sk_minIndex ) ) &&
             glm::all( glm::lessThan( pixelIndex, record.header().m_pixelDimensions ) ) )
    {
      // Position is inside the image
      double value;
      if (bool retrieved = record.pixelValue(sk_compIndex, pixelIndex, value))
      {
        return value;
      }
    }

    return std::nullopt;
  };

  std::ostringstream ssPosition;
  std::ostringstream ssImageValue;
  std::ostringstream ssLabelValue;

  // By default, show the World-space position
  ssPosition << boost::format("(%.3f, %.3f, %.3f) mm, ") % worldPos.x % worldPos.y % worldPos.z;

  // Initialize streams with defaults:
  ssImageValue << "Image: <N/A>, ";
  ssLabelValue << "Label: <N/A> ";

  do
  {
    auto imageRecord = m_dataManager.activeImageRecord().lock();
    if (!imageRecord)
      break;

    auto imageCpuRecord = imageRecord->cpuData();
    if (!imageCpuRecord)
      break;

    const glm::vec3 subjectPos = getImageSubjectPosition(*imageCpuRecord);

    // If there is an image, then display the Subject-space position instead of the
    // World-space position

    ssPosition.str(std::string());
    ssPosition << boost::format("(%.3f, %.3f, %.3f) mm, ") % subjectPos.x % subjectPos.y
                    % subjectPos.z;

    auto imageValue = getImagePixelValue(*imageCpuRecord);
    if (!imageValue)
      break;

    auto format = (imageio::isIntegerType(imageCpuRecord->header().m_componentType))
                    ? boost::format("Image: %d, ")
                    : boost::format("Image: %.6f, ");

    ssImageValue.str(std::string());
    ssImageValue << format % (*imageValue);

    break;
  } while (1);

  do
  {
    auto parcelUid = m_dataManager.activeParcellationUid();
    if (!parcelUid)
      break;

    auto parcelRecord = m_dataManager.activeParcellationRecord().lock();
    if (!parcelRecord)
      break;

    auto parcelCpuRecord = parcelRecord->cpuData();
    if (!parcelCpuRecord)
      break;

    auto labelTableUid = m_dataManager.labelTableUid_of_parcellation(*parcelUid);
    if (!labelTableUid)
      break;

    auto labelTableRecord = m_dataManager.labelTableRecord(*labelTableUid).lock();
    if (!labelTableRecord)
      break;

    auto labelTableCpuRecord = labelTableRecord->cpuData();
    if (!labelTableCpuRecord)
      break;

    // Parcellation stores the index into a map of label values
    auto labelIndexAsDouble = getImagePixelValue(*parcelCpuRecord);
    if (!labelIndexAsDouble)
      break;

    // Since image pixel values are generically returned as double,
    // we cast here to size_t.
    const size_t labelIndex = static_cast<size_t>(*labelIndexAsDouble);

    const std::string& labelName = labelTableCpuRecord->getName(labelIndex);

    ssLabelValue.str(std::string());

    const std::optional<int64_t> labelValue = parcelCpuRecord->labelValue(labelIndex);
    if (labelValue)
    {
      ssLabelValue << boost::format("Label: %d ('%s')") % (*labelValue) % labelName;
    }

    break;
  } while (1);

  m_guiManager.setWorldPositionStatusText(ssPosition.str());
  m_guiManager.setImageValueStatusText(ssImageValue.str());
  m_guiManager.setLabelValueStatusText(ssLabelValue.str());
}

/// @todo Enable objectID rendering in renderers when in pointer mode
/// @todo Disable objectID rendering in renderers when not in pointer mode

void ActionManager::centerCrosshairsOnImage(const UID& imageUid)
{
  if (!m_crosshairsFrameProvider || !m_crosshairsFrameChangedDoneBroadcaster)
  {
    return;
  }

  auto roundPositionToNearestPixel =
    [](const imageio::ImageCpuRecord& record, const glm::vec3& worldPos)
  {
    const glm::mat4& pixel_O_world = record.transformations().pixel_O_world();
    const glm::mat4& world_O_pixel = record.transformations().world_O_pixel();

    // Convert to position Pixel space and round to nearest integer coordinates
    const glm::vec3 pixelPos{pixel_O_world * glm::vec4{worldPos, 1.0f}};
    const glm::uvec3 index = glm::round(pixelPos);

    // Convert rounded coordinates back to World space
    const glm::vec4 worldPosRounded = world_O_pixel * glm::vec4{index.x, index.y, index.z, 1.0f};
    return glm::vec3{worldPosRounded / worldPosRounded.w};
  };

  auto record = m_dataManager.imageRecord(imageUid).lock();
  if (!record || !record->cpuData())
  {
    return;
  }

  const glm::vec3 bboxCenter{record->cpuData()->header().m_boundingBoxCenter};
  const glm::vec3 bboxCenterRounded = roundPositionToNearestPixel(*record->cpuData(), bboxCenter);

  auto crosshairsFrame = m_crosshairsFrameProvider();
  crosshairsFrame.setIdentity();
  crosshairsFrame.setWorldOrigin(bboxCenterRounded); // need to do update too
  m_crosshairsFrameChangedDoneBroadcaster(crosshairsFrame);
}

void ActionManager::centerCrosshairsOnSlide(const UID& slideUid)
{
  if (!m_slideStackFrameProvider || !m_crosshairsFrameChangedDoneBroadcaster)
  {
    return;
  }

  auto slideRecord = m_dataManager.slideRecord(slideUid).lock();
  if (!slideRecord || !slideRecord->cpuData())
  {
    return;
  }

  // Slide space is unit cube [0,1]^3, so the center is at (0.5, 0.5, 0.5):
  static const glm::vec4 sk_slideBottom(0.5f, 0.5f, 0.0f, 1.0f);
  static const glm::vec4 sk_slideCenter(0.5f, 0.5f, 0.5f, 1.0f);
  static const glm::vec4 sk_slideTop(0.5f, 0.5f, 1.0f, 1.0f);

  glm::vec4 slidePosition;

  if (m_assemblyManager.getSlideRenderingProperties().m_activeSlideViewShows2dSlides)
  {
    // If slides are rendered as 2D, then position the crosshairs in the center
    // of the active slide, so that the 2D intersection of the view plane and the slide
    // looks good.
    slidePosition = sk_slideCenter;
  }
  else
  {
    // If slides are rendered as 3D, then position the crosshairs either at the top
    // or the bottom of the active slide, so that the crosshairs are visible from the
    // viewer's orientation and not embedded within the slide.
    switch (m_interactionManager.getActiveSlideViewDirection())
    {
    case InteractionManager::ActiveSlideViewDirection::TopToBottomSlide:
    {
      slidePosition = sk_slideTop;
      break;
    }
    case InteractionManager::ActiveSlideViewDirection::BottomToTopSlide:
    {
      slidePosition = sk_slideBottom;
      break;
    }
    }
  }

  const auto stackFrame = m_slideStackFrameProvider();

  const glm::mat4 world_O_slide = stackFrame.world_O_frame()
                                  * slideio::stack_O_slide(*(slideRecord->cpuData()));

  const glm::vec4 worldPosition = world_O_slide * slidePosition;

  static constexpr bool sk_alignCrosshairsToStack = false;

  CoordinateFrame frame;
  frame.setWorldOrigin(glm::vec3{worldPosition / worldPosition.w});

  if (sk_alignCrosshairsToStack)
  {
    frame.setFrameToWorldRotation(stackFrame.world_O_frame_rotation());
  }

  m_crosshairsFrameChangedDoneBroadcaster(frame);
}

void ActionManager::alignCrosshairsToActiveSlide()
{
  /// @todo Call this when a slide is activated or when the user clicks on the slide stack views.
  if (const auto slideUid = m_dataManager.activeSlideUid())
  {
    centerCrosshairsOnSlide(*slideUid);
  }
}

void ActionManager::alignCrosshairsToSlideStackFrame()
{
  if (!m_crosshairsFrameProvider || !m_slideStackFrameProvider || !m_crosshairsFrameChangedDoneBroadcaster)
  {
    return;
  }

  CoordinateFrame frame = m_crosshairsFrameProvider();
  frame.setFrameToWorldRotation(m_slideStackFrameProvider().world_O_frame_rotation());
  m_crosshairsFrameChangedDoneBroadcaster(frame);
}

void ActionManager::alignCrosshairsToSubjectXyzPlanes()
{
  if (!m_crosshairsFrameProvider || !m_crosshairsFrameChangedDoneBroadcaster)
  {
    return;
  }

  glm::mat3 world_O_subject_rotation{1.0f};

  // If there is an active image, then use its world_O_subject transformation.
  // Otherwise, use identity.
  auto activeImage = m_dataManager.activeImageRecord().lock();
  if (activeImage && activeImage->cpuData())
  {
    world_O_subject_rotation = glm::mat3{activeImage->cpuData()->transformations().world_O_subject()
    };
  }

  CoordinateFrame anatomicalFrame;
  anatomicalFrame.setWorldOrigin(m_crosshairsFrameProvider().worldOrigin());
  anatomicalFrame.setFrameToWorldRotation(glm::quat_cast(world_O_subject_rotation));

  m_crosshairsFrameChangedDoneBroadcaster(anatomicalFrame);
}

void ActionManager::resetViews()
{
  /// @todo Make clear separation between
  /// 1) alignment to center of image
  /// 2) alignment to center of slide
  /// 3) reset of crosshairs rotation
  /// 4) reset of cameras

  m_interactionManager.resetCameras();

  setupCamerasAndCrosshairsForImage();
}

/// @todo Call when an image is activated
void ActionManager::setupCamerasAndCrosshairsForImage()
{
  if (!m_crosshairsFrameChangedDoneBroadcaster || !m_slideStackFrameProvider)
  {
    return;
  }

  const auto stackFrame = m_slideStackFrameProvider();

  // Set the crosshairs position and rotation
  if (const auto imageUid = m_dataManager.activeImageUid())
  {
    // There is an active image, so center on it:
    centerCrosshairsOnImage(*imageUid);
  }
  else
  {
    // There is no active image, so position the crosshairs at the center of the reference space
    // and align them to the X, Y, Z World axes (identity rotation):

    const glm::vec3 center = math::computeAABBoxCenter(
      data::refSpaceAABBox(m_dataManager, stackFrame.world_O_frame())
    );

    CoordinateFrame frame;
    frame.setWorldOrigin(center);
    m_crosshairsFrameChangedDoneBroadcaster(frame);
  }

  m_interactionManager.setupCamerasForAABBox(
    data::refSpaceAABBox(m_dataManager, stackFrame.world_O_frame()),
    data::refSpaceVoxelScale(m_dataManager)
  );

  //    m_interactionManager.setCameraNearDistance( data::refSpaceVoxelScale( m_dataManager ) );
  m_interactionManager.alignCamerasToFrames();

  m_guiManager.updateAllViewWidgets();
}

std::optional<UID> ActionManager::loadImage(
  const std::string& filename, const std::optional<std::string>& dicomSeriesUid
)
{
  std::optional<UID> imageUid;

  if (m_globalContext->makeCurrent(&m_surface))
  {
    // Loads the image and makes it active
    imageUid = data::loadImage(m_dataManager, filename, dicomSeriesUid);

    if (imageUid)
    {
      // Update the assemblies
      updateImageSliceAssembly();
      m_guiManager.updateAllViewWidgets();
    }
    else
    {
      std::cerr << "No image loaded from " << filename << std::endl;
    }

    m_globalContext->doneCurrent();
  }
  else
  {
    throw_debug(sk_glContextErrorMsg)
  }

  return imageUid;
}

std::optional<UID> ActionManager::loadParcellation(
  const std::string& filename, const std::optional<std::string>& dicomSeriesUid
)
{
  std::optional<UID> parcelUid;

  if (m_globalContext->makeCurrent(&m_surface))
  {
    parcelUid = data::loadParcellation(m_dataManager, filename, dicomSeriesUid);

    if (parcelUid)
    {
      // Update the assemblies
      updateImageSliceAssembly();
      m_guiManager.updateAllViewWidgets();
    }
    else
    {
      std::cerr << "No parcellation loaded from " << filename << std::endl;
    }

    m_globalContext->doneCurrent();
  }
  else
  {
    throw_debug(sk_glContextErrorMsg)
  }

  return parcelUid;
}

std::optional<UID> ActionManager::loadSlide(const std::string& filename, bool translateToTopOfStack)
{
  std::optional<UID> slideUid;

  if (m_globalContext->makeCurrent(&m_surface))
  {
    slideUid = data::loadSlide(m_dataManager, filename, translateToTopOfStack);

    if (slideUid)
    {
      std::cout << "Loaded slide " << *slideUid << std::endl;

      updateSlideStackAssembly();
      m_guiManager.updateAllViewWidgets();
    }
    else
    {
      std::cerr << "No slide image loaded from " << filename << std::endl;
    }

    m_globalContext->doneCurrent();
  }
  else
  {
    throw_debug(sk_glContextErrorMsg)
  }

  return slideUid;
}

void ActionManager::saveProject(const std::optional<std::string>& newFileName)
{
  // Update image and slide data in project:
  m_dataManager.updateProject(newFileName);

  // Update stack transformation:
  if (m_slideStackFrameProvider)
  {
    m_dataManager.project().m_world_T_slideStack = m_slideStackFrameProvider();
  }

  serialize::save(m_dataManager.project(), newFileName);
}

void ActionManager::generateIsoSurfaceMesh(double isoValue)
{
  if (m_globalContext->makeCurrent(&m_surface))
  {
    const auto activeImageUid = m_dataManager.activeImageUid();

    if (!activeImageUid)
    {
      std::cerr << "No active image for which to generate isosurface" << std::endl;
      return;
    }

    const auto meshUid = data::generateIsoSurfaceMesh(m_dataManager, *activeImageUid, isoValue);

    if (meshUid)
    {
      // Update the assemblies
      updateIsoMeshAssembly();
      m_guiManager.updateAllViewWidgets();
    }
    else
    {
      std::cerr << "No isosurface mesh generated from image " << *activeImageUid << std::endl;
    }

    m_globalContext->doneCurrent();
  }
  else
  {
    throw_debug(sk_glContextErrorMsg)
  }
}

void ActionManager::generateLabelMeshes()
{
  if (m_globalContext->makeCurrent(&m_surface))
  {
    const auto parcelUid = m_dataManager.activeParcellationUid();
    if (!parcelUid)
    {
      std::cerr << "No active parcellation for which to generate label meshes" << std::endl;
      return;
    }

    const auto generatedUids = data::generateAllLabelMeshes(m_dataManager, *parcelUid);

    if (!generatedUids.empty())
    {
      updateLabelMeshAssembly();
      m_guiManager.updateAllViewWidgets();
    }
    else
    {
      std::cerr << "No meshes generated from parcellation " << *parcelUid << std::endl;
    }

    m_globalContext->doneCurrent();
  }
  else
  {
    throw_debug(sk_glContextErrorMsg)
  }
}

void ActionManager::transformFeedback()
{
  QOpenGLWidget computerWidget;

  computerWidget.show();
  computerWidget.hide();

  if (!computerWidget.isValid())
  {
    throw_debug(sk_glContextErrorMsg)
  }

  computerWidget.makeCurrent();
  {
    Polygonizer polygonizer(m_shaderProgramActivator, m_uniformsProvider);

    auto activeImageWR = m_dataManager.activeImageRecord();
    auto record = activeImageWR.lock();

    if (record && record->gpuData())
    {
      polygonizer.setVolumeTexture(record->gpuData()->texture());
    }
    else
    {
      std::cerr << "No active image for which to generate isosurface" << std::endl;
      return;
    }

    std::cout << "START EXECUTION!!!" << std::endl;
    polygonizer.setIsoValue(250.0f);
    polygonizer.execute();

    polygonizer.setIsoValue(251.0f);
    polygonizer.execute();

    polygonizer.setIsoValue(252.0f);
    polygonizer.execute();

    polygonizer.setIsoValue(253.0f);
    polygonizer.execute();

    polygonizer.setIsoValue(254.0f);
    polygonizer.execute();

    polygonizer.setIsoValue(255.0f);
    polygonizer.execute();
    std::cout << "END EXECUTION!!!" << std::endl;

    computerWidget.doneCurrent();
  }
}

void ActionManager::updateImageSliceAssembly()
{
  const auto activeImageUid = m_dataManager.activeImageUid();
  if (!activeImageUid)
  {
    std::ostringstream ss;
    ss << "No active image to display" << std::ends;
    std::cerr << ss.str() << std::endl;
    return;
  }

  const auto activeParcelUid = data::getActiveParcellation(m_dataManager, *activeImageUid);
  if (!activeParcelUid)
  {
    std::ostringstream ss;
    ss << "No parcellation to display" << std::ends;
    std::cerr << ss.str() << std::endl;
    return;
  }

  const auto imageColorMapUid = m_dataManager.imageColorMapUid_of_image(*activeImageUid);
  if (!imageColorMapUid)
  {
    std::ostringstream ss;
    ss << "No color map found for image " << *activeImageUid << std::ends;
    std::cerr << ss.str() << std::endl;
  }

  const auto labelsUid = m_dataManager.labelTableUid_of_parcellation(*activeParcelUid);
  if (!labelsUid)
  {
    std::ostringstream ss;
    ss << "No color table found for parcellation " << *activeParcelUid << std::ends;
    std::cerr << ss.str() << std::endl;
  }

  if (m_globalContext->makeCurrent(&m_surface))
  {
    m_assemblyManager.updateImages(*activeImageUid, *activeParcelUid, *imageColorMapUid, *labelsUid);

    m_globalContext->doneCurrent();
  }
  else
  {
    throw_debug(sk_glContextErrorMsg)
  }
}

void ActionManager::updateIsoMeshAssembly()
{
  if (m_globalContext->makeCurrent(&m_surface))
  {
    const auto activeImageUid = m_dataManager.activeImageUid();
    if (!activeImageUid)
    {
      std::ostringstream ss;
      ss << "No active image to update" << std::ends;
      std::cerr << ss.str() << std::endl;
      return;
    }

    m_assemblyManager.updateIsoSurfaceMeshes(m_dataManager.isoMeshUids_of_image(*activeImageUid));

    m_globalContext->doneCurrent();
  }
  else
  {
    throw_debug(sk_glContextErrorMsg)
  }
}

void ActionManager::updateLabelMeshAssembly()
{
  if (m_globalContext->makeCurrent(&m_surface))
  {
    const auto activeImageUid = m_dataManager.activeImageUid();
    if (!activeImageUid)
    {
      std::ostringstream ss;
      ss << "No active image to update" << std::ends;
      std::cerr << ss.str() << std::endl;
      return;
    }

    const auto activeParcelUid = data::getActiveParcellation(m_dataManager, *activeImageUid);
    if (!activeParcelUid)
    {
      std::ostringstream ss;
      ss << "No parcellation to update" << std::ends;
      std::cerr << ss.str() << std::endl;
      return;
    }

    const auto labelsUid = m_dataManager.labelTableUid_of_parcellation(*activeParcelUid);
    if (!labelsUid)
    {
      std::ostringstream ss;
      ss << "No label table found for parcellation " << *activeParcelUid << std::ends;
      std::cerr << ss.str() << std::endl;
      return;
    }

    // Get map of label index to mesh UID
    std::map<uint32_t, UID> labelMeshUidMap = m_dataManager.labelMeshUids_of_parcellation(
      *activeParcelUid
    );

    std::vector<UID> labelMeshUids;
    for (const auto& p : labelMeshUidMap)
    {
      labelMeshUids.push_back(p.second);
    }

    m_assemblyManager.updateLabelMeshes(labelMeshUids, *labelsUid);

    m_globalContext->doneCurrent();
  }
  else
  {
    throw_debug(sk_glContextErrorMsg)
  }
}

void ActionManager::updateSlideStackAssembly()
{
  if (m_globalContext->makeCurrent(&m_surface))
  {
    if (m_slideStackFrameProvider)
    {
      m_assemblyManager.updateSlideStack(m_dataManager.orderedSlideUids());
    }

    m_globalContext->doneCurrent();
  }
  else
  {
    throw_debug(sk_glContextErrorMsg)
  }
}

void ActionManager::updateLandmarkAssemblies()
{
  const auto activeImageUid = m_dataManager.activeImageUid();
  if (!activeImageUid)
  {
    return;
  }

  if (m_globalContext->makeCurrent(&m_surface))
  {
    m_assemblyManager.updateRefImageLandmarkGroups(*activeImageUid);
    m_assemblyManager.updateSlideLandmarkGroups(m_dataManager.orderedSlideUids());

    m_globalContext->doneCurrent();
  }
  else
  {
    throw_debug(sk_glContextErrorMsg)
  }
}

void ActionManager::updateAnnotationAssemblies()
{
  if (m_globalContext->makeCurrent(&m_surface))
  {
    m_assemblyManager.updateSlideAnnotations(m_dataManager.orderedSlideUids());

    m_globalContext->doneCurrent();
  }
  else
  {
    throw_debug(sk_glContextErrorMsg)
  }
}

void ActionManager::updateAllAssemblies()
{
  updateImageSliceAssembly();
  updateIsoMeshAssembly();
  updateLabelMeshAssembly();
  updateSlideStackAssembly();
  updateLandmarkAssemblies();
  updateAnnotationAssemblies();
}

void ActionManager::updateAllViews()
{
  m_guiManager.updateAllViewWidgets();
}
