#include "logic/app/ProjectSnapshot.h"
#include "logic/app/Data.h"

#include "common/UuidUtility.h"
#include "logic/app/ProjectLayoutDelta.h"
#include "logic/app/ProjectSnapshotSettings.h"
#include "logic/app/DeformationArchive.h"
#include "logic/annotation/Annotation.h"
#include "logic/annotation/LandmarkGroup.h"
#include "logic/annotation/SerializeAnnot.h"
#include "logic/serialization/ProjectSerialization.h"
#include "mesh/MeshTypes.h"
#include "registration/Artifacts.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
bool isApproximatelyIdentity(const glm::mat4& matrix)
{
  constexpr float epsilon = 1.0e-5f;
  const glm::mat4 identity{1.0f};

  for (int c = 0; c < 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      if (std::abs(matrix[c][r] - identity[c][r]) > epsilon) {
        return false;
      }
    }
  }

  return true;
}

glm::vec3 defaultImageBorderColor(const std::size_t imageIndex, const std::size_t numImages)
{
  if (0u == numImages) {
    return glm::vec3{1.0f, 0.0f, 1.0f};
  }

  static constexpr float k_colorSat = 0.65f;
  static constexpr float k_colorVal = 0.90f;
  static constexpr float k_startHue = -1.0f / 48.0f;

  const float normalizedHue =
    std::fmod(1.0f + k_startHue + static_cast<float>(imageIndex) / static_cast<float>(numImages), 1.0f);
  return glm::rgbColor(glm::vec3{360.0f * normalizedHue, k_colorSat, k_colorVal});
}

serialize::RegistrationResult registrationResultSnapshot(const registration::JobRecord& job)
{
  serialize::RegistrationResult result;
  result.m_backend = std::string{registration::label(job.spec.backend)};
  if (!job.spec.fixedImage.fileName.empty()) {
    result.m_fixedImage = job.spec.fixedImage.fileName;
  }
  if (!job.spec.movingImage.fileName.empty()) {
    result.m_movingImage = job.spec.movingImage.fileName;
  }

  if (!job.manifest) {
    return result;
  }

  const registration::ResultManifest& manifest = *job.manifest;
  result.m_manifestFileName = registration::artifactPath(job.spec, registration::ArtifactRole::ResultManifest);
  if (!manifest.warpedImage.empty()) {
    result.m_warpedImage = manifest.warpedImage;
  }
  if (!manifest.inverseWarp.empty()) {
    result.m_inverseWarpField = manifest.inverseWarp;
  }
  if (!manifest.forwardWarp.empty()) {
    result.m_forwardWarpField = manifest.forwardWarp;
  }
  if (!manifest.affineTransform.empty()) {
    result.m_affineTransform = manifest.affineTransform;
  }
  result.m_warpedSegmentations = manifest.warpedSegmentations;
  result.m_transformedSurfaces = manifest.transformedSurfaces;
  result.m_transformedLandmarks = manifest.transformedLandmarks;
  result.m_warnings = manifest.warnings;

  return result;
}

std::string registrationResultKey(const serialize::RegistrationResult& result)
{
  const std::string manifest = result.m_manifestFileName ? result.m_manifestFileName->string() : std::string{};
  const std::string fixedImage = result.m_fixedImage ? result.m_fixedImage->string() : std::string{};
  const std::string movingImage = result.m_movingImage ? result.m_movingImage->string() : std::string{};
  return result.m_backend + '\n' + fixedImage + '\n' + movingImage + '\n' + manifest;
}

std::vector<serialize::RegistrationResult> registrationResultSnapshots(const AppData& data)
{
  std::vector<serialize::RegistrationResult> results = data.project().m_registrationResults;
  std::set<std::string> keys;
  for (const serialize::RegistrationResult& result : results) {
    keys.insert(registrationResultKey(result));
  }

  for (const registration::JobRecord& job : data.registrationJobs().jobs()) {
    if (job.status != registration::JobStatus::Completed || !job.manifest) {
      continue;
    }

    serialize::RegistrationResult result = registrationResultSnapshot(job);
    if (keys.insert(registrationResultKey(result)).second) {
      results.push_back(std::move(result));
    }
  }

  return results;
}
} // namespace

