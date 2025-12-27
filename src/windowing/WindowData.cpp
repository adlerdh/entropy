#include "windowing/WindowData.h"

#include "common/Exception.hpp"
#include "common/UuidUtility.h"

#include "logic/camera/CameraHelpers.h"
#include "logic/camera/CameraTypes.h"

#include "windowing/ViewTypes.h"

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/string_cast.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <boost/range.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/map.hpp>

#undef min
#undef max

namespace
{
using uuid = uuids::uuid;

Layout createFourUpLayout(
  const CrosshairsState& crosshairs,
  const ViewAlignmentMode& viewAlignment,
  const ViewConvention& viewConvention)
{
  const UiControls uiControls(true);

  const auto noRotationSyncGroup = std::nullopt;
  const auto noTranslationSyncGroup = std::nullopt;
  const auto noZoomSyncGroup = std::nullopt;

  Layout layout(false);

  auto zoomSyncGroupUid = layout.addCameraSyncGroup(CameraSyncMode::Zoom);
  auto* zoomGroup = layout.getCameraSyncGroup(CameraSyncMode::Zoom, zoomSyncGroupUid);

  ViewOffsetSetting offsetSetting;
  offsetSetting.m_offsetMode = ViewOffsetMode::None;

  {
    // top right
    auto view = std::make_unique<View>(
      glm::vec4{0.0f, 0.0f, 1.0f, 1.0f},
      offsetSetting,
      ViewType::Coronal,
      ViewRenderMode::Image,
      IntensityProjectionMode::None,
      uiControls,
      viewConvention,
      crosshairs,
      viewAlignment,
      noRotationSyncGroup,
      noTranslationSyncGroup,
      zoomSyncGroupUid);

    view->setPreferredDefaultRenderedImages({});
    view->setDefaultRenderAllImages(true);
    zoomGroup->push_back(view->uid());
    layout.addView(std::move(view));
  }
  {
    // top left
    auto view = std::make_unique<View>(
      glm::vec4{-1.0f, 0.0f, 1.0f, 1.0f},
      offsetSetting,
      ViewType::Sagittal,
      ViewRenderMode::Image,
      IntensityProjectionMode::None,
      uiControls,
      viewConvention,
      crosshairs,
      viewAlignment,
      noRotationSyncGroup,
      noTranslationSyncGroup,
      zoomSyncGroupUid);

    view->setPreferredDefaultRenderedImages({});
    view->setDefaultRenderAllImages(true);
    zoomGroup->push_back(view->uid());
    layout.addView(std::move(view));
  }
  {
    // bottom left
    auto view = std::make_unique<View>(
      glm::vec4{-1.0f, -1.0f, 1.0f, 1.0f},
      offsetSetting,
      ViewType::ThreeD,
      ViewRenderMode::VolumeRender,
      IntensityProjectionMode::None,
      uiControls,
      viewConvention,
      crosshairs,
      viewAlignment,
      noRotationSyncGroup,
      noTranslationSyncGroup,
      noZoomSyncGroup);

    view->setPreferredDefaultRenderedImages({});
    view->setDefaultRenderAllImages(true);
    zoomGroup->push_back(view->uid());
    layout.addView(std::move(view));
  }
  {
    // bottom right
    auto view = std::make_unique<View>(
      glm::vec4{0.0f, -1.0f, 1.0f, 1.0f},
      offsetSetting,
      ViewType::Axial,
      ViewRenderMode::Image,
      IntensityProjectionMode::None,
      uiControls,
      viewConvention,
      crosshairs,
      viewAlignment,
      noRotationSyncGroup,
      noTranslationSyncGroup,
      zoomSyncGroupUid);

    view->setPreferredDefaultRenderedImages({});
    view->setDefaultRenderAllImages(true);
    zoomGroup->push_back(view->uid());
    layout.addView(std::move(view));
  }

  return layout;
}

Layout createTriLayout(
  const CrosshairsState& crosshairs,
  const ViewAlignmentMode& viewAlignment,
  const ViewConvention& viewConvention)
{
  const UiControls uiControls(true);

  const auto noRotationSyncGroup = std::nullopt;
  const auto noTranslationSyncGroup = std::nullopt;
  const auto noZoomSyncGroup = std::nullopt;

  Layout layout(false);

  auto zoomSyncGroupUid = layout.addCameraSyncGroup(CameraSyncMode::Zoom);
  auto* zoomGroup = layout.getCameraSyncGroup(CameraSyncMode::Zoom, zoomSyncGroupUid);

  ViewOffsetSetting offsetSetting;
  offsetSetting.m_offsetMode = ViewOffsetMode::None;

  {
    // left
    auto view = std::make_unique<View>(
      glm::vec4{-1.0f, -1.0f, 1.5f, 2.0f},
      offsetSetting,
      ViewType::Axial,
      ViewRenderMode::Image,
      IntensityProjectionMode::None,
      uiControls,
      viewConvention,
      crosshairs,
      viewAlignment,
      noRotationSyncGroup,
      noTranslationSyncGroup,
      noZoomSyncGroup);

    view->setPreferredDefaultRenderedImages({});
    view->setDefaultRenderAllImages(true);
    layout.addView(std::move(view));
  }
  {
    // bottom right
    auto view = std::make_unique<View>(
      glm::vec4{0.5f, -1.0f, 0.5f, 1.0f},
      offsetSetting,
      ViewType::Coronal,
      ViewRenderMode::Image,
      IntensityProjectionMode::None,
      uiControls,
      viewConvention,
      crosshairs,
      viewAlignment,
      noRotationSyncGroup,
      noTranslationSyncGroup,
      zoomSyncGroupUid);

    view->setPreferredDefaultRenderedImages({});
    view->setDefaultRenderAllImages(true);
    zoomGroup->push_back(view->uid());
    layout.addView(std::move(view));
  }
  {
    // top right
    auto view = std::make_unique<View>(
      glm::vec4{0.5f, 0.0f, 0.5f, 1.0f},
      offsetSetting,
      ViewType::Sagittal,
      ViewRenderMode::Image,
      IntensityProjectionMode::None,
      uiControls,
      viewConvention,
      crosshairs,
      viewAlignment,
      noRotationSyncGroup,
      noTranslationSyncGroup,
      zoomSyncGroupUid);

    view->setPreferredDefaultRenderedImages({});
    view->setDefaultRenderAllImages(true);
    zoomGroup->push_back(view->uid());
    layout.addView(std::move(view));
  }

  return layout;
}

Layout createTriTopBottomLayout(
  std::size_t numRows,
  const CrosshairsState& crosshairs,
  const ViewAlignmentMode& viewAlignment,
  const ViewConvention& viewConvention)
{
  const UiControls uiControls(true);

  Layout layout(false);

  auto axiRotationSyncGroupUid = layout.addCameraSyncGroup(CameraSyncMode::Rotation);
  auto* axiRotGroup = layout.getCameraSyncGroup(CameraSyncMode::Rotation, axiRotationSyncGroupUid);

  auto corRotationSyncGroupUid = layout.addCameraSyncGroup(CameraSyncMode::Rotation);
  auto* corRotGroup = layout.getCameraSyncGroup(CameraSyncMode::Rotation, corRotationSyncGroupUid);

  auto sagRotationSyncGroupUid = layout.addCameraSyncGroup(CameraSyncMode::Rotation);
  auto* sagRotGroup = layout.getCameraSyncGroup(CameraSyncMode::Rotation, sagRotationSyncGroupUid);

  auto axiTranslationSyncGroupUid = layout.addCameraSyncGroup(CameraSyncMode::Translation);
  auto* axiTransGroup = layout.getCameraSyncGroup(CameraSyncMode::Translation, axiTranslationSyncGroupUid);

  auto corTranslationSyncGroupUid = layout.addCameraSyncGroup(CameraSyncMode::Translation);
  auto* corTransGroup = layout.getCameraSyncGroup(CameraSyncMode::Translation, corTranslationSyncGroupUid);

  auto sagTranslationSyncGroupUid = layout.addCameraSyncGroup(CameraSyncMode::Translation);
  auto* sagTransGroup = layout.getCameraSyncGroup(CameraSyncMode::Translation, sagTranslationSyncGroupUid);

  // Should zoom be synchronized across columns or across rows?
  auto axiZoomSyncGroupUid = layout.addCameraSyncGroup(CameraSyncMode::Zoom);
  auto* axiZoomGroup = layout.getCameraSyncGroup(CameraSyncMode::Zoom, axiZoomSyncGroupUid);

  auto corZoomSyncGroupUid = layout.addCameraSyncGroup(CameraSyncMode::Zoom);
  auto* corZoomGroup = layout.getCameraSyncGroup(CameraSyncMode::Zoom, corZoomSyncGroupUid);

  auto sagZoomSyncGroupUid = layout.addCameraSyncGroup(CameraSyncMode::Zoom);
  auto* sagZoomGroup = layout.getCameraSyncGroup(CameraSyncMode::Zoom, sagZoomSyncGroupUid);

  ViewOffsetSetting offsetSetting;
  offsetSetting.m_offsetMode = ViewOffsetMode::None;

  for (std::size_t r = 0; r < numRows; ++r)
  {
    const float height = 2.0f / static_cast<float>(numRows);
    const float bottom = 1.0f - (static_cast<float>(r + 1)) * height;

    {
      // axial
      auto view = std::make_unique<View>(
        glm::vec4{-1.0f, bottom, 2.0f / 3.0f, height},
        offsetSetting,
        ViewType::Axial,
        ViewRenderMode::Image,
        IntensityProjectionMode::None,
        uiControls,
        viewConvention,
        crosshairs,
        viewAlignment,
        axiRotationSyncGroupUid,
        axiTranslationSyncGroupUid,
        axiZoomSyncGroupUid);

      view->setPreferredDefaultRenderedImages({r});
      view->setDefaultRenderAllImages(false);

      axiRotGroup->push_back(view->uid());
      axiTransGroup->push_back(view->uid());
      axiZoomGroup->push_back(view->uid());

      layout.addView(std::move(view));
    }

    {
      // coronal
      auto view = std::make_unique<View>(
        glm::vec4{-1.0f / 3.0f, bottom, 2.0f / 3.0f, height},
        offsetSetting,
        ViewType::Coronal,
        ViewRenderMode::Image,
        IntensityProjectionMode::None,
        uiControls,
        viewConvention,
        crosshairs,
        viewAlignment,
        corRotationSyncGroupUid,
        corTranslationSyncGroupUid,
        corZoomSyncGroupUid);

      view->setPreferredDefaultRenderedImages({r});
      view->setDefaultRenderAllImages(false);

      corRotGroup->push_back(view->uid());
      corTransGroup->push_back(view->uid());
      corZoomGroup->push_back(view->uid());

      layout.addView(std::move(view));
    }
    {
      // sagittal
      auto view = std::make_unique<View>(
        glm::vec4{1.0f / 3.0f, bottom, 2.0f / 3.0f, height},
        offsetSetting,
        ViewType::Sagittal,
        ViewRenderMode::Image,
        IntensityProjectionMode::None,
        uiControls,
        viewConvention,
        crosshairs,
        viewAlignment,
        sagRotationSyncGroupUid,
        sagTranslationSyncGroupUid,
        sagZoomSyncGroupUid);

      view->setPreferredDefaultRenderedImages({r});
      view->setDefaultRenderAllImages(false);

      sagRotGroup->push_back(view->uid());
      sagTransGroup->push_back(view->uid());
      sagZoomGroup->push_back(view->uid());

      layout.addView(std::move(view));
    }
  }

  return layout;
}

Layout createGridLayout(
  const ViewType& viewType,
  std::size_t width,
  std::size_t height,
  bool offsetViews,
  bool isLightbox,
  const CrosshairsState& crosshairs,
  const ViewAlignmentMode& viewAlignment,
  const ViewConvention& viewConvention,
  const std::optional<size_t>& imageIndexForLightbox,
  const std::optional<uuid>& imageUidForLightbox)
{
  static const ViewRenderMode s_shaderType = ViewRenderMode::Image;
  static const IntensityProjectionMode s_ipMode = IntensityProjectionMode::None;

  Layout layout(isLightbox);

  if (isLightbox) {
    layout.setViewType(viewType);
    layout.setRenderMode(s_shaderType);
    layout.setIntensityProjectionMode(s_ipMode);

    layout.setPreferredDefaultRenderedImages({imageIndexForLightbox ? *imageIndexForLightbox : 0});
    layout.setDefaultRenderAllImages(false);
  }

  const auto rotSyncGroupUid = layout.addCameraSyncGroup(CameraSyncMode::Rotation);
  auto* rotGroup =layout.getCameraSyncGroup(CameraSyncMode::Rotation, rotSyncGroupUid);

  const auto transSyncGroupUid = layout.addCameraSyncGroup(CameraSyncMode::Translation);
  auto* transGroup =layout.getCameraSyncGroup(CameraSyncMode::Translation, transSyncGroupUid);

  const auto zoomSyncGroupUid = layout.addCameraSyncGroup(CameraSyncMode::Zoom);
  auto* zoomGroup =layout.getCameraSyncGroup(CameraSyncMode::Zoom, zoomSyncGroupUid);

  const float w = 2.0f / static_cast<float>(width);
  const float h = 2.0f / static_cast<float>(height);

  std::size_t count = 0;

  ViewOffsetSetting offsetSetting;
  offsetSetting.m_offsetImage = imageUidForLightbox;

  if (imageIndexForLightbox)
  {
    if (0 == *imageIndexForLightbox) {
      offsetSetting.m_offsetMode = ViewOffsetMode::RelativeToRefImageScrolls;
    }
    else {
      offsetSetting.m_offsetMode = ViewOffsetMode::RelativeToImageScrolls;

      // Need to offset according to reference image scrolls, since the crosshairs
      // always move relative to the reference image
      //            offsetSetting.m_offsetMode = ViewOffsetMode::RelativeToRefImageScrolls;
    }
  }
  else
  {
    offsetSetting.m_offsetMode = ViewOffsetMode::RelativeToImageScrolls;
  }

  for (std::size_t j = 0; j < height; ++j)
  {
    for (std::size_t i = 0; i < width; ++i)
    {
      const float l = -1.0f + static_cast<float>(i) * w;
      const float b = -1.0f + static_cast<float>(j) * h;

      const int counter = static_cast<int>(width * j + i);

      offsetSetting.m_relativeOffsetSteps = (offsetViews)
                                              ? (counter - static_cast<int>(width * height) / 2)
                                              : 0;

      auto view = std::make_unique<View>(
        glm::vec4{l, b, w, h},
        offsetSetting,
        viewType,
        s_shaderType,
        s_ipMode,
        UiControls(!isLightbox),
        viewConvention,
        crosshairs,
        viewAlignment,
        rotSyncGroupUid,
        transSyncGroupUid,
        zoomSyncGroupUid);

      if (!isLightbox) {
        // Make each view render a different image by default:
        view->setPreferredDefaultRenderedImages({count});
        view->setDefaultRenderAllImages(false);
      }

      // Synchronize rotations, translations, and zooms for all views in the layout:
      rotGroup->push_back(view->uid());
      transGroup->push_back(view->uid());
      zoomGroup->push_back(view->uid());

      layout.addView(std::move(view));
      ++count;
    }
  }

  return layout;
}
} // namespace

