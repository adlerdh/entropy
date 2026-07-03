#include "EntropyApp.h"

#include "logic/app/DataHelper.h"

#include "registration/AffineTransformIO.h"
#include "registration/Artifacts.h"
#include "registration/ImportPlan.h"

#include <glm/glm.hpp>

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;

namespace
{

std::optional<glm::dmat4> readRegistrationSamplingAffine(const fs::path& affinePath, registration::Backend backend)
{
  return (backend == registration::Backend::Greedy) ? registration::readGreedyAffineTransform(affinePath)
                                                    : registration::readAffineTransform(affinePath);
}

std::optional<glm::dmat4> readRegistrationDisplayAffine(const fs::path& affinePath, registration::Backend backend)
{
  const std::optional<glm::dmat4> affine = readRegistrationSamplingAffine(affinePath, backend);
  if (!affine) {
    return std::nullopt;
  }
  return glm::inverse(*affine);
}

bool applyRegistrationAffineToImage(
  AppData& appData,
  const uuids::uuid& imageUid,
  const fs::path& affinePath,
  registration::Backend backend)
{
  Image* image = appData.image(imageUid);
  if (!image) {
    return false;
  }

  const std::optional<glm::dmat4> displayAffine = readRegistrationDisplayAffine(affinePath, backend);
  if (!displayAffine) {
    return false;
  }

  ImageTransformations& imageTx = image->transformations();
  const bool wasLocked = imageTx.is_worldDef_T_affine_locked();
  imageTx.set_worldDef_T_affine_locked(false);
  imageTx.set_enable_affine_T_subject(true);
  imageTx.set_affine_T_subject(glm::mat4{*displayAffine});
  imageTx.set_affine_T_subject_fileName(affinePath);
  imageTx.set_enable_worldDef_T_affine(true);
  imageTx.reset_worldDef_T_affine();
  imageTx.set_worldDef_T_affine_locked(wasLocked);

  for (const uuids::uuid& segUid : appData.imageToSegUids(imageUid)) {
    Image* seg = appData.seg(segUid);
    if (!seg) {
      continue;
    }

    ImageTransformations& segTx = seg->transformations();
    const bool wasSegLocked = segTx.is_worldDef_T_affine_locked();
    segTx.set_worldDef_T_affine_locked(false);
    segTx.set_enable_affine_T_subject(imageTx.get_enable_affine_T_subject());
    segTx.set_affine_T_subject(imageTx.get_affine_T_subject());
    segTx.set_affine_T_subject_fileName(affinePath);
    segTx.set_enable_worldDef_T_affine(imageTx.get_enable_worldDef_T_affine());
    segTx.reset_worldDef_T_affine();
    segTx.set_worldDef_T_affine_locked(wasSegLocked);
  }

  return true;
}

std::vector<GuiData::LoadingStatusItem> registrationLoadingItems(const registration::ImportPlan& plan)
{
  std::vector<GuiData::LoadingStatusItem> items;
  for (const registration::ImportStep& step : plan.steps) {
    GuiData::LoadingStatusItem::Kind kind = GuiData::LoadingStatusItem::Kind::Image;
    switch (step.action) {
      case registration::ImportAction::LoadWarpedImage:
      case registration::ImportAction::LoadInverseWarp:
      case registration::ImportAction::LoadForwardWarp:
        kind = GuiData::LoadingStatusItem::Kind::Image;
        break;
      case registration::ImportAction::LoadWarpedSegmentation:
        kind = GuiData::LoadingStatusItem::Kind::Segmentation;
        break;
      case registration::ImportAction::ApplyAffineTransform:
      case registration::ImportAction::AssignWarpsToMovingImage:
      case registration::ImportAction::TransformLandmarksAndAnnotations:
      case registration::ImportAction::LoadTransformedSurface:
      case registration::ImportAction::MakeWarpedImageActive:
        continue;
    }

    std::error_code error;
    const std::uintmax_t bytes = fs::file_size(step.path, error);
    items.push_back(GuiData::LoadingStatusItem{kind, step.path, error ? std::nullopt : std::optional{bytes}, false});
  }
  return items;
}

} // namespace

