#define GLM_ENABLE_EXPERIMENTAL
#include "ui/ImGuiWrapper.h"

#include "common/MathFuncs.h"
#include "image/ImageUtility.h"
#include "logic/annotation/PointRecord.h"
#include "logic/annotation/SerializeAnnot.h"
#include "logic/app/Data.h"
#include "logic/serialization/ProjectSerialization.h"
#include "logic/states/annotation/AnnotationStateMachine.h"
#include "ui/Helpers.h"
#include "ui/ImageExport.h"
#include "ui/NativeFileDialogs.h"
#include "ui/dialogs/InputLoadErrorDialog.h"
#include "ui/dialogs/NativeMessageDialogs.h"

#include <glm/gtx/color_space.hpp>

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <functional>
#include <utility>

void ImGuiWrapper::createActiveSegmentation()
{
  const auto imageUid = activeImageUid();
  const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
  if (!imageUid || !image || !m_createBlankSeg) {
    return;
  }

  const std::size_t numSegsForImage = m_appData.imageToSegUids(*imageUid).size();
  const std::string displayName = "Untitled segmentation " + std::to_string(numSegsForImage + 1) + " for image '" +
                                  image->settings().displayName() + "'";
  if (m_createBlankSeg(*imageUid, displayName) && m_updateImageUniforms) {
    m_updateImageUniforms(*imageUid);
  }
}

void ImGuiWrapper::exportActiveSegmentation()
{
  const auto segUid = activeSegUid();
  if (segUid) {
    image_export::exportSegmentation(m_appData, *segUid);
  }
}

void ImGuiWrapper::importAnnotationsToActiveImage()
{
  const auto imageUid = activeImageUid();
  if (!imageUid) {
    return;
  }

  if (const auto selectedFile = native_dialog::openFile(native_dialog::annotationFilters())) {
    std::vector<Annotation> annotations;
    if (!serialize::openAnnotationsFromJsonFile(annotations, *selectedFile)) {
      spdlog::error("Error importing annotations from JSON file {}", *selectedFile);
      native_dialog::showInputLoadErrorDialog(
        {.inputType = "annotations",
         .path = selectedFile,
         .cause = "The annotation JSON file could not be read or parsed."});
      return;
    }

    std::size_t numImportedAnnotations = 0;
    for (auto& annotation : annotations) {
      annotation.setFileName(*selectedFile);
      if (const auto annotationUid = m_appData.addAnnotation(*imageUid, annotation)) {
        m_appData.assignActiveAnnotationUidToImage(*imageUid, annotationUid);
        ++numImportedAnnotations;
      }
    }
    ASM::synchronizeAnnotationHighlights();
    spdlog::info("Imported {} annotations from JSON file {}", numImportedAnnotations, *selectedFile);
  }
}

bool ImGuiWrapper::activeImageHasAnnotations()
{
  const auto imageUid = activeImageUid();
  return imageUid && !m_appData.annotationsForImage(*imageUid).empty();
}

void ImGuiWrapper::exportAnnotationsForActiveImage()
{
  const auto imageUid = activeImageUid();
  if (!imageUid || !activeImageHasAnnotations()) {
    return;
  }

  if (const auto selectedFile = native_dialog::saveFile(native_dialog::annotationFilters())) {
    std::vector<Annotation> annotations;
    std::size_t numExportedAnnotations = 0;
    for (const auto& annotUid : m_appData.annotationsForImage(*imageUid)) {
      if (const Annotation* annot = m_appData.annotation(annotUid)) {
        annotations.push_back(*annot);
        ++numExportedAnnotations;
      }
    }

    const nlohmann::json annotationsJson = annotationsToJson(annotations);

    if (serialize::saveToJsonFile(annotationsJson, *selectedFile)) {
      spdlog::info(
        "Exported {} annotations for image {} to JSON file {}",
        numExportedAnnotations,
        *imageUid,
        *selectedFile);
      for (const auto& annotUid : m_appData.annotationsForImage(*imageUid)) {
        if (Annotation* annot = m_appData.annotation(annotUid)) {
          annot->setFileName(*selectedFile);
          annot->markClean();
        }
      }
    }
    else {
      spdlog::error("Error exporting annotations to JSON file {}", *selectedFile);
    }
  }
}

void ImGuiWrapper::createActiveLandmarkGroup()
{
  const auto imageUid = activeImageUid();
  const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
  if (!imageUid || !image) {
    return;
  }

  LandmarkGroup newGroup;
  newGroup.setName("Landmarks for " + image->settings().displayName());
  const auto landmarkGroupUid = m_appData.addLandmarkGroup(newGroup);
  m_appData.assignLandmarkGroupUidToImage(*imageUid, landmarkGroupUid);
  m_appData.setRainbowColorsForAllLandmarkGroups();
  m_appData.assignActiveLandmarkGroupUidToImage(*imageUid, landmarkGroupUid);
}