std::map<std::string, deformation::EditHistory> project_snapshot::restoreDeformationHistories(
  const serialize::EntropyProject& project,
  std::size_t maxTotalFieldBytes)
{
  std::map<std::string, deformation::EditHistory> restored;
  for (const auto& reference : project.m_deformationEdits) {
    if (restored.contains(reference.m_editId)) throw std::invalid_argument("Duplicate deformation edit ID");
    auto history = deformation_archive::loadBundle(reference, maxTotalFieldBytes);
    const auto& maps = history.current().maps();
    const auto forwardSamples = maps.sourceDomain().sampleCount();
    const auto inverseSamples = maps.outputDomain().sampleCount();
    if (forwardSamples > std::numeric_limits<std::size_t>::max() - inverseSamples) throw std::bad_alloc();
    const auto pairSamples = forwardSamples + inverseSamples;
    if (pairSamples > maxTotalFieldBytes / sizeof(glm::vec4) / history.size()) throw std::bad_alloc();
    maxTotalFieldBytes -= pairSamples * sizeof(glm::vec4) * history.size();
    restored.emplace(reference.m_editId, std::move(history));
  }
  return restored;
}

serialize::EntropyProject
project_snapshot::captureProject(const AppData& data, const DicomSources& dicomSources, const NativeViews& nativeViews)
{
  serialize::EntropyProject project;
  const auto imageUids = data.imageUidsOrdered();

  if (imageUids.empty()) {
    return project;
  }

  const uuids::uuid referenceImageUid = data.refImageUid().value_or(imageUids.front());
  const auto defaultColorForImage = [&imageUids](const uuids::uuid& imageUid) {
    const auto imageIt = std::find(imageUids.begin(), imageUids.end(), imageUid);
    const std::size_t imageIndex =
      imageIt == imageUids.end() ? 0u : static_cast<std::size_t>(std::distance(imageUids.begin(), imageIt));
    return defaultImageBorderColor(imageIndex, imageUids.size());
  };

  project.m_referenceImage =
    captureImage(data, dicomSources, referenceImageUid, defaultColorForImage(referenceImageUid));

  for (const auto& imageUid : imageUids) {
    if (imageUid == referenceImageUid) {
      continue;
    }

    project.m_additionalImages.emplace_back(captureImage(data, dicomSources, imageUid, defaultColorForImage(imageUid)));
  }

  const auto defaultProjectLayouts = data.windowData().createDefaultProjectLayoutSnapshots(data, nativeViews);
  const std::size_t defaultProjectLayoutIndex = data.windowData().defaultProjectLayoutIndex(data, nativeViews);
  const auto currentProjectLayouts = data.windowData().createProjectLayoutSnapshots(imageUids);

  if (const auto layoutDelta = project_layout_delta::compactLayoutDelta(currentProjectLayouts, defaultProjectLayouts)) {
    project.m_layouts = layoutDelta->m_addedLayouts;
    project.m_removedDefaultLayoutIndices = layoutDelta->m_removedDefaultLayoutIndices;
    project.m_modifiedDefaultLayouts = layoutDelta->m_modifiedDefaultLayouts;
    if (
      !project.m_layouts.empty() || !project.m_removedDefaultLayoutIndices.empty() ||
      !project.m_modifiedDefaultLayouts.empty() || data.windowData().currentLayoutIndex() != defaultProjectLayoutIndex)
    {
      project.m_currentLayoutIndex = data.windowData().currentLayoutIndex();
    }
  }
  else if (currentProjectLayouts != defaultProjectLayouts) {
    project.m_layouts = currentProjectLayouts;
    project.m_currentLayoutIndex = data.windowData().currentLayoutIndex();
  }
  project.m_synchronization = project_snapshot::synchronizationSettings(data);
  project.m_view = project_snapshot::viewSettings(data);
  project.m_comparison = project_snapshot::comparisonSettings(data);
  project.m_threeDRendering = project_snapshot::threeDRenderingSettings(data);
  project.m_raycasting = project_snapshot::raycastingSettings(data);
  project.m_meshRendering = project_snapshot::meshRenderingSettings(data);
  project.m_intensityProjection = project_snapshot::intensityProjectionSettings(data);
  project.m_segmentationDisplay = project_snapshot::segmentationDisplaySettings(data);
  project.m_isocontours = project_snapshot::isocontourDisplaySettings(data);
  project.m_deformationEdits = data.project().m_deformationEdits;
  project.m_registrationResults = registrationResultSnapshots(data);

  return project;
}