WindowData::WindowData(const CrosshairsState& crosshairs)
  : m_crosshairs(crosshairs)
  , m_viewport(0, 0, 800, 800)
  , m_windowPos(0, 0)
  , m_windowSize(800, 800)
  , m_framebufferSize(800, 800)
  , m_contentScaleRatio(1.0f, 1.0f)
  , m_currentLayout(0)
{
  setupViews();
  setCurrentLayoutIndex(0);

  setWindowSize(m_windowSize.x, m_windowSize.y);
  setFramebufferSize(m_framebufferSize.x, m_framebufferSize.y);
}

void WindowData::setupViews()
{
  m_layouts.emplace_back(createFourUpLayout(m_crosshairs, m_viewAlignment, m_viewConvention));
  m_layouts.emplace_back(createTriLayout(m_crosshairs, m_viewAlignment, m_viewConvention));

  static constexpr std::size_t refImage = 0;

  m_layouts.emplace_back(
    createGridLayout(ViewType::Axial, 1, 1, false, false,
                     m_crosshairs, m_viewAlignment, m_viewConvention, refImage, std::nullopt));

  updateAllViews();
}

void WindowData::addGridLayout(
  const ViewType& viewType,
  std::size_t width,
  std::size_t height,
  bool offsetViews,
  bool isLightbox,
  std::size_t imageIndexForLightbox,
  const uuid& imageUidForLightbox)
{
  m_layouts.emplace_back(createGridLayout(
    viewType, width, height,
    offsetViews, isLightbox,
    m_crosshairs, m_viewAlignment, m_viewConvention,
    imageIndexForLightbox, imageUidForLightbox));

  updateAllViews();
}

