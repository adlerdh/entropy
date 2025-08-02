#include "windowing/Layout.h"
#include "common/UuidUtility.h"
#include "logic/app/Data.h"

namespace
{
using uuid = uuids::uuid;

// Viewport of a full window, defined in window Clip space:
static const glm::vec4 sk_winClipFullWindowViewport{-1.0f, -1.0f, 2.0f, 2.0f};
} // namespace

Layout::Layout(bool isLightbox)
  : ControlFrame(sk_winClipFullWindowViewport, ViewType::Axial, ViewRenderMode::Image,
                 IntensityProjectionMode::None, UiControls(isLightbox))
  , m_uid(generateRandomUuid())
  , m_isLightbox(isLightbox)
  , m_views()
  , m_cameraSyncGroups{
      {CameraSyncMode::Rotation, {}},
      {CameraSyncMode::Translation, {}},
      {CameraSyncMode::Zoom, {}}}
{
  // Render the first image by default (and do not render all images):
  m_preferredDefaultRenderedImages = {0};
  m_defaultRenderAllImages = false;
}

Layout::Layout(Layout&& other) noexcept
  :
  ControlFrame(std::move(other)), // move base class
  m_uid(std::move(other.m_uid)),
  m_isLightbox(other.m_isLightbox),
  m_views(std::move(other.m_views)),
  m_cameraSyncGroups(std::move(other.m_cameraSyncGroups))
{}

Layout& Layout::operator=(Layout&& other) noexcept {
  if (this != &other) {
    ControlFrame::operator=(std::move(other)); // move-assign base class
    m_uid = std::move(other.m_uid);
    m_isLightbox = other.m_isLightbox;
    m_views = std::move(other.m_views);
    m_cameraSyncGroups = std::move(other.m_cameraSyncGroups);
  }
  return *this;
}

void Layout::setImageRendered(const AppData& appData, std::size_t index, bool visible)
{
  ControlFrame::setImageRendered(appData, index, visible);
  updateAllViewsInLayout();
}

void Layout::setRenderedImages(const std::list<uuid>& imageUids, bool filterByDefaults)
{
  ControlFrame::setRenderedImages(imageUids, filterByDefaults);
  updateAllViewsInLayout();
}

void Layout::setMetricImages(const std::list<uuid>& imageUids)
{
  ControlFrame::setMetricImages(imageUids);
  updateAllViewsInLayout();
}

void Layout::setImageUsedForMetric(const AppData& appData, std::size_t index, bool visible)
{
  ControlFrame::setImageUsedForMetric(appData, index, visible);
  updateAllViewsInLayout();
}

void Layout::updateImageOrdering(uuid_range_t orderedImageUids)
{
  ControlFrame::updateImageOrdering(orderedImageUids);
  updateAllViewsInLayout();
}

void Layout::setViewType(const ViewType& viewType)
{
  ControlFrame::setViewType(viewType);
  updateAllViewsInLayout();
}

void Layout::setRenderMode(const ViewRenderMode& renderMode)
{
  ControlFrame::setRenderMode(renderMode);
  updateAllViewsInLayout();
}

void Layout::setIntensityProjectionMode(const IntensityProjectionMode& ipMode)
{
  ControlFrame::setIntensityProjectionMode(ipMode);
  updateAllViewsInLayout();
}

void Layout::updateAllViewsInLayout()
{
  for (auto& [viewUid, view] : m_views) {
    if (view) {
      view->setRenderedImages(m_renderedImageUids, false);
      view->setMetricImages(m_metricImageUids);
      view->setViewType(m_viewType);
      view->setRenderMode(m_renderMode);
    }
  }
}

const uuid& Layout::uid() const
{
  return m_uid;
}

bool Layout::isLightbox() const
{
  return m_isLightbox;
}

bool Layout::addView(std::unique_ptr<View> view) {
  if (!view) {
    throw std::invalid_argument("Cannot add null view");
  }
  return m_views.emplace(view->uid(), std::move(view)).second;
}

const std::unordered_map<uuid, std::unique_ptr<View>>& Layout::views() const
{
  return m_views;
}

uuid Layout::addCameraSyncGroup(CameraSyncMode mode)
{
  uuid newUid = generateRandomUuid();
  while (!m_cameraSyncGroups.at(mode).try_emplace(newUid).second) {
    newUid = generateRandomUuid();
  }
  return newUid;
}

const std::list<uuid>* Layout::getCameraSyncGroup(CameraSyncMode mode, const uuid& groupUid) const
{
  auto it = m_cameraSyncGroups.at(mode).find(groupUid);
  return it != m_cameraSyncGroups.at(mode).end() ? &it->second : nullptr;
}

std::list<uuid>* Layout::getCameraSyncGroup(CameraSyncMode mode, const uuid& groupUid)
{
  auto it = m_cameraSyncGroups.at(mode).find(groupUid);
  return it != m_cameraSyncGroups.at(mode).end() ? &it->second : nullptr;
}