void EntropyApp::importRegistrationJobOutputs(const std::string& jobId)
{
  registration::JobStore& jobs = m_data.registrationJobs();
  const registration::JobRecord* job = jobs.find(jobId);
  if (!job || !job->manifest) {
    return;
  }

  if (ProjectLoadState::Loading == m_data.state().projectLoadState() || m_data.state().animating()) {
    spdlog::warn("Ignoring registration output import because another load task is running");
    return;
  }

  const registration::JobSpec spec = job->spec;
  const registration::JobStatus statusBeforeImport = job->status;
  registration::ResultManifest manifest = *job->manifest;
  auto appendEvent = [&jobs, &jobId](registration::ProgressEventKind kind, std::string message) {
    jobs.appendProgress(jobId, registration::ProgressEvent{.kind = kind, .message = std::move(message)});
  };

  if (spec.backend == registration::Backend::ANTs && registration::includesDeformableTransform(spec.transformModel)) {
    const registration::ResultManifest expectedManifest = registration::buildExpectedResultManifest(spec);
    if (
      manifest.inverseWarp.empty() && !expectedManifest.inverseWarp.empty() && fs::exists(expectedManifest.inverseWarp))
    {
      manifest.inverseWarp = expectedManifest.inverseWarp;
      appendEvent(
        registration::ProgressEventKind::Artifact,
        "Discovered ANTs inverse warp artifact: " + manifest.inverseWarp.string());
    }
    if (
      manifest.forwardWarp.empty() && !expectedManifest.forwardWarp.empty() && fs::exists(expectedManifest.forwardWarp))
    {
      manifest.forwardWarp = expectedManifest.forwardWarp;
      appendEvent(
        registration::ProgressEventKind::Artifact,
        "Discovered ANTs forward warp artifact: " + manifest.forwardWarp.string());
    }
  }

  const registration::ImportPlan plan = registration::buildImportPlan(spec, manifest);
  m_preserveLayoutsOnImagesReady = true;
  m_pendingAddedImageUids.clear();
  appendEvent(registration::ProgressEventKind::Progress, "Importing registration outputs.");

  startAsyncImageLoad(
    "Importing registration outputs...",
    [this, jobId, spec, plan, statusBeforeImport]() {
      registration::JobStore& asyncJobs = m_data.registrationJobs();
      auto appendAsyncEvent = [&asyncJobs, &jobId](registration::ProgressEventKind kind, std::string message) {
        asyncJobs.appendProgress(jobId, registration::ProgressEvent{.kind = kind, .message = std::move(message)});
      };

      auto parseTargetImageUid =
        [&appendAsyncEvent](const registration::ImportStep& step) -> std::optional<uuids::uuid> {
        if (step.targetImageUid.empty()) {
          appendAsyncEvent(
            registration::ProgressEventKind::Warning,
            "Registration import step has no target image UID.");
          return std::nullopt;
        }
        std::optional<uuids::uuid> uid = uuids::uuid::from_string(step.targetImageUid);
        if (!uid) {
          appendAsyncEvent(
            registration::ProgressEventKind::Warning,
            "Registration import step has an invalid target image UID: " + step.targetImageUid);
        }
        return uid;
      };

      auto attachSegmentation = [&](const uuids::uuid& imageUid, const fs::path& fileName) -> bool {
        std::optional<uuids::uuid> segUid;
        bool isNewSeg = false;
        std::tie(segUid, isNewSeg) = loadSegmentation(fileName, imageUid);
        if (!segUid) {
          return false;
        }

        Image* image = m_data.image(imageUid);
        Image* seg = m_data.seg(*segUid);
        if (!image || !seg) {
          return false;
        }

        const std::vector<uuids::uuid> assignedSegUids = m_data.imageToSegUids(imageUid);
        const bool alreadyAssigned =
          std::find(assignedSegUids.begin(), assignedSegUids.end(), *segUid) != assignedSegUids.end();

        if (isNewSeg && !data::createLabelColorTableForSegmentation(m_data, *segUid)) {
          constexpr size_t defaultTableIndex = 0;
          spdlog::error(
            "Unable to create label color table for segmentation {}. Defaulting to table index {}.",
            *segUid,
            defaultTableIndex);
          seg->settings().setLabelTableIndex(defaultTableIndex);
        }

        if (!alreadyAssigned && !m_data.assignSegUidToImage(imageUid, *segUid)) {
          if (isNewSeg) {
            m_data.removeSeg(*segUid);
          }
          return false;
        }

        m_data.assignActiveSegUidToImage(imageUid, *segUid);
        seg->transformations().set_affine_T_subject(image->transformations().get_affine_T_subject());
        m_callbackHandler.syncManualImageTransformationOnSegs(imageUid);
        markLoadingStatusItemLoaded(GuiData::LoadingStatusItem::Kind::Segmentation, fileName);
        return true;
      };

      for (const std::string& warning : plan.warnings) {
        appendAsyncEvent(registration::ProgressEventKind::Warning, warning);
      }

      std::vector<uuids::uuid> addedImageUids;
      auto recordAddedImageUid = [&addedImageUids](const uuids::uuid& imageUid) {
        if (std::find(addedImageUids.begin(), addedImageUids.end(), imageUid) == addedImageUids.end()) {
          addedImageUids.push_back(imageUid);
        }
      };
      std::optional<uuids::uuid> warpedImageUid;
      bool hadError = false;

      for (const registration::ImportStep& step : plan.steps) {
        try {
          if (!step.path.empty() && !fs::exists(step.path)) {
            appendAsyncEvent(
              registration::ProgressEventKind::Warning,
              "Registration output does not exist and was not imported: " + step.path.string());
            continue;
          }

          switch (step.action) {
            case registration::ImportAction::ApplyAffineTransform: {
              const std::optional<uuids::uuid> imageUid = parseTargetImageUid(step);
              if (!imageUid) {
                break;
              }
              if (applyRegistrationAffineToImage(m_data, *imageUid, step.path, spec.backend)) {
                appendAsyncEvent(
                  registration::ProgressEventKind::Artifact,
                  "Applied affine transform: " + step.path.string());
              }
              else {
                appendAsyncEvent(
                  registration::ProgressEventKind::Warning,
                  "Unable to parse or apply affine transform: " + step.path.string());
              }
              break;
            }
            case registration::ImportAction::LoadInverseWarp: {
              const std::optional<uuids::uuid> imageUid = parseTargetImageUid(step);
              if (!imageUid) {
                break;
              }
              const auto [warpUid, loaded] = loadDeformationField(step.path);
              if (warpUid && m_data.assignInverseWarpUidToImage(*imageUid, *warpUid)) {
                if (loaded) {
                  recordAddedImageUid(*warpUid);
                }
                appendAsyncEvent(
                  registration::ProgressEventKind::Artifact,
                  "Imported inverse warp: " + step.path.string());
              }
              else {
                if (loaded && warpUid) {
                  m_data.removeDef(*warpUid);
                }
                appendAsyncEvent(
                  registration::ProgressEventKind::Warning,
                  "Unable to import inverse warp: " + step.path.string());
              }
              break;
            }
            case registration::ImportAction::LoadForwardWarp: {
              const std::optional<uuids::uuid> imageUid = parseTargetImageUid(step);
              if (!imageUid) {
                break;
              }
              const auto [warpUid, loaded] = loadDeformationField(step.path);
              if (warpUid && m_data.assignForwardWarpUidToImage(*imageUid, *warpUid)) {
                if (loaded) {
                  recordAddedImageUid(*warpUid);
                }
                appendAsyncEvent(
                  registration::ProgressEventKind::Artifact,
                  "Imported forward warp: " + step.path.string());
              }
              else {
                if (loaded && warpUid) {
                  m_data.removeDef(*warpUid);
                }
                appendAsyncEvent(
                  registration::ProgressEventKind::Warning,
                  "Unable to import forward warp: " + step.path.string());
              }
              break;
            }
            case registration::ImportAction::AssignWarpsToMovingImage:
              appendAsyncEvent(
                registration::ProgressEventKind::Progress,
                "Assigned imported warps to the moving image.");
              break;
            case registration::ImportAction::LoadWarpedImage: {
              const std::size_t numImagesBeforeLoad = m_data.numImages();
              serialize::Image serializedImage;
              serializedImage.m_imageFileName = step.path;
              if (loadSerializedImage(serializedImage, false) && m_data.numImages() > numImagesBeforeLoad) {
                if (const auto imageUid = m_data.imageUid(m_data.numImages() - 1)) {
                  warpedImageUid = imageUid;
                  recordAddedImageUid(*imageUid);
                }
                appendAsyncEvent(
                  registration::ProgressEventKind::Artifact,
                  "Imported warped image: " + step.path.string());
              }
              else {
                appendAsyncEvent(
                  registration::ProgressEventKind::Warning,
                  "Unable to import warped image: " + step.path.string());
              }
              break;
            }
            case registration::ImportAction::LoadWarpedSegmentation: {
              const std::optional<uuids::uuid> imageUid = parseTargetImageUid(step);
              if (imageUid && attachSegmentation(*imageUid, step.path)) {
                appendAsyncEvent(
                  registration::ProgressEventKind::Artifact,
                  "Imported warped segmentation: " + step.path.string());
              }
              else {
                appendAsyncEvent(
                  registration::ProgressEventKind::Warning,
                  "Unable to import warped segmentation: " + step.path.string());
              }
              break;
            }
            case registration::ImportAction::TransformLandmarksAndAnnotations:
              appendAsyncEvent(
                registration::ProgressEventKind::Warning,
                "Transformed landmark/annotation import is not wired to the app yet: " + step.path.string());
              break;
            case registration::ImportAction::LoadTransformedSurface:
              appendAsyncEvent(
                registration::ProgressEventKind::Warning,
                "Transformed surface import is not wired to the app yet: " + step.path.string());
              break;
            case registration::ImportAction::MakeWarpedImageActive:
              if (warpedImageUid) {
                m_data.setActiveImageUid(*warpedImageUid);
              }
              else {
                appendAsyncEvent(
                  registration::ProgressEventKind::Warning,
                  "The warped image could not be made active because it was not imported.");
              }
              break;
          }
        }
        catch (const std::exception& e) {
          hadError = true;
          appendAsyncEvent(registration::ProgressEventKind::Warning, e.what());
          spdlog::error("Exception while importing registration output for job {}: {}", jobId, e.what());
          break;
        }
      }

      if (hadError) {
        asyncJobs.setStatus(jobId, statusBeforeImport);
        return false;
      }

      m_pendingAddedImageUids = std::move(addedImageUids);
      m_data.setRainbowColorsForAllImages();
      m_data.setRainbowColorsForAllLandmarkGroups();
      m_data.setProject(createProjectSnapshot());
      appendAsyncEvent(registration::ProgressEventKind::Completed, "Registration outputs imported.");
      asyncJobs.setStatus(jobId, statusBeforeImport);
      return true;
    },
    [this, jobId, statusBeforeImport]() {
      m_preserveLayoutsOnImagesReady = false;
      m_pendingAddedImageUids.clear();
      m_data.registrationJobs().setStatus(jobId, statusBeforeImport);
      m_data.state().setProjectLoadState(ProjectLoadState::Loaded);
      m_data.state().setAnimating(false);
      hideLoadingStatus();
      updateWindowTitleStatus();
      m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
    },
    false,
    registrationLoadingItems(plan),
    "Importing registration outputs");
}