void WindowData::addLightboxLayoutForImage(
  const ViewType& viewType, std::size_t numSlices, std::size_t imageIndex, const uuid& imageUid)
{
  static constexpr bool k_offsetViews = true;
  static constexpr bool k_isLightbox = true;

  const int w = static_cast<int>(std::sqrt(numSlices + 1));
  const auto div = std::div(static_cast<int>(numSlices), w);
  const int h = div.quot + (div.rem > 0 ? 1 : 0);

  addGridLayout(viewType, static_cast<std::size_t>(w), static_cast<std::size_t>(h),
                k_offsetViews, k_isLightbox, imageIndex, imageUid);
}

void WindowData::addAxCorSagLayout(std::size_t numImages)
{
  m_layouts.emplace_back(createTriTopBottomLayout(numImages, m_crosshairs, m_viewAlignment, m_viewConvention));
  updateAllViews();
}

void WindowData::removeLayout(std::size_t index)
{
  if (index >= m_layouts.size()) {
    return;
  }

  m_layouts.erase(std::begin(m_layouts) + static_cast<long>(index));
}

void WindowData::setDefaultRenderedImagesForLayout(Layout& layout, uuid_range_t orderedImageUids)
{
  static constexpr bool s_filterAgainstDefaults = true;

  std::list<uuid> renderedImages;
  std::list<uuid> metricImages;

  std::size_t count = 0;

  for (const auto& uid : orderedImageUids)
  {
    renderedImages.push_back(uid);

    if (count < 2) {
      // By default, compute the metric using the first two images:
      metricImages.push_back(uid);
      ++count;
    }
  }

  if (layout.isLightbox()) {
    layout.setRenderedImages(renderedImages, s_filterAgainstDefaults);
    layout.setMetricImages(metricImages);
    return;
  }

  for (auto& [viewUid, view] : layout.views()) {
    if (view) {
      view->setRenderedImages(renderedImages, s_filterAgainstDefaults);
      view->setMetricImages(metricImages);
    }
  }
}

