#include "windowing/ControlFrame.h"

#include "logic/app/Data.h"

#include <optional>
#include <utility>

ControlFrame::ControlFrame(
  glm::vec4 winClipViewport,
  ViewType viewType,
  ViewRenderMode renderMode,
  IntensityProjectionMode ipMode,
  UiControls uiControls)
  : m_viewport(winClipViewport)
  , m_imageSelection()
  , m_viewType(viewType)
  , m_renderMode(renderMode)
  , m_intensityProjectionMode(ipMode)
  , m_uiControls(uiControls)
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
  return m_imageSelection.isImageRendered(imageUid);
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
  m_imageSelection.setImageRendered(imageUid, appData.imageUidsOrdered(), visible);
}

const std::list<uuids::uuid>& ControlFrame::renderedImages() const
{
  return m_imageSelection.renderedImages();
}

void ControlFrame::setRenderedImages(const std::list<uuids::uuid>& imageUids, bool filterByDefaults)
{
  m_imageSelection.setRenderedImages(imageUids, filterByDefaults);
  if (ViewRenderMode::VolumeRender == m_renderMode) {
    m_imageSelection.ensureVolumeRenderedImageSelected();
  }
}

bool ControlFrame::isImageVolumeRendered(const AppData& appData, std::size_t index)
{
  auto imageUid = appData.imageUid(index);
  if (!imageUid) {
    return false;
  }
  return isImageVolumeRendered(*imageUid);
}

bool ControlFrame::isImageVolumeRendered(const uuids::uuid& imageUid)
{
  return m_imageSelection.isImageVolumeRendered(imageUid);
}

void ControlFrame::setImageVolumeRendered(const AppData& appData, std::size_t index, bool visible)
{
  auto imageUid = appData.imageUid(index);
  if (!imageUid) {
    return;
  }
  setImageVolumeRendered(appData, *imageUid, visible);
}

void ControlFrame::setImageVolumeRendered(const AppData& appData, const uuids::uuid& imageUid, bool visible)
{
  m_imageSelection.setImageVolumeRendered(imageUid, appData.imageUidsOrdered(), visible);
}

const std::list<uuids::uuid>& ControlFrame::volumeRenderedImages() const
{
  return m_imageSelection.volumeRenderedImages();
}

void ControlFrame::setVolumeRenderedImages(const std::list<uuids::uuid>& imageUids)
{
  m_imageSelection.setVolumeRenderedImages(imageUids);
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
  return m_imageSelection.isImageUsedForMetric(imageUid);
}

void ControlFrame::setImageUsedForMetric(const AppData& appData, std::size_t index, bool visible)
{
  auto imageUid = appData.imageUid(index);
  if (!imageUid) {
    return; // invalid image index
  }

  m_imageSelection.setImageUsedForMetric(*imageUid, appData.imageUidsOrdered(), visible);
}

const std::list<uuids::uuid>& ControlFrame::metricImages() const
{
  return m_imageSelection.metricImages();
}

void ControlFrame::setMetricImages(const std::list<uuids::uuid>& imageUids)
{
  m_imageSelection.setMetricImages(imageUids);
}

const std::list<uuids::uuid>& ControlFrame::visibleImages() const
{
  return m_imageSelection.visibleImages(m_renderMode);
}

void ControlFrame::updateImageOrdering(const uuid_range_t& orderedImageUids)
{
  m_imageSelection.updateImageOrdering(orderedImageUids);
}

void ControlFrame::setPreferredDefaultRenderedImages(std::set<std::size_t> imageIndices)
{
  m_imageSelection.setPreferredDefaultRenderedImages(std::move(imageIndices));
}

const std::set<std::size_t>& ControlFrame::preferredDefaultRenderedImages() const
{
  return m_imageSelection.preferredDefaultRenderedImages();
}

void ControlFrame::setDefaultRenderAllImages(bool renderAll)
{
  m_imageSelection.setDefaultRenderAllImages(renderAll);
}

bool ControlFrame::defaultRenderAllImages() const
{
  return m_imageSelection.defaultRenderAllImages();
}

void ControlFrame::setWindowClipViewport(glm::vec4 winClipViewport)
{
  m_viewport.setWindowClipViewport(winClipViewport);
}

const glm::vec4& ControlFrame::windowClipViewport() const
{
  return m_viewport.windowClipViewport();
}

const glm::mat4& ControlFrame::windowClip_T_viewClip() const
{
  return m_viewport.windowClip_T_viewClip();
}

const glm::mat4& ControlFrame::viewClip_T_windowClip() const
{
  return m_viewport.viewClip_T_windowClip();
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
  if (ViewRenderMode::VolumeRender == m_renderMode) {
    m_imageSelection.ensureVolumeRenderedImageSelected();
  }
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
