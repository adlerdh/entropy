#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "rendering/TextureSetup.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <future>
#include <utility>

void Rendering::updateDistanceMapForRaycasting(const uuids::uuid& imageUid, uint32_t component)
{
  constexpr float k_distanceMapDownsample = 0.25f;
  using namespace std::chrono_literals;

  Image* image = m_appData.image(imageUid);
  if (!image || !image->settings().useDistanceMapForRaycasting()) {
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

  auto imageTexturesIt = m_appData.renderData().m_distanceMapTextures.find(imageUid);
  if (
    imageTexturesIt != m_appData.renderData().m_distanceMapTextures.end() &&
    imageTexturesIt->second.contains(component))
  {
    return;
  }

  if (!m_appData.distanceMaps(imageUid, component).empty()) {
    if (auto texture = createDistanceMapTexture(m_appData, imageUid, component)) {
      m_appData.renderData().m_distanceMapTextures[imageUid].insert_or_assign(component, std::move(*texture));
    }
    return;
  }

  const DistanceMapGenerationRequest currentRequest{
    .pixelDataRevision = image->pixelDataRevision(),
    .foregroundThresholds = image->settings().foregroundThresholds(component)};
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
      image->settings().foregroundThresholds(component) != requested.foregroundThresholds)
    {
      return;
    }
    if (!result || !m_appData.addDistanceMap(imageUid, component, std::move(result->image), result->boundaryIsoValue)) {
      m_failedDistanceMapGenerations[imageUid].insert_or_assign(component, requested);
      return;
    }

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
    [imageSnapshot = std::move(imageSnapshot), component, downsamplingFactor = k_distanceMapDownsample]() mutable {
      return createDistanceMapImage(imageSnapshot, component, downsamplingFactor);
    });

  componentJobs.emplace(
    component,
    PendingDistanceMapGeneration{.request = currentRequest, .future = std::move(future)});
  spdlog::debug("Started lazy distance-map generation for component {} of image {}", component, imageUid);
}