void WindowData::setDefaultRenderedImagesForAllLayouts(uuid_range_t orderedImageUids)
{
  static constexpr bool s_filterAgainstDefaults = true;

  std::list<uuid> renderedImages;
  std::list<uuid> metricImages;

  std::size_t count = 0;

  for (const auto& uid : orderedImageUids)
  {
    renderedImages.push_back(uid);

    if (count < 2) {
      // By default, compute the metric using the first two images:
      metricImages.push_back(uid);
      ++count;
    }
  }

  for (auto& layout : m_layouts)
  {
    if (layout.isLightbox()) {
      layout.setRenderedImages(renderedImages, s_filterAgainstDefaults);
      layout.setMetricImages(metricImages);
      continue;
    }

    for (auto& [viewUid, view] : layout.views()) {
      if (view) {
        view->setRenderedImages(renderedImages, s_filterAgainstDefaults);
        view->setMetricImages(metricImages);
      }
    }
  }
}

void WindowData::updateImageOrdering(uuid_range_t orderedImageUids)
{
  for (auto& layout : m_layouts)
  {
    if (layout.isLightbox()) {
      layout.updateImageOrdering(orderedImageUids);
      continue;
    }

    for (auto& [viewUid, view] : layout.views()) {
      if (view) {
        view->updateImageOrdering(orderedImageUids);
      }
    }
  }
}

