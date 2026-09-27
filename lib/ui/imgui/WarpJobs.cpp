#include "ui/ImGuiWrapper.h"

#include "common/UuidUtility.h"
#include "logic/app/Data.h"
#include "rendering/TextureSetup.h"
#include "ui/Scaling.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <functional>
#include <imgui/imgui.h>
#include <utility>

namespace
{
std::string
warpInversionTaskKey(const uuids::uuid& imageUid, const uuids::uuid& sourceWarpUid, ComputedWarpDirection direction)
{
  return uuids::to_string(imageUid) + ":" + uuids::to_string(sourceWarpUid) + ":" +
         std::to_string(static_cast<int>(direction));
}

std::string computedWarpDirectionLabel(ComputedWarpDirection direction)
{
  return ComputedWarpDirection::Inverse == direction ? "inverse" : "forward";
}

void logWarpInversionReport(
  const Image& image,
  const Image& sourceWarp,
  const WarpInversionResult& result,
  const uuids::uuid& resultUid)
{
  const WarpInversionReport& report = result.report;
  const double outsidePercent = report.samples + report.outsideOppositeField > 0
                                  ? 100.0 * static_cast<double>(report.outsideOppositeField) /
                                      static_cast<double>(report.samples + report.outsideOppositeField)
                                  : 0.0;

  spdlog::info(
    "Computed {} warp '{}' ({}) for image '{}' from '{}'. "
    "elapsed={:.3f}s, itkMeanError={:.6g}, itkMaxError={:.6g}, "
    "meanResidual={:.6g} mm ({:.6g} vox), maxResidual={:.6g} mm ({:.6g} vox), "
    "outsideOppositeField={}/{} ({:.2f}%)",
    computedWarpDirectionLabel(result.direction),
    result.image.settings().displayName(),
    resultUid,
    image.settings().displayName(),
    sourceWarp.settings().displayName(),
    report.elapsedSeconds,
    report.itkMeanErrorNorm,
    report.itkMaxErrorNorm,
    report.meanResidualMm,
    report.meanResidualVoxels,
    report.maxResidualMm,
    report.maxResidualVoxels,
    report.outsideOppositeField,
    report.samples + report.outsideOppositeField,
    outsidePercent);
}
} // namespace

