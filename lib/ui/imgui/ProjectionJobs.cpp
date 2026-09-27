#include "ui/ImGuiWrapper.h"

#include "common/UuidUtility.h"
#include "image/ImageUtility.h"
#include "logic/app/Data.h"
#include "rendering/TextureSetup.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <utility>

namespace
{
std::string componentProjectionTaskKey(const uuids::uuid& imageUid, ComponentProjectionMode mode, uint32_t timePoint)
{
  return uuids::to_string(imageUid) + ":" + std::to_string(static_cast<int>(mode)) + ":" + std::to_string(timePoint);
}

} // namespace

void ImGuiWrapper::requestComponentProjectionImage(const uuids::uuid& imageUid, ComponentProjectionMode mode)
{
  const Image* image = m_appData.image(imageUid);
  if (!image) {
    spdlog::warn("Cannot compute component projection for invalid image {}", imageUid);
    return;
  }

  const uint32_t timePoint = image->timeAxis().clamp(image->settings().activeTimePoint());
  requestComponentProjectionImage(imageUid, mode, timePoint);
}

void ImGuiWrapper::requestComponentProjectionImage(
  const uuids::uuid& imageUid,
  ComponentProjectionMode mode,
  uint32_t requestedTimePoint)
{
  const Image* image = m_appData.image(imageUid);
  if (!image) {
    spdlog::warn("Cannot compute component projection for invalid image {}", imageUid);
    return;
  }

  const uint32_t timePoint = image->timeAxis().clamp(requestedTimePoint);
  std::vector<uint32_t> requestedTimePoints;
  requestedTimePoints.reserve(image->timeAxis().numTimePoints());
  requestedTimePoints.push_back(timePoint);
  for (uint32_t i = 0; i < image->timeAxis().numTimePoints(); ++i) {
    if (i != timePoint) {
      requestedTimePoints.push_back(i);
    }
  }
  requestComponentProjectionImages(imageUid, mode, requestedTimePoints);
}

void ImGuiWrapper::requestComponentProjectionImages(
  const uuids::uuid& imageUid,
  ComponentProjectionMode mode,
  const std::vector<uint32_t>& timePoints)
{
  const Image* image = m_appData.image(imageUid);
  if (!image) {
    spdlog::warn("Cannot compute component projection for invalid image {}", imageUid);
    return;
  }

  std::vector<uint32_t> missingTimePoints;
  {
    std::lock_guard<std::mutex> lock(m_componentProjectionFuturesMutex);
    for (const uint32_t requestedTimePoint : timePoints) {
      const uint32_t timePoint = image->timeAxis().clamp(requestedTimePoint);
      if (m_appData.componentProjectionImageUid(imageUid, mode, timePoint)) {
        continue;
      }
      const std::string taskKey = componentProjectionTaskKey(imageUid, mode, timePoint);
      if (m_pendingComponentProjectionKeys.contains(taskKey)) {
        continue;
      }
      m_pendingComponentProjectionKeys.insert(taskKey);
      missingTimePoints.push_back(timePoint);
    }
  }

  if (missingTimePoints.empty()) {
    return;
  }

  const uuids::uuid taskUid = generateRandomUuid();
  Image imageCopy = *image;
  auto future =
    std::async(std::launch::async, [imageUid, mode, missingTimePoints, imageCopy = std::move(imageCopy)]() mutable {
      ComponentProjectionTaskResult result{imageUid, mode, {}};
      result.frames.reserve(missingTimePoints.size());
      for (const uint32_t timePoint : missingTimePoints) {
        result.frames.emplace_back(timePoint, createComponentProjectionImage(imageCopy, mode, timePoint));
      }
      return result;
    });

  {
    std::lock_guard<std::mutex> lock(m_componentProjectionFuturesMutex);
    m_componentProjectionFutures.emplace(taskUid, std::move(future));
  }

  spdlog::debug(
    "Started {} component projection task {} for image {} with {} frame(s)",
    componentProjectionModeName(mode),
    taskUid,
    imageUid,
    missingTimePoints.size());

  if (m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}

void ImGuiWrapper::requestMissingComponentProjectionImages()
{
  for (const auto& imageUid : m_appData.imageUidsOrdered()) {
    const Image* image = m_appData.image(imageUid);
    if (!image) {
      continue;
    }

    if (image->header().numComponentsPerPixel() < 2) {
      continue;
    }

    const auto mode = componentProjectionForImage(*image);
    const uint32_t timePoint = image->timeAxis().clamp(image->settings().activeTimePoint());
    if (mode) {
      std::vector<uint32_t> requestedTimePoints;
      requestedTimePoints.reserve(image->timeAxis().numTimePoints());
      requestedTimePoints.push_back(timePoint);
      for (uint32_t i = 0; i < image->timeAxis().numTimePoints(); ++i) {
        if (i != timePoint) {
          requestedTimePoints.push_back(i);
        }
      }
      requestComponentProjectionImages(imageUid, *mode, requestedTimePoints);
    }
  }
}

void ImGuiWrapper::processComponentProjectionFutures()
{
  using namespace std::chrono_literals;

  std::vector<uuids::uuid> readyTasks;
  {
    std::lock_guard<std::mutex> lock(m_componentProjectionFuturesMutex);
    for (auto& [taskUid, future] : m_componentProjectionFutures) {
      if (future.valid() && std::future_status::ready == future.wait_for(0ms)) {
        readyTasks.push_back(taskUid);
      }
    }
  }

  for (const auto& taskUid : readyTasks) {
    std::future<ComponentProjectionTaskResult> future;
    {
      std::lock_guard<std::mutex> lock(m_componentProjectionFuturesMutex);
      auto it = m_componentProjectionFutures.find(taskUid);
      if (m_componentProjectionFutures.end() == it) {
        continue;
      }
      future = std::move(it->second);
      m_componentProjectionFutures.erase(it);
    }

    ComponentProjectionTaskResult result = future.get();
    {
      std::lock_guard<std::mutex> lock(m_componentProjectionFuturesMutex);
      for (const auto& [timePoint, image] : result.frames) {
        (void)image;
        m_pendingComponentProjectionKeys.erase(
          componentProjectionTaskKey(result.sourceImageUid, result.mode, timePoint));
      }
    }

    for (auto& [timePoint, image] : result.frames) {
      if (!image) {
        spdlog::warn(
          "Unable to compute {} component projection for image {} frame {}: {}",
          componentProjectionModeName(result.mode),
          result.sourceImageUid,
          timePoint,
          image.error());
        continue;
      }

      const auto projectionUid =
        m_appData.setComponentProjectionImage(result.sourceImageUid, result.mode, timePoint, std::move(*image));
      if (!projectionUid) {
        spdlog::warn("Source image {} no longer exists for component projection", result.sourceImageUid);
        continue;
      }

      m_appData.renderResources().m_imageTextures.erase(*projectionUid);
      createImageTextures(m_appData, std::vector<uuids::uuid>{*projectionUid});

      if (m_updateImageUniforms) {
        m_updateImageUniforms(*projectionUid);
      }
    }

    if (m_updateImageUniforms) {
      m_updateImageUniforms(result.sourceImageUid);
    }

    spdlog::debug(
      "Finished {} component projection for image {} with {} frame(s)",
      componentProjectionModeName(result.mode),
      result.sourceImageUid,
      result.frames.size());
  }

  if (!readyTasks.empty() && m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}