void WindowData::recenterAllViews(
  const glm::vec3& worldCenter,
  const glm::vec3& worldFov,
  bool resetZoom,
  bool resetObliqueOrientation,
  const std::set<uuid>& excludedViews)
{
  for (auto& layout : m_layouts) {
    for (auto& [viewUid, view] : layout.views()) {
      if (excludedViews.contains(viewUid)) {
        continue;
      }

      if (view) {
        recenterView(*view, worldCenter, worldFov, resetZoom, resetObliqueOrientation);
      }
    }
  }
}

void WindowData::recenterView(
  const uuid& viewUid,
  const glm::vec3& worldCenter,
  const glm::vec3& worldFov,
  bool resetZoom,
  bool resetObliqueOrientation)
{
  if (View* view = getView(viewUid)) {
    recenterView(*view, worldCenter, worldFov, resetZoom, resetObliqueOrientation);
  }
}

void WindowData::recenterView(
  View& view,
  const glm::vec3& worldCenter,
  const glm::vec3& worldFov,
  bool resetZoom,
  bool resetObliqueOrientation)
{
  if (resetZoom) {
    helper::resetZoom(view.camera());
  }

  if (resetObliqueOrientation)
  {
    if (ViewType::Oblique == view.viewType()) {
      // Reset the view orientation for oblique views:
      helper::resetViewTransformation(view.camera());
    }
  }

  helper::positionCameraForWorldTargetAndFov(view.camera(), worldFov, worldCenter);
}