void ImGuiWrapper::requestWarpInversion(
  const uuids::uuid& imageUid,
  const uuids::uuid& sourceWarpUid,
  ComputedWarpDirection direction,
  const WarpInversionOptions& options)
{
  const Image* image = m_appData.image(imageUid);
  const Image* sourceWarp = m_appData.warpField(sourceWarpUid);
  const auto refImageUid = m_appData.refImageUid();
  const Image* referenceImage = refImageUid ? m_appData.image(*refImageUid) : nullptr;
  const Image* outputDomain = ComputedWarpDirection::Inverse == direction ? referenceImage : image;

  if (!image || !sourceWarp || !outputDomain) {
    spdlog::warn(
      "Cannot compute {} warp: missing image, source warp, or output domain",
      computedWarpDirectionLabel(direction));
    return;
  }

  const std::string key = warpInversionTaskKey(imageUid, sourceWarpUid, direction);
  {
    std::lock_guard<std::mutex> lock(m_warpInversionFuturesMutex);
    if (m_pendingWarpInversionKeys.contains(key)) {
      return;
    }
    m_pendingWarpInversionKeys.insert(key);
  }

  const uuids::uuid taskUid = generateRandomUuid();
  auto progress = std::make_shared<std::atomic<double>>(0.0);
  auto cancel = std::make_shared<std::atomic_bool>(false);
  auto lastPostedProgress = std::make_shared<std::atomic<double>>(0.0);
  auto postEmptyEvent = m_postEmptyGlfwEvent;

  WarpInversionTaskState state{
    .imageUid = imageUid,
    .sourceWarpUid = sourceWarpUid,
    .domainUid = ComputedWarpDirection::Inverse == direction ? *refImageUid : imageUid,
    .referenceUid = refImageUid,
    .targetWarpUid = direction == ComputedWarpDirection::Inverse ? m_appData.imageToActiveInverseWarpUid(imageUid)
                                                                 : m_appData.imageToActiveForwardWarpUid(imageUid),
    .sourcePixelRevision = sourceWarp->pixelDataRevision(),
    .sourceGeometryRevision = sourceWarp->geometryRevision(),
    .domainGeometryRevision = outputDomain->geometryRevision(),
    .imageGeometryRevision = image->geometryRevision(),
    .sourceTimePoint = sourceWarp->settings().activeTimePoint(),
    .sourceTransform = sourceWarp->transformations().worldDef_T_subject(),
    .imageTransform = image->transformations().worldDef_T_subject(),
    .direction = direction,
    .description = std::format("Computing {} warp", computedWarpDirectionLabel(direction)),
    .progress = progress,
    .cancel = cancel};

  Image sourceWarpCopy = *sourceWarp;
  Image outputDomainCopy = *outputDomain;

  auto future = std::async(
    std::launch::async,
    [imageUid,
     sourceWarpUid,
     direction,
     options,
     progress,
     cancel,
     lastPostedProgress,
     postEmptyEvent,
     sourceWarpCopy = std::move(sourceWarpCopy),
     outputDomainCopy = std::move(outputDomainCopy)]() mutable {
      auto progressCallback = [progress, lastPostedProgress, postEmptyEvent](double value) {
        const double clamped = std::clamp(value, 0.0, 1.0);
        progress->store(clamped);
        const double previous = lastPostedProgress->load();
        if (postEmptyEvent && (clamped >= 1.0 || clamped - previous >= 0.01)) {
          lastPostedProgress->store(clamped);
          postEmptyEvent();
        }
      };

      return WarpInversionTaskResult{
        imageUid,
        sourceWarpUid,
        direction,
        computeMatchingWarp(sourceWarpCopy, outputDomainCopy, direction, options, progressCallback, cancel.get())};
    });

  {
    std::lock_guard<std::mutex> lock(m_warpInversionFuturesMutex);
    m_warpInversionTaskStates.emplace(taskUid, std::move(state));
    m_latestWarpInversionTasks[uuids::to_string(imageUid) + ":" + computedWarpDirectionLabel(direction)] = taskUid;
    m_warpInversionFutures.emplace(taskUid, std::move(future));
  }

  spdlog::info(
    "Started {} warp computation for image {} from source warp {}",
    computedWarpDirectionLabel(direction),
    imageUid,
    sourceWarpUid);

  if (m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}

void ImGuiWrapper::processWarpInversionFutures()
{
  using namespace std::chrono_literals;

  std::vector<uuids::uuid> readyTasks;
  {
    std::lock_guard<std::mutex> lock(m_warpInversionFuturesMutex);
    for (auto& [taskUid, future] : m_warpInversionFutures) {
      if (future.valid() && std::future_status::ready == future.wait_for(0ms)) {
        readyTasks.push_back(taskUid);
      }
    }
  }

  for (const auto& taskUid : readyTasks) {
    std::future<WarpInversionTaskResult> future;
    WarpInversionTaskState state;
    {
      std::lock_guard<std::mutex> lock(m_warpInversionFuturesMutex);
      auto it = m_warpInversionFutures.find(taskUid);
      if (m_warpInversionFutures.end() == it) {
        continue;
      }
      future = std::move(it->second);
      state = m_warpInversionTaskStates.at(taskUid);
      m_warpInversionFutures.erase(it);
    }

    std::optional<WarpInversionTaskResult> taskResult;
    try {
      taskResult = future.get();
    }
    catch (const std::exception& e) {
      spdlog::error("Warp inversion task {} failed: {}", taskUid, e.what());
      std::lock_guard<std::mutex> lock(m_warpInversionFuturesMutex);
      if (const auto stateIt = m_warpInversionTaskStates.find(taskUid); m_warpInversionTaskStates.end() != stateIt) {
        m_pendingWarpInversionKeys.erase(
          warpInversionTaskKey(stateIt->second.imageUid, stateIt->second.sourceWarpUid, stateIt->second.direction));
      }
      m_warpInversionTaskStates.erase(taskUid);
      continue;
    }

    {
      std::lock_guard<std::mutex> lock(m_warpInversionFuturesMutex);
      m_pendingWarpInversionKeys.erase(
        warpInversionTaskKey(taskResult->imageUid, taskResult->sourceWarpUid, taskResult->direction));
      m_warpInversionTaskStates.erase(taskUid);
    }

    if (!taskResult->result) {
      spdlog::warn(
        "Unable to compute {} warp for image {} from source warp {}: {}",
        computedWarpDirectionLabel(taskResult->direction),
        taskResult->imageUid,
        taskResult->sourceWarpUid,
        taskResult->result.error());
      continue;
    }

    const std::string latestKey =
      uuids::to_string(taskResult->imageUid) + ":" + computedWarpDirectionLabel(taskResult->direction);
    if (!warp_inversion::canPublish(m_appData, state, m_latestWarpInversionTasks[latestKey] == taskUid)) {
      spdlog::warn("Discarding computed warp because its inputs, domain, or requested assignment changed");
      continue;
    }

    WarpInversionResult result = std::move(*taskResult->result);
    const std::optional<uuids::uuid> resultUid = m_appData.addDef(std::move(result.image));
    if (!resultUid) {
      spdlog::warn("Unable to add computed {} warp to the project", computedWarpDirectionLabel(taskResult->direction));
      continue;
    }

    createImageTextures(m_appData, std::vector<uuids::uuid>{*resultUid});

    if (ComputedWarpDirection::Inverse == taskResult->direction) {
      (void)m_appData.assignInverseWarpUidToImage(taskResult->imageUid, *resultUid, state.domainUid);
    }
    else {
      (void)m_appData.assignForwardWarpUidToImage(taskResult->imageUid, *resultUid);
    }

    const Image* resultImage = m_appData.warpField(*resultUid);
    if (resultImage) {
      result.image = *resultImage;
    }
    logWarpInversionReport(
      *m_appData.image(state.imageUid),
      *m_appData.warpField(state.sourceWarpUid),
      result,
      *resultUid);

    if (m_updateImageUniforms) {
      m_updateImageUniforms(taskResult->imageUid);
    }
  }

  if (!readyTasks.empty() && m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}

void ImGuiWrapper::renderWarpInversionProgressPopup()
{
  std::vector<WarpInversionTaskState> states;
  {
    std::lock_guard<std::mutex> lock(m_warpInversionFuturesMutex);
    for (const auto& [taskUid, state] : m_warpInversionTaskStates) {
      states.push_back(state);
    }
  }

  if (states.empty()) {
    return;
  }

  ImGui::OpenPopup("Warp inversion progress");
  if (ImGui::BeginPopupModal("Warp inversion progress", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    for (auto& state : states) {
      ImGui::TextUnformatted(state.description.c_str());
      const float progress = static_cast<float>(state.progress ? state.progress->load() : 0.0);
      ImGui::ProgressBar(progress, ImVec2{ui::scaledPixel(320.0f), 0.0f});
      if (state.cancel && ImGui::Button("Cancel")) {
        state.cancel->store(true);
      }
    }
    ImGui::EndPopup();
  }
}
