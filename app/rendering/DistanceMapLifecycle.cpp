#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "rendering/TextureSetup.h"
#include "rendering/helpers/TextureSetupHelpers.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <future>
#include <utility>

void Rendering::updateDistanceMapForRaycasting(const uuids::uuid& imageUid, uint32_t component)
{
  constexpr float k_distanceMapDownsample = 0.25f;
  using namespace std::chrono_literals;

  Image* image = m_appData.image(imageUid);
  const auto& renderData = m_appData.renderData();
  if (!image || !renderData.m_useDistanceMapForRaycasting) {
    return;
  }
  if (component >= image->header().numComponentsPerPixel()) {
    return;
  }
  const auto imageLayoutIt = m_appData.renderData().m_imageTextureLayouts.find(imageUid);
  if (
    imageLayoutIt == m_appData.renderData().m_imageTextureLayouts.end() ||
    imageLayoutIt->second.dimension != RenderData::TextureDimension::Texture3D)
  {
    return;
  }

  const auto foregroundThresholds = rendering::texture_setup::distanceMapForegroundThresholds(
    image->settings().componentStatistics(component),
    renderData.m_distanceMapForegroundLowerPercentile,
    renderData.m_distanceMapForegroundUpperPercentile);
  const DistanceMapGenerationRequest currentRequest{
    .pixelDataRevision = image->pixelDataRevision(),
    .foregroundThresholds = foregroundThresholds};

  auto imageTexturesIt = m_appData.renderData().m_distanceMapTextures.find(imageUid);
  const auto completedImageIt = m_completedDistanceMapGenerations.find(imageUid);
  const bool completedRequestMatches = completedImageIt != m_completedDistanceMapGenerations.end() &&
                                       completedImageIt->second.contains(component) &&
                                       completedImageIt->second.at(component) == currentRequest;
  if (
    imageTexturesIt != m_appData.renderData().m_distanceMapTextures.end() &&
    imageTexturesIt->second.contains(component) && completedRequestMatches)
  {
    return;
  }

  if (!completedRequestMatches) {
    m_appData.removeDistanceMaps(imageUid, component);
    if (imageTexturesIt != m_appData.renderData().m_distanceMapTextures.end()) {
      imageTexturesIt->second.erase(component);
      if (imageTexturesIt->second.empty()) {
        m_appData.renderData().m_distanceMapTextures.erase(imageTexturesIt);
      }
    }
  }

  if (completedRequestMatches && !m_appData.distanceMaps(imageUid, component).empty()) {
    if (auto texture = createDistanceMapTexture(m_appData, imageUid, component)) {
      m_appData.renderData().m_distanceMapTextures[imageUid].insert_or_assign(component, std::move(*texture));
    }
    return;
  }

  auto failedImageIt = m_failedDistanceMapGenerations.find(imageUid);
  if (failedImageIt != m_failedDistanceMapGenerations.end()) {
    auto failedComponentIt = failedImageIt->second.find(component);
    if (failedComponentIt != failedImageIt->second.end()) {
      if (failedComponentIt->second == currentRequest) {
        return;
      }
      failedImageIt->second.erase(failedComponentIt);
      if (failedImageIt->second.empty()) {
        m_failedDistanceMapGenerations.erase(failedImageIt);
      }
    }
  }

  auto& componentJobs = m_pendingDistanceMapGenerations[imageUid];
  auto jobIt = componentJobs.find(component);
  if (jobIt != componentJobs.end()) {
    if (!jobIt->second.future.valid() || jobIt->second.future.wait_for(0ms) != std::future_status::ready) {
      return;
    }

    const DistanceMapGenerationRequest requested = jobIt->second.request;
    std::optional<DistanceMapImageResult> result;
    try {
      result = jobIt->second.future.get();
    }
    catch (const std::exception& e) {
      spdlog::warn("Distance-map generation failed for component {} of image {}: {}", component, imageUid, e.what());
    }
    componentJobs.erase(jobIt);
    if (componentJobs.empty()) {
      m_pendingDistanceMapGenerations.erase(imageUid);
    }

    image = m_appData.image(imageUid);
    if (!image) {
      return;
    }
    if (
      image->pixelDataRevision() != requested.pixelDataRevision ||
      rendering::texture_setup::distanceMapForegroundThresholds(
        image->settings().componentStatistics(component),
        m_appData.renderData().m_distanceMapForegroundLowerPercentile,
        m_appData.renderData().m_distanceMapForegroundUpperPercentile) != requested.foregroundThresholds)
    {
      return;
    }
    if (!result || !m_appData.addDistanceMap(imageUid, component, std::move(result->image), result->boundaryIsoValue)) {
      m_failedDistanceMapGenerations[imageUid].insert_or_assign(component, requested);
      return;
    }
    m_completedDistanceMapGenerations[imageUid].insert_or_assign(component, requested);

    if (auto texture = createDistanceMapTexture(m_appData, imageUid, component)) {
      m_appData.renderData().m_distanceMapTextures[imageUid].insert_or_assign(component, std::move(*texture));
      spdlog::debug("Distance-map acceleration is ready for component {} of image {}", component, imageUid);
    }
    else {
      m_appData.removeDistanceMaps(imageUid, component);
      m_failedDistanceMapGenerations[imageUid].insert_or_assign(component, requested);
    }
    return;
  }

  if (image->header().interleavedComponents()) {
    m_failedDistanceMapGenerations[imageUid].insert_or_assign(component, currentRequest);
    return;
  }

  Image imageSnapshot = *image;
  auto future = std::async(
    std::launch::async,
    [imageSnapshot = std::move(imageSnapshot),
     component,
     downsamplingFactor = k_distanceMapDownsample,
     foregroundThresholds]() mutable {
      return createDistanceMapImage(imageSnapshot, component, downsamplingFactor, foregroundThresholds);
    });

  componentJobs.emplace(
    component,
    PendingDistanceMapGeneration{.request = currentRequest, .future = std::move(future)});
  spdlog::debug("Started lazy distance-map generation for component {} of image {}", component, imageUid);
}