uuid_range_t WindowData::currentViewUids() const
{
  return (m_layouts.at(m_currentLayout).views() | boost::adaptors::map_keys);
}

const View* WindowData::getCurrentView(const uuid& uid) const
{
  const auto& views = m_layouts.at(m_currentLayout).views();
  auto it = views.find(uid);
  if (std::end(views) != it) {
    if (it->second) {
      return it->second.get();
    }
  }
  return nullptr;
}

View* WindowData::getCurrentView(const uuid& uid)
{
  auto& views = m_layouts.at(m_currentLayout).views();
  auto it = views.find(uid);
  if (std::end(views) != it) {
    if (it->second) {
      return it->second.get();
    }
  }
  return nullptr;
}

const View* WindowData::getView(const uuid& uid) const
{
  for (const auto& layout : m_layouts)
  {
    auto it = layout.views().find(uid);
    if (std::end(layout.views()) != it) {
      if (it->second) {
        return it->second.get();
      }
    }
  }
  return nullptr;
}

View* WindowData::getView(const uuid& uid)
{
  for (const auto& layout : m_layouts)
  {
    auto it = layout.views().find(uid);
    if (std::end(layout.views()) != it) {
      if (it->second)
        return it->second.get();
    }
  }
  return nullptr;
}

std::optional<uuid> WindowData::currentViewUidAtCursor(const glm::vec2& windowPos) const
{
  if (m_layouts.empty()) {
    return std::nullopt;
  }

  const glm::vec2 winClipPos = helper::windowNdc_T_window(m_viewport, windowPos);

  for (const auto& view : m_layouts.at(m_currentLayout).views())
  {
    if (!view.second) {
      continue;
    }

    const glm::vec4& winClipVp = view.second->windowClipViewport();

    if ((winClipVp[0] <= winClipPos.x ) && (winClipPos.x < winClipVp[0] + winClipVp[2]) &&
        (winClipVp[1] <= winClipPos.y) && (winClipPos.y < winClipVp[1] + winClipVp[3]))
    {
      return view.first;
    }
  }

  return std::nullopt;
}

std::optional<uuid> WindowData::activeViewUid() const
{
  return m_activeViewUid;
}

