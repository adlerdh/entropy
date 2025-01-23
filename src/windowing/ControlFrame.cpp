#include "windowing/ControlFrame.h"

#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"

#include <glm/glm.hpp>

ControlFrame::ControlFrame(
  glm::vec4 winClipViewport,
  ViewType viewType,
  camera::ViewRenderMode renderMode,
  camera::IntensityProjectionMode ipMode,
  UiControls uiControls
)
  : m_winClipViewport(std::move(winClipViewport))
  ,

  m_windowClip_T_viewClip(camera::compute_windowClip_T_viewClip(m_winClipViewport))
  , m_viewClip_T_windowClip(glm::inverse(m_windowClip_T_viewClip))
  ,

  m_renderedImageUids()
  , m_metricImageUids()
  ,

  // Don't specify images to render by default:
  m_preferredDefaultRenderedImages({})
  ,

  // Render all images by default in the frame:
  m_defaultRenderAllImages(true)
  ,

  m_viewType(viewType)
  , m_renderMode(renderMode)
  , m_intensityProjectionMode(ipMode)
  ,

  m_uiControls(std::move(uiControls))
{
}

bool ControlFrame::isImageRendered(const AppData& appData, size_t index)
{
  auto imageUid = appData.imageUid(index);
  if (!imageUid)
    return false; // invalid image index

  return isImageRendered(*imageUid);
}

bool ControlFrame::isImageRendered(const uuids::uuid& imageUid)
{
  auto it = std::find(std::begin(m_renderedImageUids), std::end(m_renderedImageUids), imageUid);

  return (std::end(m_renderedImageUids) != it);
}

void ControlFrame::setImageRendered(const AppData& appData, size_t index, bool visible)
{
  auto imageUid = appData.imageUid(index);
  if (!imageUid)
    return; // invalid image index

  setImageRendered(appData, *imageUid, visible);
}

void ControlFrame::setImageRendered(const AppData& appData, const uuids::uuid& imageUid, bool visible)
{
  if (!visible)
  {
    m_renderedImageUids.remove(imageUid);
    return;
  }

  if (std::end(m_renderedImageUids) != std::find(std::begin(m_renderedImageUids), std::end(m_renderedImageUids), imageUid))
  {
    return; // image already exists, so do nothing
  }

  const auto imageIndex = appData.imageIndex(imageUid);
  if (!imageIndex)
    return; // invalid image index

  bool inserted = false;

  for (auto it = std::begin(m_renderedImageUids); std::end(m_renderedImageUids) != it; ++it)
  {
    if (const auto i = appData.imageIndex(*it))
    {
      if (*imageIndex < *i)
      {
        // Insert the desired image in the right place
        m_renderedImageUids.insert(it, imageUid);
        inserted = true;
        break;
      }
    }
  }

  if (!inserted)
  {
    m_renderedImageUids.push_back(imageUid);
  }
}

const std::list<uuids::uuid>& ControlFrame::renderedImages() const
{
  return m_renderedImageUids;
}

void ControlFrame::setRenderedImages(const std::list<uuids::uuid>& imageUids, bool filterByDefaults)
{
  if (filterByDefaults && !m_defaultRenderAllImages)
  {
    m_renderedImageUids.clear();
    size_t index = 0;

    for (const auto& imageUid : imageUids)
    {
      if (m_preferredDefaultRenderedImages.count(index) > 0)
      {
        m_renderedImageUids.push_back(imageUid);
      }
      ++index;
    }
  }
  else
  {
    m_renderedImageUids = imageUids;
  }
}

bool ControlFrame::isImageUsedForMetric(const AppData& appData, size_t index)
{
  auto imageUid = appData.imageUid(index);
  if (!imageUid)
    return false; // invalid image index

  return isImageUsedForMetric(*imageUid);
}

bool ControlFrame::isImageUsedForMetric(const uuids::uuid& imageUid)
{
  auto it = std::find(std::begin(m_metricImageUids), std::end(m_metricImageUids), imageUid);

  return (std::end(m_metricImageUids) != it);
}

void ControlFrame::setImageUsedForMetric(const AppData& appData, size_t index, bool visible)
{
  // Maximum number of images that are used for metric computations in a view
  static constexpr size_t MAX_METRIC_IMAGES = 2;

  auto imageUid = appData.imageUid(index);
  if (!imageUid)
    return; // invalid image index

  if (!visible)
  {
    m_metricImageUids.remove(*imageUid);
    return;
  }

  if (std::end(m_metricImageUids) != std::find(std::begin(m_metricImageUids), std::end(m_metricImageUids), *imageUid))
  {
    return; // image already exists, so do nothing
  }

  if (m_metricImageUids.size() >= MAX_METRIC_IMAGES)
  {
    // If trying to add another image UID to list with 2 or more UIDs,
    // remove the last UID to make room:
    m_metricImageUids.erase(std::prev(std::end(m_metricImageUids)));
  }

  bool inserted = false;

  for (auto it = std::begin(m_metricImageUids); std::end(m_metricImageUids) != it; ++it)
  {
    if (const auto i = appData.imageIndex(*it))
    {
      if (index < *i)
      {
        // Insert the desired image in the right place
        m_metricImageUids.insert(it, *imageUid);
        inserted = true;
        break;
      }
    }
  }

  if (!inserted)
  {
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

  switch (m_renderMode)
  {
  case camera::ViewRenderMode::Image:
  {
    return renderedImages();
  }
  case camera::ViewRenderMode::Disabled:
  {
    return sk_noImages;
  }
  default:
  {
    return metricImages();
  }
  }
}

void ControlFrame::updateImageOrdering(uuid_range_t orderedImageUids)
{
  std::list<uuids::uuid> newRenderedImageUids;
  std::list<uuids::uuid> newMetricImageUids;

  // Loop through the images in new order:
  for (const auto& imageUid : orderedImageUids)
  {
    auto it1 = std::find(std::begin(m_renderedImageUids), std::end(m_renderedImageUids), imageUid);

    if (std::end(m_renderedImageUids) != it1)
    {
      // This image is rendered, so place in new order:
      newRenderedImageUids.push_back(imageUid);
    }

    auto it2 = std::find(std::begin(m_metricImageUids), std::end(m_metricImageUids), imageUid);

    if (std::end(m_metricImageUids) != it2 && newMetricImageUids.size() < 2)
    {
      // This image is in metric computation, so place in new order:
      newMetricImageUids.push_back(imageUid);
    }
  }

  m_renderedImageUids = newRenderedImageUids;
  m_metricImageUids = newMetricImageUids;
}

void ControlFrame::setPreferredDefaultRenderedImages(std::set<size_t> imageIndices)
{
  m_preferredDefaultRenderedImages = std::move(imageIndices);
}

const std::set<size_t>& ControlFrame::preferredDefaultRenderedImages() const
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

camera::ViewRenderMode ControlFrame::renderMode() const
{
  return m_renderMode;
}
void ControlFrame::setRenderMode(const camera::ViewRenderMode& shaderType)
{
  m_renderMode = shaderType;
}

camera::IntensityProjectionMode ControlFrame::intensityProjectionMode() const
{
  return m_intensityProjectionMode;
}
void ControlFrame::setIntensityProjectionMode(const camera::IntensityProjectionMode& ipMode)
{
  m_intensityProjectionMode = ipMode;
}

const UiControls& ControlFrame::uiControls() const
{
  return m_uiControls;
}
