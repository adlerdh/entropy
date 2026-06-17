#include "windowing/ControlFrame.h"

#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"
#include "windowing/ImageSelection.h"

#include <glm/glm.hpp>

ControlFrame::ControlFrame(
  glm::vec4 winClipViewport,
  ViewType viewType,
  ViewRenderMode renderMode,
  IntensityProjectionMode ipMode,
  UiControls uiControls)
  : m_winClipViewport(std::move(winClipViewport))
  , m_windowClip_T_viewClip(helper::compute_windowClip_T_viewClip(m_winClipViewport))
  , m_viewClip_T_windowClip(glm::inverse(m_windowClip_T_viewClip))
  , m_renderedImageUids()
  , m_metricImageUids()
  , m_preferredDefaultRenderedImages({}) // Don't specify images to render by default
  , m_defaultRenderAllImages(true)       // Render all images by default in the frame:
  , m_viewType(viewType)
  , m_renderMode(renderMode)
  , m_intensityProjectionMode(ipMode)
  , m_uiControls(std::move(uiControls))
{
}

bool ControlFrame::isImageRendered(const AppData& appData, std::size_t index)
{
  auto imageUid = appData.imageUid(index);
  if (!imageUid) {
    return false; // invalid image index
  }
  return isImageRendered(*imageUid);
}

bool ControlFrame::isImageRendered(const uuids::uuid& imageUid)
{
  auto it = std::find(std::begin(m_renderedImageUids), std::end(m_renderedImageUids), imageUid);
  return (std::end(m_renderedImageUids) != it);
}

void ControlFrame::setImageRendered(const AppData& appData, std::size_t index, bool visible)
{
  auto imageUid = appData.imageUid(index);
  if (!imageUid) {
    return; // invalid image index
  }
  setImageRendered(appData, *imageUid, visible);
}

void ControlFrame::setImageRendered(const AppData& appData, const uuids::uuid& imageUid, bool visible)
{
  if (!visible) {
    m_renderedImageUids.remove(imageUid);
    return;
  }

  if (
    std::end(m_renderedImageUids) !=
    std::find(std::begin(m_renderedImageUids), std::end(m_renderedImageUids), imageUid))
  {
    return; // image already exists, so do nothing
  }

  const auto imageIndex = appData.imageIndex(imageUid);
  if (!imageIndex) {
    return; // invalid image index
  }

  bool inserted = false;

  for (auto it = std::begin(m_renderedImageUids); std::end(m_renderedImageUids) != it; ++it) {
    if (const auto i = appData.imageIndex(*it)) {
      if (*imageIndex < *i) {
        // Insert the desired image in the right place
        m_renderedImageUids.insert(it, imageUid);
        inserted = true;
        break;
      }
    }
  }

  if (!inserted) {
    m_renderedImageUids.push_back(imageUid);
  }
}

const std::list<uuids::uuid>& ControlFrame::renderedImages() const
{
  return m_renderedImageUids;
}

void ControlFrame::setRenderedImages(const std::list<uuids::uuid>& imageUids, bool filterByDefaults)
{
  if (filterByDefaults) {
    m_renderedImageUids =
      windowing::filteredDefaultRenderedImages(imageUids, m_defaultRenderAllImages, m_preferredDefaultRenderedImages);
  }
  else {
    m_renderedImageUids = imageUids;
  }
}

bool ControlFrame::isImageUsedForMetric(const AppData& appData, std::size_t index)
{
  auto imageUid = appData.imageUid(index);
  if (!imageUid) {
    return false; // invalid image index
  }
  return isImageUsedForMetric(*imageUid);
}

bool ControlFrame::isImageUsedForMetric(const uuids::uuid& imageUid)
{
  auto it = std::find(std::begin(m_metricImageUids), std::end(m_metricImageUids), imageUid);
  return (std::end(m_metricImageUids) != it);
}