void WindowData::setActiveViewUid(const std::optional<uuid>& uid)
{
  m_activeViewUid = uid;
}

std::size_t WindowData::numLayouts() const
{
  return m_layouts.size();
}

const std::vector<Layout>& WindowData::layouts() const
{
  return m_layouts;
}

std::size_t WindowData::currentLayoutIndex() const
{
  return m_currentLayout;
}

const Layout* WindowData::layout(std::size_t index) const
{
  if (index < m_layouts.size()) {
    return &(m_layouts.at(index));
  }
  return nullptr;
}

const Layout& WindowData::currentLayout() const
{
  return m_layouts.at(m_currentLayout);
}

Layout& WindowData::currentLayout()
{
  return m_layouts.at(m_currentLayout);
}

void WindowData::setCurrentLayoutIndex(std::size_t index)
{
  if (index >= m_layouts.size()) {
    return;
  }
  m_currentLayout = index;
}

void WindowData::cycleCurrentLayout(int step)
{
  const int i = static_cast<int>(currentLayoutIndex());
  const int N = static_cast<int>(numLayouts());
  setCurrentLayoutIndex(static_cast<size_t>((N + i + step) % N));
}

const Viewport& WindowData::viewport() const
{
  return m_viewport;
}

void WindowData::setViewport(float left, float bottom, float width, float height)
{
  m_viewport.setLeft(left);
  m_viewport.setBottom(bottom);
  m_viewport.setWidth(width);
  m_viewport.setHeight(height);
  updateAllViews();
}

void WindowData::setContentScaleRatios(const glm::vec2& scale)
{
  if (m_contentScaleRatio == scale) {
    return;
  }

  spdlog::trace("Setting content scale ratio to {}x{}", scale.x, scale.y);
  m_contentScaleRatio = scale;
  updateAllViews();
}

const glm::vec2& WindowData::getContentScaleRatios() const
{
  return m_contentScaleRatio;
}

float WindowData::getContentScaleRatio() const
{
  return glm::compMax(m_contentScaleRatio);
}

void WindowData::setWindowPos(int posX, int posY)
{
  m_windowPos = glm::ivec2{posX, posY};
}

const glm::ivec2& WindowData::getWindowPos() const
{
  return m_windowPos;
}

void WindowData::setWindowSize(int width, int height)
{
  static const glm::ivec2 sk_minWindowSize{1, 1};

  if (m_windowSize.x == width && m_windowSize.y == height) {
    return;
  }

  m_windowSize = glm::max(glm::ivec2{width, height}, sk_minWindowSize);
  m_viewport.setDevicePixelRatio(computeFramebufferToWindowRatio());
  updateAllViews();
}

const glm::ivec2& WindowData::getWindowSize() const
{
  return m_windowSize;
}

void WindowData::setFramebufferSize(int width, int height)
{
  static const glm::ivec2 sk_minFramebufferSize{1, 1};

  if (m_framebufferSize.x == width && m_framebufferSize.y == height) {
    return;
  }

  m_framebufferSize = glm::max(glm::ivec2{width, height}, sk_minFramebufferSize);
  m_viewport.setDevicePixelRatio(computeFramebufferToWindowRatio());
  updateAllViews();
}

const glm::ivec2& WindowData::getFramebufferSize() const
{
  return m_framebufferSize;
}

glm::vec2 WindowData::computeFramebufferToWindowRatio() const
{
  return glm::vec2{
    static_cast<float>(m_framebufferSize.x) / static_cast<float>(m_windowSize.x),
    static_cast<float>(m_framebufferSize.y) / static_cast<float>(m_windowSize.y)};
}

void WindowData::setViewOrientationConvention(const ViewConvention& convention)
{
  m_viewConvention = convention;
}

ViewConvention WindowData::getViewOrientationConvention() const
{
  return m_viewConvention;
}

ViewAlignmentMode WindowData::viewAlignmentMode() const
{
  return m_viewAlignment;
}

void WindowData::setViewAlignmentMode(ViewAlignmentMode mode)
{
  m_viewAlignment = mode;
}