serialize::Image project_snapshot::captureImage(
  const AppData& data,
  const DicomSources& dicomSources,
  const uuids::uuid& imageUid,
  const std::optional<glm::vec3>& defaultBorderColor)
{
  serialize::Image serializedImage;
  const Image* image = data.image(imageUid);

  if (!image) {
    spdlog::warn("Cannot serialize missing image {}", imageUid);
    return serializedImage;
  }

  serializedImage.m_imageFileName = image->header().fileName();
  serializedImage.m_spatialMetadata = image->header().userSpatialMetadata();
  const auto& overrides = image->header().getHeaderOverrides();
  serializedImage.m_useIdentityPixelSpacings = overrides.m_useIdentityPixelSpacings;
  serializedImage.m_useZeroPixelOrigin = overrides.m_useZeroPixelOrigin;
  serializedImage.m_useIdentityPixelDirections = overrides.m_useIdentityPixelDirections;
  serializedImage.m_snapToClosestOrthogonalPixelDirections = overrides.m_snapToClosestOrthogonalPixelDirections;
  if (const auto sourceIt = dicomSources.find(imageUid); sourceIt != dicomSources.end()) {
    serializedImage.m_dicomSource = sourceIt->second;
  }
  const auto& transformations = image->transformations();
  serializedImage.m_initialAffineEnabled = transformations.get_enable_affine_T_subject();
  serializedImage.m_manualAffineEnabled = transformations.get_enable_worldDef_T_affine();
  if (
    transformations.get_affine_T_subject_fileName() ||
    !isApproximatelyIdentity(transformations.stored_affine_T_subject()))
  {
    serializedImage.m_initialAffineMatrix = transformations.stored_affine_T_subject();
  }

  if (!isApproximatelyIdentity(transformations.stored_worldDef_T_affine())) {
    serializedImage.m_manualAffineMatrix = transformations.stored_worldDef_T_affine();
  }
  serializedImage.m_settings = project_snapshot::imageSettings(*image, defaultBorderColor);

  const auto defUids = data.imageToDefUids(imageUid);
  const auto activeInverseWarpUid = data.imageToActiveInverseWarpUid(imageUid);
  const auto activeForwardWarpUid = data.imageToActiveForwardWarpUid(imageUid);
  for (const auto& defUid : defUids) {
    const Image* warp = data.warpField(defUid);
    if (!warp || !warp->header().existsOnDisk() || warp->header().fileName().empty()) {
      spdlog::warn("Cannot serialize missing or pathless warp field {} for image {}", defUid, imageUid);
      continue;
    }

    serialize::ImageWarpField serializedWarp{
      .m_path = warp->header().fileName(),
      .m_activeInverse = activeInverseWarpUid && *activeInverseWarpUid == defUid,
      .m_activeForward = activeForwardWarpUid && *activeForwardWarpUid == defUid};
    if (serializedWarp.m_activeInverse) {
      if (const auto referenceUid = data.imageToActiveInverseWarpReferenceImageUid(imageUid)) {
        const Image* referenceImage = data.image(*referenceUid);
        if (referenceImage && referenceImage->header().existsOnDisk() && !referenceImage->header().fileName().empty()) {
          serializedWarp.m_inverseReferenceImagePath = referenceImage->header().fileName();
        }
      }
    }
    serializedImage.m_warpFields.push_back(std::move(serializedWarp));
  }

  for (const auto& segUid : data.imageToSegUids(imageUid)) {
    const Image* seg = data.seg(segUid);
    if (!seg) {
      spdlog::warn("Cannot serialize missing segmentation {} for image {}", segUid, imageUid);
      continue;
    }

    const auto segmentationPath = data.segmentationPersistencePath(segUid);
    if (!segmentationPath) {
      spdlog::debug("Skipping untouched in-memory segmentation {} for image {}", segUid, imageUid);
      continue;
    }

    serialize::Segmentation serializedSeg;
    serializedSeg.m_active = data.imageToActiveSegUid(imageUid) == segUid;
    serializedSeg.m_segFileName = *segmentationPath;
    serializedSeg.m_settings = project_snapshot::segmentationSettings(data, *seg);
    serializedImage.m_segmentations.emplace_back(std::move(serializedSeg));
  }

  for (const auto& lmUid : data.imageToLandmarkGroupUids(imageUid)) {
    const LandmarkGroup* lmGroup = data.landmarkGroup(lmUid);
    if (!lmGroup) {
      spdlog::warn("Cannot serialize missing landmark group {} for image {}", lmUid, imageUid);
      continue;
    }

    serialize::LandmarkGroup serializedLandmarks;
    serializedLandmarks.m_active = data.imageToActiveLandmarkGroupUid(imageUid) == lmUid;
    if (!lmGroup->getFileName().empty()) {
      serializedLandmarks.m_csvFileName = lmGroup->getFileName();
    }
    serializedLandmarks.m_coordinateSpace = lmGroup->getInVoxelSpace()
                                              ? serialize::ProjectLandmarkCoordinateSpace::Voxel
                                              : serialize::ProjectLandmarkCoordinateSpace::Subject;
    serializedLandmarks.m_name = lmGroup->getName();
    serializedLandmarks.m_pointsEmbedded = true;
    serializedLandmarks.m_visible = lmGroup->getVisibility();
    serializedLandmarks.m_opacity = lmGroup->getOpacity();
    serializedLandmarks.m_color = lmGroup->getColor();
    serializedLandmarks.m_colorOverride = lmGroup->getColorOverride();
    serializedLandmarks.m_textColor = lmGroup->getTextColor();
    serializedLandmarks.m_renderLandmarkIndices = lmGroup->getRenderLandmarkIndices();
    serializedLandmarks.m_renderLandmarkNames = lmGroup->getRenderLandmarkNames();
    serializedLandmarks.m_glyphRadiusFactor = lmGroup->getRadiusFactor();
    for (const auto& [index, point] : lmGroup->getPoints()) {
      serializedLandmarks.m_points.push_back(serialize::LandmarkPoint{
        .m_index = index,
        .m_position = point.getPosition(),
        .m_name = point.getName(),
        .m_description = point.getDescription(),
        .m_visible = point.getVisibility(),
        .m_color = point.getColor()});
    }
    serializedImage.m_landmarkGroups.emplace_back(std::move(serializedLandmarks));
  }

  for (const auto& annotationUid : data.annotationsForImage(imageUid)) {
    const Annotation* annotation = data.annotation(annotationUid);
    if (!annotation) {
      spdlog::warn("Cannot serialize missing annotation {} for image {}", annotationUid, imageUid);
      continue;
    }

    serializedImage.m_annotations.push_back(*annotation);
  }
  serializedImage.m_annotationsFileName = commonAnnotationFileName(serializedImage.m_annotations);
  if (
    !serializedImage.m_annotationsFileName &&
    std::any_of(serializedImage.m_annotations.begin(), serializedImage.m_annotations.end(), [](const Annotation& a) {
      return !a.getFileName().empty();
    }))
  {
    spdlog::warn(
      "Image {} has annotations from different files or without files; omitting ambiguous project path",
      imageUid);
  }

  for (uint32_t component = 0; component < image->header().numComponentsPerPixel(); ++component) {
    for (const auto& isosurfaceUid : data.isosurfaceUids(imageUid, component)) {
      const Isosurface* surface = data.isosurface(imageUid, component, isosurfaceUid);
      if (!surface) {
        spdlog::warn("Cannot serialize missing isosurface {} for image {}", isosurfaceUid, imageUid);
        continue;
      }

      serialize::ImageIsosurface serializedSurface;
      serializedSurface.m_component = component;
      serializedSurface.m_surface = *surface;
      serializedImage.m_isosurfaces.emplace_back(std::move(serializedSurface));
    }
  }

  for (const auto& meshUid : data.imageToImportedMeshUids(imageUid)) {
    const mesh::MeshRecord* imported = data.importedMesh(meshUid);
    if (!imported || imported->sourcePath.empty()) {
      spdlog::warn("Cannot serialize missing or pathless imported mesh {} for image {}", meshUid, imageUid);
      continue;
    }
    serializedImage.m_importedMeshes.push_back(serialize::ImportedMesh{
      .m_uid = uuids::to_string(meshUid),
      .m_path = imported->sourcePath,
      .m_name = imported->name,
      .m_color = imported->display.baseColor,
      .m_opacity = imported->display.opacity,
      .m_visibleIn2d = imported->display.visibleIn2d,
      .m_visibleIn3d = imported->display.visibleIn3d});
  }

  return serializedImage;
}