void ControlFrame::setImageUsedForMetric(const AppData& appData, std::size_t index, bool visible)
{
  // Maximum number of images that are used for metric computations in a view
  static constexpr std::size_t MAX_METRIC_IMAGES = 2;

  auto imageUid = appData.imageUid(index);
  if (!imageUid) {
    return; // invalid image index
  }

  if (!visible) {
    m_metricImageUids.remove(*imageUid);
    return;
  }

  if (std::end(m_metricImageUids) != std::find(std::begin(m_metricImageUids), std::end(m_metricImageUids), *imageUid)) {
    return; // image already exists, so do nothing
  }

  if (m_metricImageUids.size() >= MAX_METRIC_IMAGES) {
    // If trying to add another image UID to list with 2 or more UIDs,
    // remove the last UID to make room:
    m_metricImageUids.erase(std::prev(std::end(m_metricImageUids)));
  }

  bool inserted = false;

  for (auto it = std::begin(m_metricImageUids); std::end(m_metricImageUids) != it; ++it) {
    if (const auto i = appData.imageIndex(*it)) {
      if (index < *i) {
        // Insert the desired image in the right place
        m_metricImageUids.insert(it, *imageUid);
        inserted = true;
        break;
      }
    }
  }

  if (!inserted) {
    m_metricImageUids.push_back(*imageUid);
  }
}

const std::list<uuids::uuid>& ControlFrame::metricImages() const
{
  return m_metricImageUids;
}

void ControlFrame::setMetricImages(const std::list<uuids::uuid>& imageUids)
{
  m_metricImageUids = imageUids;
}

const std::list<uuids::uuid>& ControlFrame::visibleImages() const
{
  static const std::list<uuids::uuid> sk_noImages;

  switch (m_renderMode) {
    case ViewRenderMode::Image: {
      return renderedImages();
    }
    case ViewRenderMode::Disabled: {
      return sk_noImages;
    }
    default: {
      return metricImages();
    }
  }
}

void ControlFrame::updateImageOrdering(uuid_range_t orderedImageUids)
{
  m_renderedImageUids = windowing::reorderSelectedImages(m_renderedImageUids, orderedImageUids);
  m_metricImageUids = windowing::reorderSelectedImages(m_metricImageUids, orderedImageUids, 2);
}

void ControlFrame::setPreferredDefaultRenderedImages(std::set<std::size_t> imageIndices)
{
  m_preferredDefaultRenderedImages = std::move(imageIndices);
}

const std::set<std::size_t>& ControlFrame::preferredDefaultRenderedImages() const
{
  return m_preferredDefaultRenderedImages;
}

void ControlFrame::setDefaultRenderAllImages(bool renderAll)
{
  m_defaultRenderAllImages = renderAll;
}

bool ControlFrame::defaultRenderAllImages() const
{
  return m_defaultRenderAllImages;
}

void ControlFrame::setWindowClipViewport(glm::vec4 winClipViewport)
{
  m_winClipViewport = std::move(winClipViewport);
  m_windowClip_T_viewClip = helper::compute_windowClip_T_viewClip(m_winClipViewport);
  m_viewClip_T_windowClip = glm::inverse(m_windowClip_T_viewClip);
}

const glm::vec4& ControlFrame::windowClipViewport() const
{
  return m_winClipViewport;
}

const glm::mat4& ControlFrame::windowClip_T_viewClip() const
{
  return m_windowClip_T_viewClip;
}

const glm::mat4& ControlFrame::viewClip_T_windowClip() const
{
  return m_viewClip_T_windowClip;
}

ViewType ControlFrame::viewType() const
{
  return m_viewType;
}

void ControlFrame::setViewType(const ViewType& viewType)
{
  m_viewType = viewType;
}

ViewRenderMode ControlFrame::renderMode() const
{
  return m_renderMode;
}

void ControlFrame::setRenderMode(const ViewRenderMode& shaderType)
{
  m_renderMode = shaderType;
}

IntensityProjectionMode ControlFrame::intensityProjectionMode() const
{
  return m_intensityProjectionMode;
}

void ControlFrame::setIntensityProjectionMode(const IntensityProjectionMode& ipMode)
{
  m_intensityProjectionMode = ipMode;
}

const UiControls& ControlFrame::uiControls() const
{
  return m_uiControls;
}
