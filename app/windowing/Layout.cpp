#include "windowing/Layout.h"
#include "common/UuidUtility.h"
#include "logic/app/Data.h"
#include "ui/UiControls.h"
#include "viewer/ViewModes.h"
#include "viewer/ViewTypes.h"

#include <set>
#include <stdexcept>
#include <utility>

namespace
{
using uuid = uuids::uuid;

// Viewport of a full window, defined in window Clip space:
static const glm::vec4 sk_winClipFullWindowViewport{-1.0f, -1.0f, 2.0f, 2.0f};

ViewRenderMode reconcileRenderModeForViewType(ViewType viewType, ViewRenderMode renderMode)
{
  if (ViewType::ThreeD == viewType) {
    return ViewRenderMode::VolumeRender;
  }

  return ViewRenderMode::VolumeRender == renderMode ? ViewRenderMode::Image : renderMode;
}
} // namespace

Layout::Layout(bool isLightbox)
  : ControlFrame(
      sk_winClipFullWindowViewport,
      ViewType::Axial,
      ViewRenderMode::Image,
      IntensityProjectionMode::None,
      UiControls(isLightbox))
  , m_uid(generateRandomUuid())
  , m_isLightbox(isLightbox)

{
  // Render the first image by default (and do not render all images):
  setPreferredDefaultRenderedImages({0});
  setDefaultRenderAllImages(false);
}

void Layout::replaceContentsPreservingUid(Layout&& other) noexcept
{
  const uuid uid = m_uid;
  *this = std::move(other);
  m_uid = uid;
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

void Layout::setImageVolumeRendered(const AppData& appData, std::size_t index, bool visible)
{
  ControlFrame::setImageVolumeRendered(appData, index, visible);
  updateAllViewsInLayout();
}

void Layout::setVolumeRenderedImages(const std::list<uuid>& imageUids)
{
  ControlFrame::setVolumeRenderedImages(imageUids);
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

void Layout::updateImageOrdering(const uuid_range_t& orderedImageUids)
{
  ControlFrame::updateImageOrdering(orderedImageUids);
  updateAllViewsInLayout();
}

void Layout::setViewType(const ViewType& viewType)
{
  ControlFrame::setViewType(viewType);
  ControlFrame::setRenderMode(reconcileRenderModeForViewType(viewType, renderMode()));
  updateAllViewsInLayout();
}

void Layout::setRenderMode(const ViewRenderMode& renderMode)
{
  ControlFrame::setRenderMode(reconcileRenderModeForViewType(viewType(), renderMode));
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
      view->setRenderedImages(renderedImages(), false);
      view->setVolumeRenderedImages(volumeRenderedImages());
      view->setMetricImages(metricImages());
      view->setViewType(m_viewType);
      view->setRenderMode(m_renderMode);
      view->setIntensityProjectionMode(m_intensityProjectionMode);
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

LayoutKind Layout::kind() const
{
  return m_kind;
}

void Layout::setKind(LayoutKind kind)
{
  m_kind = kind;
}

const std::string& Layout::displayName() const
{
  return m_displayName;
}

void Layout::setDisplayName(std::string displayName)
{
  m_displayName = displayName.empty() ? std::string{"Custom"} : std::move(displayName);
}

bool Layout::addView(std::unique_ptr<View> view)
{
  if (!view) {
    throw std::invalid_argument("Cannot add null view");
  }
  const uuid viewUid = view->uid();
  const bool inserted = m_views.emplace(viewUid, std::move(view)).second;
  if (inserted) {
    m_orderedViewUids.push_back(viewUid);
  }
  return inserted;
}

const std::unordered_map<uuid, std::unique_ptr<View>>& Layout::views() const
{
  return m_views;
}

const std::vector<uuid>& Layout::orderedViewUids() const
{
  return m_orderedViewUids;
}

std::vector<View*> Layout::orderedViews()
{
  std::vector<View*> views;
  views.reserve(m_orderedViewUids.size());
  for (const auto& viewUid : m_orderedViewUids) {
    auto it = m_views.find(viewUid);
    if (it != m_views.end() && it->second) {
      views.push_back(it->second.get());
    }
  }
  return views;
}

std::vector<const View*> Layout::orderedViews() const
{
  std::vector<const View*> views;
  views.reserve(m_orderedViewUids.size());
  for (const auto& viewUid : m_orderedViewUids) {
    auto it = m_views.find(viewUid);
    if (it != m_views.end() && it->second) {
      views.push_back(it->second.get());
    }
  }
  return views;
}

uuid Layout::addCameraSyncGroup(CameraSyncMode mode)
{
  return m_cameraSyncGroups.addGroup(mode);
}

const std::list<uuid>* Layout::getCameraSyncGroup(CameraSyncMode mode, const uuid& groupUid) const
{
  return m_cameraSyncGroups.group(mode, groupUid);
}

std::list<uuid>* Layout::getCameraSyncGroup(CameraSyncMode mode, const uuid& groupUid)
{
  return m_cameraSyncGroups.group(mode, groupUid);
}

std::optional<uuid> Layout::cameraSyncGroupUidContainingView(CameraSyncMode mode, const uuid& viewUid) const
{
  return m_cameraSyncGroups.groupUidContainingView(mode, viewUid);
}

void Layout::addViewToCameraSyncGroup(CameraSyncMode mode, const std::optional<uuid>& groupUid, const uuid& viewUid)
{
  m_cameraSyncGroups.addViewToGroup(mode, groupUid, viewUid);
}