uuid_range_t WindowData::cameraSyncGroupViewUids(
  CameraSyncMode mode, const uuid& syncGroupUid) const
{
  const auto& currentLayout = m_layouts.at(m_currentLayout);
  if (const auto* group = currentLayout.getCameraSyncGroup(mode, syncGroupUid)) {
    return *group;
  }
  return {};
}

void WindowData::applyImageSelectionToAllCurrentViews(const uuid& referenceViewUid)
{
  static constexpr bool s_filterAgainstDefaults = false;

  const View* referenceView = getCurrentView(referenceViewUid);
  if (!referenceView) {
    return;
  }

  const auto renderedImages = referenceView->renderedImages();
  const auto metricImages = referenceView->metricImages();

  for (auto& viewUid : currentViewUids())
  {
    View* view = getCurrentView(viewUid);
    if (!view) {
      continue;
    }

    view->setRenderedImages(renderedImages, s_filterAgainstDefaults);
    view->setMetricImages(metricImages);
  }
}

void WindowData::applyViewRenderModeAndProjectionToAllCurrentViews(
  const uuid& referenceViewUid)
{
  const View* referenceView = getCurrentView(referenceViewUid);
  if (!referenceView) {
    return;
  }

  const auto renderMode = referenceView->renderMode();
  const auto ipMode = referenceView->intensityProjectionMode();

  for (auto& viewUid : currentViewUids())
  {
    View* view = getCurrentView(viewUid);
    if (!view) {
      continue;
    }

    if (ViewType::ThreeD != view->viewType()) {
      // Don't allow changing render mode of 3D views
      view->setRenderMode(renderMode);
    }

    view->setIntensityProjectionMode(ipMode);
  }
}

std::vector<uuid> WindowData::findCurrentViewsWithNormal(const glm::vec3& worldNormal) const
{
  // Angle threshold (in degrees) for checking whether two vectors are parallel
  static constexpr float sk_parallelThreshold_degrees = 0.1f;

  std::vector<uuid> viewUids;

  for (auto& viewUid : currentViewUids())
  {
    const View* view = getCurrentView(viewUid);
    if (!view) {
      continue;
    }

    const glm::vec3 viewBackDir = helper::worldDirection(view->camera(), Directions::View::Back);
    if (helper::areVectorsParallel(worldNormal, viewBackDir, sk_parallelThreshold_degrees)) {
      viewUids.push_back(viewUid);
    }
  }

  return viewUids;
}

uuid WindowData::findLargestCurrentView() const
{
  uuid largestViewUid = currentViewUids().front();
  const View* largestView = getCurrentView(largestViewUid);

  if (!largestView) {
    spdlog::error("The current layout has no views");
    throw_debug("The current layout has no views")
  }

  float largestArea = largestView->windowClipViewport()[2] * largestView->windowClipViewport()[3];

  for (auto& viewUid : currentViewUids())
  {
    const View* view = getCurrentView(viewUid);
    if (!view) {
      continue;
    }

    const float area = view->windowClipViewport()[2] * view->windowClipViewport()[3];
    if (area > largestArea) {
      largestArea = area;
      largestViewUid = viewUid;
    }
  }

  return largestViewUid;
}

void WindowData::recomputeCameraAspectRatios()
{
  for (auto& layout : m_layouts)
  {
    for (auto& [viewUid, view] : layout.views())
    {
      if (!view) {
        continue;
      }

      // The view camera's aspect ratio is the product of the main window's
      // aspect ratio and the view's aspect ratio:
      const float h = view->windowClipViewport()[3];

      if (glm::epsilonEqual(h, 0.0f, glm::epsilon<float>())) {
        spdlog::error("View {} has zero height: setting it to 1.", viewUid);
        view->setWindowClipViewport(glm::vec4{glm::vec3{view->windowClipViewport()}, 1.0f});
      }

      const float viewAspect = view->windowClipViewport()[2] / view->windowClipViewport()[3];
      view->camera().setAspectRatio(m_viewport.aspectRatio() * viewAspect);
    }
  }
}

void WindowData::updateAllViews()
{
  recomputeCameraAspectRatios();
}