void ImGuiWrapper::saveActiveLandmarkGroup()
{
  const auto landmarkGroupUid = activeLandmarkGroupUid();
  LandmarkGroup* landmarkGroup = landmarkGroupUid ? m_appData.landmarkGroup(*landmarkGroupUid) : nullptr;
  const auto imageUid = activeImageUid();
  const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
  if (!landmarkGroup || !image) {
    return;
  }

  if (const auto selectedFile = native_dialog::saveFile(native_dialog::landmarkFilters())) {
    if (serialize::saveLandmarkGroupCsvFile(
          landmarkGroup->pointsInSubjectSpace(image->transformations().subject_T_pixel()),
          *selectedFile))
    {
      spdlog::info("Saved landmarks to CSV file {}", *selectedFile);
      landmarkGroup->setFileName(*selectedFile);
    }
    else {
      spdlog::error("Error saving landmarks to CSV file {}", *selectedFile);
    }
  }
}

void ImGuiWrapper::importLandmarkGroupForActiveImage()
{
  const std::optional<uuids::uuid> imageUid = activeImageUid();
  const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
  if (!image) {
    return;
  }

  const std::optional<std::filesystem::path> selectedFile = native_dialog::openFile(native_dialog::landmarkFilters());
  if (!selectedFile) {
    return;
  }

  std::map<size_t, PointRecord<glm::vec3>> landmarks;
  if (!serialize::openLandmarkGroupCsvFile(landmarks, *selectedFile)) {
    spdlog::error("Error importing landmarks from CSV file {}", *selectedFile);
    native_dialog::showInputLoadErrorDialog(
      {.inputType = "landmarks", .path = selectedFile, .cause = "The landmark CSV file could not be read or parsed."});
    return;
  }

  LandmarkGroup landmarkGroup;
  landmarkGroup.setFileName(*selectedFile);
  landmarkGroup.setName(getFileName(selectedFile->string(), false));
  landmarkGroup.setPoints(std::move(landmarks));
  landmarkGroup.setInVoxelSpace(false);
  landmarkGroup.setRenderLandmarkNames(false);

  const uuids::uuid landmarkGroupUid = m_appData.addLandmarkGroup(landmarkGroup);
  m_appData.assignLandmarkGroupUidToImage(*imageUid, landmarkGroupUid);
  m_appData.setRainbowColorsForAllLandmarkGroups();
  m_appData.assignActiveLandmarkGroupUidToImage(*imageUid, landmarkGroupUid);
  spdlog::info("Imported landmarks from CSV file {} for image {}", *selectedFile, *imageUid);
}

void ImGuiWrapper::removeActiveLandmarkGroup()
{
  const std::optional<uuids::uuid> landmarkGroupUid = activeLandmarkGroupUid();
  if (!landmarkGroupUid) {
    return;
  }

  if (m_appData.removeLandmarkGroup(*landmarkGroupUid)) {
    m_appData.setRainbowColorsForAllLandmarkGroups();
    spdlog::info("Removed landmark group {}", *landmarkGroupUid);
  }
}

void ImGuiWrapper::addLandmarkAtCrosshairs()
{
  const auto imageUid = activeImageUid();
  const auto landmarkGroupUid = activeLandmarkGroupUid();
  const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
  LandmarkGroup* landmarkGroup = landmarkGroupUid ? m_appData.landmarkGroup(*landmarkGroupUid) : nullptr;
  if (!image || !landmarkGroup) {
    return;
  }

  const glm::mat4 landmark_T_world = landmarkGroup->getInVoxelSpace() ? image->transformations().pixel_T_worldDef()
                                                                      : image->transformations().subject_T_worldDef();
  const glm::vec4 landmarkPosition =
    landmark_T_world * glm::vec4{m_appData.state().worldCrosshairs().worldOrigin(), 1.0f};

  PointRecord<glm::vec3> point{glm::vec3{landmarkPosition / landmarkPosition.w}};
  const size_t newIndex = landmarkGroup->getPoints().empty() ? 0u : landmarkGroup->maxIndex() + 1;
  const auto colors = math::generateRandomHsvSamples(
    1,
    std::make_pair(0.0f, 360.0f),
    std::make_pair(0.3f, 1.0f),
    std::make_pair(0.3f, 1.0f),
    static_cast<uint32_t>(newIndex));
  if (!colors.empty()) {
    point.setColor(glm::rgbColor(colors.front()));
  }

  landmarkGroup->addPoint(newIndex, point);
}

void ImGuiWrapper::requestResetProjectSettings()
{
  if (!m_resetProjectSettings) {
    return;
  }

  const auto result = native_dialog::showMessageDialog(
    {"Reset project settings?",
     "Reset project display and review settings to defaults?",
     "This resets project-wide view, comparison, raycasting, projection, segmentation display, isosurface display, "
     "annotation display, and time-series synchronization settings. Loaded images, segmentations, landmarks, "
     "annotations, layouts, affine transformations, per-image settings, and deformation warp assignments are not "
     "removed or reset.",
     "Reset Project Settings",
     "Cancel",
     ""});

  if (result && native_dialog::MessageDialogResult::FirstButton == *result) {
    m_resetProjectSettings();
  }
}
