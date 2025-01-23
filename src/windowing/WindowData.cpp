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

Layout createFourUpLayout(std::function<ViewConvention()> conventionProvider)
{
  using namespace camera;

  const UiControls uiControls(true);

  const auto noRotationSyncGroup = std::nullopt;
  const auto noTranslationSyncGroup = std::nullopt;
  const auto noZoomSyncGroup = std::nullopt;

  const auto zoomSyncGroupUid = generateRandomUuid();

  Layout layout(false);

  auto zoomGroup = layout.cameraZoomSyncGroups().try_emplace(zoomSyncGroupUid).first;

  ViewOffsetSetting offsetSetting;
  offsetSetting.m_offsetMode = ViewOffsetMode::None;

  {
    // top right
    auto view = std::make_shared<View>(
      glm::vec4{0.0f, 0.0f, 1.0f, 1.0f},
      offsetSetting,
      ViewType::Coronal,
      ViewRenderMode::Image,
      IntensityProjectionMode::None,
      uiControls,
      conventionProvider,
      noRotationSyncGroup,
      noTranslationSyncGroup,
      zoomSyncGroupUid
    );

    view->setPreferredDefaultRenderedImages({});
    view->setDefaultRenderAllImages(true);

    const auto viewUid = generateRandomUuid();
    layout.views().emplace(viewUid, std::move(view));
    zoomGroup->second.push_back(viewUid);
  }
  {
    // top left
    auto view = std::make_shared<View>(
      glm::vec4{-1.0f, 0.0f, 1.0f, 1.0f},
      offsetSetting,
      ViewType::Sagittal,
      ViewRenderMode::Image,
      IntensityProjectionMode::None,
      uiControls,
      conventionProvider,
      noRotationSyncGroup,
      noTranslationSyncGroup,
      zoomSyncGroupUid
    );

    view->setPreferredDefaultRenderedImages({});
    view->setDefaultRenderAllImages(true);

    const auto viewUid = generateRandomUuid();
    layout.views().emplace(viewUid, std::move(view));
    zoomGroup->second.push_back(viewUid);
  }
  {
    // bottom left
    auto view = std::make_shared<View>(
      glm::vec4{-1.0f, -1.0f, 1.0f, 1.0f},
      offsetSetting,
      ViewType::ThreeD,
      ViewRenderMode::Disabled,
      IntensityProjectionMode::None,
      uiControls,
      conventionProvider,
      noRotationSyncGroup,
      noTranslationSyncGroup,
      noZoomSyncGroup
    );

    view->setPreferredDefaultRenderedImages({});
    view->setDefaultRenderAllImages(true);

    const auto viewUid = generateRandomUuid();
    layout.views().emplace(viewUid, std::move(view));
    zoomGroup->second.push_back(viewUid);
  }
  {
    // bottom right
    auto view = std::make_shared<View>(
      glm::vec4{0.0f, -1.0f, 1.0f, 1.0f},
      offsetSetting,
      ViewType::Axial,
      ViewRenderMode::Image,
      IntensityProjectionMode::None,
      uiControls,
      conventionProvider,
      noRotationSyncGroup,
      noTranslationSyncGroup,
      zoomSyncGroupUid
    );

    view->setPreferredDefaultRenderedImages({});
    view->setDefaultRenderAllImages(true);

    const auto viewUid = generateRandomUuid();
    layout.views().emplace(viewUid, std::move(view));
    zoomGroup->second.push_back(viewUid);
  }

  return layout;
}

Layout createTriLayout(std::function<ViewConvention()> conventionProvider)
{
  using namespace camera;

  const UiControls uiControls(true);

  const auto noRotationSyncGroup = std::nullopt;
  const auto noTranslationSyncGroup = std::nullopt;
  const auto noZoomSyncGroup = std::nullopt;
  const auto zoomSyncGroupUid = generateRandomUuid();

  Layout layout(false);

  auto zoomGroup = layout.cameraZoomSyncGroups().try_emplace(zoomSyncGroupUid).first;

  ViewOffsetSetting offsetSetting;
  offsetSetting.m_offsetMode = ViewOffsetMode::None;

  {
    // left
    auto view = std::make_shared<View>(
      glm::vec4{-1.0f, -1.0f, 1.5f, 2.0f},
      offsetSetting,
      ViewType::Axial,
      ViewRenderMode::Image,
      IntensityProjectionMode::None,
      uiControls,
      conventionProvider,
      noRotationSyncGroup,
      noTranslationSyncGroup,
      noZoomSyncGroup
    );

    view->setPreferredDefaultRenderedImages({});
    view->setDefaultRenderAllImages(true);

    const auto viewUid = generateRandomUuid();
    layout.views().emplace(viewUid, std::move(view));
  }
  {
    // bottom right
    auto view = std::make_shared<View>(
      glm::vec4{0.5f, -1.0f, 0.5f, 1.0f},
      offsetSetting,
      ViewType::Coronal,
      ViewRenderMode::Image,
      IntensityProjectionMode::None,
      uiControls,
      conventionProvider,
      noRotationSyncGroup,
      noTranslationSyncGroup,
      zoomSyncGroupUid
    );

    view->setPreferredDefaultRenderedImages({});
    view->setDefaultRenderAllImages(true);

    const auto viewUid = generateRandomUuid();
    layout.views().emplace(viewUid, std::move(view));
    zoomGroup->second.push_back(viewUid);
  }
  {
    // top right
    auto view = std::make_shared<View>(
      glm::vec4{0.5f, 0.0f, 0.5f, 1.0f},
      offsetSetting,
      ViewType::Sagittal,
      ViewRenderMode::Image,
      IntensityProjectionMode::None,
      uiControls,
      conventionProvider,
      noRotationSyncGroup,
      noTranslationSyncGroup,
      zoomSyncGroupUid
    );

    view->setPreferredDefaultRenderedImages({});
    view->setDefaultRenderAllImages(true);

    const auto viewUid = generateRandomUuid();
    layout.views().emplace(viewUid, std::move(view));
    zoomGroup->second.push_back(viewUid);
  }

  return layout;
}

Layout createTriTopBottomLayout(
  std::size_t numRows, std::function<ViewConvention()> conventionProvider
)
{
  using namespace camera;

  const UiControls uiControls(true);

  const auto axiRotationSyncGroupUid = generateRandomUuid();
  const auto corRotationSyncGroupUid = generateRandomUuid();
  const auto sagRotationSyncGroupUid = generateRandomUuid();

  const auto axiTranslationSyncGroupUid = generateRandomUuid();
  const auto corTranslationSyncGroupUid = generateRandomUuid();
  const auto sagTranslationSyncGroupUid = generateRandomUuid();

  const auto axiZoomSyncGroupUid = generateRandomUuid();
  const auto corZoomSyncGroupUid = generateRandomUuid();
  const auto sagZoomSyncGroupUid = generateRandomUuid();

  Layout layout(false);

  auto axiRotGroup = layout.cameraRotationSyncGroups().try_emplace(axiRotationSyncGroupUid).first;
  auto corRotGroup = layout.cameraRotationSyncGroups().try_emplace(corRotationSyncGroupUid).first;
  auto sagRotGroup = layout.cameraRotationSyncGroups().try_emplace(sagRotationSyncGroupUid).first;

  auto axiTransGroup
    = layout.cameraTranslationSyncGroups().try_emplace(axiTranslationSyncGroupUid).first;
  auto corTransGroup
    = layout.cameraTranslationSyncGroups().try_emplace(corTranslationSyncGroupUid).first;
  auto sagTransGroup
    = layout.cameraTranslationSyncGroups().try_emplace(sagTranslationSyncGroupUid).first;

  // Should zoom be synchronized across columns or across rows?
  auto axiZoomGroup = layout.cameraZoomSyncGroups().try_emplace(axiZoomSyncGroupUid).first;
  auto corZoomGroup = layout.cameraZoomSyncGroups().try_emplace(corZoomSyncGroupUid).first;
  auto sagZoomGroup = layout.cameraZoomSyncGroups().try_emplace(sagZoomSyncGroupUid).first;

  ViewOffsetSetting offsetSetting;
  offsetSetting.m_offsetMode = ViewOffsetMode::None;

  for (std::size_t r = 0; r < numRows; ++r)
  {
    const float height = 2.0f / static_cast<float>(numRows);
    const float bottom = 1.0f - (static_cast<float>(r + 1)) * height;

    {
      // axial
      auto view = std::make_shared<View>(
        glm::vec4{-1.0f, bottom, 2.0f / 3.0f, height},
        offsetSetting,
        ViewType::Axial,
        ViewRenderMode::Image,
        IntensityProjectionMode::None,
        uiControls,
        conventionProvider,
        axiRotationSyncGroupUid,
        axiTranslationSyncGroupUid,
        axiZoomSyncGroupUid
      );

      view->setPreferredDefaultRenderedImages({r});
      view->setDefaultRenderAllImages(false);

      const auto viewUid = generateRandomUuid();
      layout.views().emplace(viewUid, std::move(view));

      axiRotGroup->second.push_back(viewUid);
      axiTransGroup->second.push_back(viewUid);
      axiZoomGroup->second.push_back(viewUid);
    }

    {
      // coronal
      auto view = std::make_shared<View>(
        glm::vec4{-1.0f / 3.0f, bottom, 2.0f / 3.0f, height},
        offsetSetting,
        ViewType::Coronal,
        ViewRenderMode::Image,
        IntensityProjectionMode::None,
        uiControls,
        conventionProvider,
        corRotationSyncGroupUid,
        corTranslationSyncGroupUid,
        corZoomSyncGroupUid
      );

      view->setPreferredDefaultRenderedImages({r});
      view->setDefaultRenderAllImages(false);

      const auto viewUid = generateRandomUuid();
      layout.views().emplace(viewUid, std::move(view));

      corRotGroup->second.push_back(viewUid);
      corTransGroup->second.push_back(viewUid);
      corZoomGroup->second.push_back(viewUid);
    }
    {
      // sagittal
      auto view = std::make_shared<View>(
        glm::vec4{1.0f / 3.0f, bottom, 2.0f / 3.0f, height},
        offsetSetting,
        ViewType::Sagittal,
        ViewRenderMode::Image,
        IntensityProjectionMode::None,
        uiControls,
        conventionProvider,
        sagRotationSyncGroupUid,
        sagTranslationSyncGroupUid,
        sagZoomSyncGroupUid
      );

      view->setPreferredDefaultRenderedImages({r});
      view->setDefaultRenderAllImages(false);

      const auto viewUid = generateRandomUuid();
      layout.views().emplace(viewUid, std::move(view));

      sagRotGroup->second.push_back(viewUid);
      sagTransGroup->second.push_back(viewUid);
      sagZoomGroup->second.push_back(viewUid);
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
  std::function<ViewConvention()> conventionProvider,
  const std::optional<size_t>& imageIndexForLightbox,
  const std::optional<uuids::uuid>& imageUidForLightbox
)
{
  static const camera::ViewRenderMode s_shaderType = camera::ViewRenderMode::Image;
  static const camera::IntensityProjectionMode s_ipMode = camera::IntensityProjectionMode::None;

  Layout layout(isLightbox);

  if (isLightbox)
  {
    layout.setViewType(viewType);
    layout.setRenderMode(s_shaderType);
    layout.setIntensityProjectionMode(s_ipMode);

    layout.setPreferredDefaultRenderedImages({imageIndexForLightbox ? *imageIndexForLightbox : 0});
    layout.setDefaultRenderAllImages(false);
  }

  const auto rotationSyncGroupUid = generateRandomUuid();
  const auto translationSyncGroupUid = generateRandomUuid();
  const auto zoomSyncGroupUid = generateRandomUuid();

  auto rotGroup = layout.cameraRotationSyncGroups().try_emplace(rotationSyncGroupUid).first;
  auto transGroup = layout.cameraTranslationSyncGroups().try_emplace(translationSyncGroupUid).first;
  auto zoomGroup = layout.cameraZoomSyncGroups().try_emplace(zoomSyncGroupUid).first;

  const float w = 2.0f / static_cast<float>(width);
  const float h = 2.0f / static_cast<float>(height);

  std::size_t count = 0;

  ViewOffsetSetting offsetSetting;
  offsetSetting.m_offsetImage = imageUidForLightbox;

  if (imageIndexForLightbox)
  {
    if (0 == *imageIndexForLightbox)
    {
      offsetSetting.m_offsetMode = ViewOffsetMode::RelativeToRefImageScrolls;
    }
    else
    {
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

      auto view = std::make_shared<View>(
        glm::vec4{l, b, w, h},
        offsetSetting,
        viewType,
        s_shaderType,
        s_ipMode,
        UiControls(!isLightbox),
        conventionProvider,
        rotationSyncGroupUid,
        translationSyncGroupUid,
        zoomSyncGroupUid
      );

      if (!isLightbox)
      {
        // Make each view render a different image by default:
        view->setPreferredDefaultRenderedImages({count});
        view->setDefaultRenderAllImages(false);
      }

      const auto viewUid = generateRandomUuid();
      layout.views().emplace(viewUid, std::move(view));

      // Synchronize rotations, translations, and zooms for all views in the layout:
      rotGroup->second.push_back(viewUid);
      transGroup->second.push_back(viewUid);
      zoomGroup->second.push_back(viewUid);

      ++count;
    }
  }

  return layout;
}

} // namespace

WindowData::WindowData()
  : m_viewport(0, 0, 800, 800)
  , m_windowPos(0, 0)
  , m_windowSize(800, 800)
  , m_framebufferSize(800, 800)
  , m_contentScaleRatio(1.0f, 1.0f)
  ,

  m_layouts()
  , m_currentLayout(0)
  , m_activeViewUid(std::nullopt)
  ,

  m_viewConvention(ViewConvention::Radiological)
{
  setupViews();
  setCurrentLayoutIndex(0);

  setWindowSize(m_windowSize.x, m_windowSize.y);
  setFramebufferSize(m_framebufferSize.x, m_framebufferSize.y);
}

void WindowData::setupViews()
{
  auto conventionProvider = [this]() { return m_viewConvention; };

  m_layouts.emplace_back(createFourUpLayout(conventionProvider));
  m_layouts.emplace_back(createTriLayout(conventionProvider));

  static constexpr std::size_t refImage = 0;

  m_layouts.emplace_back(
    createGridLayout(ViewType::Axial, 1, 1, false, false, conventionProvider, refImage, std::nullopt)
  );

  updateAllViews();
}

void WindowData::addGridLayout(
  const ViewType& viewType,
  std::size_t width,
  std::size_t height,
  bool offsetViews,
  bool isLightbox,
  std::size_t imageIndexForLightbox,
  const uuids::uuid& imageUidForLightbox
)
{
  auto conventionProvider = [this]() { return m_viewConvention; };

  m_layouts.emplace_back(createGridLayout(
    viewType,
    width,
    height,
    offsetViews,
    isLightbox,
    conventionProvider,
    imageIndexForLightbox,
    imageUidForLightbox
  ));

  updateAllViews();
}

void WindowData::addLightboxLayoutForImage(
  const ViewType& viewType, std::size_t numSlices, std::size_t imageIndex, const uuids::uuid& imageUid
)
{
  static constexpr bool k_offsetViews = true;
  static constexpr bool k_isLightbox = true;

  const int w = static_cast<int>(std::sqrt(numSlices + 1));
  const auto div = std::div(static_cast<int>(numSlices), w);
  const int h = div.quot + (div.rem > 0 ? 1 : 0);

  addGridLayout(
    viewType,
    static_cast<std::size_t>(w),
    static_cast<std::size_t>(h),
    k_offsetViews,
    k_isLightbox,
    imageIndex,
    imageUid
  );
}

void WindowData::addAxCorSagLayout(std::size_t numImages)
{
  auto conventionProvider = [this]() { return m_viewConvention; };

  m_layouts.emplace_back(createTriTopBottomLayout(numImages, conventionProvider));
  updateAllViews();
}

void WindowData::removeLayout(std::size_t index)
{
  if (index >= m_layouts.size())
    return;

  m_layouts.erase(std::begin(m_layouts) + static_cast<long>(index));
}

void WindowData::setDefaultRenderedImagesForLayout(Layout& layout, uuid_range_t orderedImageUids)
{
  static constexpr bool s_filterAgainstDefaults = true;

  std::list<uuids::uuid> renderedImages;
  std::list<uuids::uuid> metricImages;

  std::size_t count = 0;

  for (const auto& uid : orderedImageUids)
  {
    renderedImages.push_back(uid);

    if (count < 2)
    {
      // By default, compute the metric using the first two images:
      metricImages.push_back(uid);
      ++count;
    }
  }

  if (layout.isLightbox())
  {
    layout.setRenderedImages(renderedImages, s_filterAgainstDefaults);
    layout.setMetricImages(metricImages);
    return;
  }

  for (auto& view : layout.views())
  {
    if (!view.second)
      continue;
    view.second->setRenderedImages(renderedImages, s_filterAgainstDefaults);
    view.second->setMetricImages(metricImages);
  }
}

void WindowData::setDefaultRenderedImagesForAllLayouts(uuid_range_t orderedImageUids)
{
  static constexpr bool s_filterAgainstDefaults = true;

  std::list<uuids::uuid> renderedImages;
  std::list<uuids::uuid> metricImages;

  std::size_t count = 0;

  for (const auto& uid : orderedImageUids)
  {
    renderedImages.push_back(uid);

    if (count < 2)
    {
      // By default, compute the metric using the first two images:
      metricImages.push_back(uid);
      ++count;
    }
  }

  for (auto& layout : m_layouts)
  {
    if (layout.isLightbox())
    {
      layout.setRenderedImages(renderedImages, s_filterAgainstDefaults);
      layout.setMetricImages(metricImages);
      continue;
    }

    for (auto& view : layout.views())
    {
      if (!view.second)
        continue;
      view.second->setRenderedImages(renderedImages, s_filterAgainstDefaults);
      view.second->setMetricImages(metricImages);
    }
  }
}

void WindowData::updateImageOrdering(uuid_range_t orderedImageUids)
{
  for (auto& layout : m_layouts)
  {
    if (layout.isLightbox())
    {
      layout.updateImageOrdering(orderedImageUids);
      continue;
    }

    for (auto& view : layout.views())
    {
      if (!view.second)
        continue;
      view.second->updateImageOrdering(orderedImageUids);
    }
  }
}

void WindowData::recenterAllViews(
  const glm::vec3& worldCenter,
  const glm::vec3& worldFov,
  bool resetZoom,
  bool resetObliqueOrientation
)
{
  for (auto& layout : m_layouts)
  {
    for (auto& view : layout.views())
    {
      if (view.second)
      {
        recenterView(*view.second, worldCenter, worldFov, resetZoom, resetObliqueOrientation);
      }
    }
  }
}

void WindowData::recenterView(
  const uuids::uuid& viewUid,
  const glm::vec3& worldCenter,
  const glm::vec3& worldFov,
  bool resetZoom,
  bool resetObliqueOrientation
)
{
  if (View* view = getView(viewUid))
  {
    recenterView(*view, worldCenter, worldFov, resetZoom, resetObliqueOrientation);
  }
}

void WindowData::recenterView(
  View& view,
  const glm::vec3& worldCenter,
  const glm::vec3& worldFov,
  bool resetZoom,
  bool resetObliqueOrientation
)
{
  if (resetZoom)
  {
    camera::resetZoom(view.camera());
  }

  if (resetObliqueOrientation && (ViewType::Oblique == view.viewType()))
  {
    // Reset the view orientation for oblique views:
    camera::resetViewTransformation(view.camera());
  }

  camera::positionCameraForWorldTargetAndFov(view.camera(), worldFov, worldCenter);
}

uuid_range_t WindowData::currentViewUids() const
{
  return (m_layouts.at(m_currentLayout).views() | boost::adaptors::map_keys);
}

const View* WindowData::getCurrentView(const uuids::uuid& uid) const
{
  const auto& views = m_layouts.at(m_currentLayout).views();
  auto it = views.find(uid);
  if (std::end(views) != it)
  {
    if (it->second)
      return it->second.get();
  }
  return nullptr;
}

View* WindowData::getCurrentView(const uuids::uuid& uid)
{
  auto& views = m_layouts.at(m_currentLayout).views();
  auto it = views.find(uid);
  if (std::end(views) != it)
  {
    if (it->second)
      return it->second.get();
  }
  return nullptr;
}

const View* WindowData::getView(const uuids::uuid& uid) const
{
  for (const auto& layout : m_layouts)
  {
    auto it = layout.views().find(uid);
    if (std::end(layout.views()) != it)
    {
      if (it->second)
        return it->second.get();
    }
  }
  return nullptr;
}

View* WindowData::getView(const uuids::uuid& uid)
{
  for (const auto& layout : m_layouts)
  {
    auto it = layout.views().find(uid);
    if (std::end(layout.views()) != it)
    {
      if (it->second)
        return it->second.get();
    }
  }
  return nullptr;
}

std::optional<uuids::uuid> WindowData::currentViewUidAtCursor(const glm::vec2& windowPos) const
{
  if (m_layouts.empty())
    return std::nullopt;

  const glm::vec2 winClipPos = camera::windowNdc_T_window(m_viewport, windowPos);

  for (const auto& view : m_layouts.at(m_currentLayout).views())
  {
    if (!view.second)
      continue;
    const glm::vec4& winClipVp = view.second->windowClipViewport();

    if ( ( winClipVp[0] <= winClipPos.x ) &&
             ( winClipPos.x < winClipVp[0] + winClipVp[2] ) &&
             ( winClipVp[1] <= winClipPos.y ) &&
             ( winClipPos.y < winClipVp[1] + winClipVp[3] ) )
    {
      return view.first;
    }
  }

  return std::nullopt;
}

std::optional<uuids::uuid> WindowData::activeViewUid() const
{
  return m_activeViewUid;
}

void WindowData::setActiveViewUid(const std::optional<uuids::uuid>& uid)
{
  m_activeViewUid = uid;
}

size_t WindowData::numLayouts() const
{
  return m_layouts.size();
}

size_t WindowData::currentLayoutIndex() const
{
  return m_currentLayout;
}

const Layout* WindowData::layout(std::size_t index) const
{
  if (index < m_layouts.size())
  {
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
  if (index >= m_layouts.size())
    return;
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
  if (m_contentScaleRatio == scale)
  {
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

  if (m_windowSize.x == width && m_windowSize.y == height)
  {
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

  if (m_framebufferSize.x == width && m_framebufferSize.y == height)
  {
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
    static_cast<float>(m_framebufferSize.y) / static_cast<float>(m_windowSize.y)
  };
}

void WindowData::setViewOrientationConvention(const ViewConvention& convention)
{
  m_viewConvention = convention;
}

ViewConvention WindowData::getViewOrientationConvention() const
{
  return m_viewConvention;
}

uuid_range_t WindowData::cameraRotationGroupViewUids(const uuids::uuid& syncGroupUid) const
{
  const auto& currentLayout = m_layouts.at(m_currentLayout);
  const auto it = currentLayout.cameraRotationSyncGroups().find(syncGroupUid);
  if (std::end(currentLayout.cameraRotationSyncGroups()) != it)
    return it->second;
  return {};
}

uuid_range_t WindowData::cameraTranslationGroupViewUids(const uuids::uuid& syncGroupUid) const
{
  const auto& currentLayout = m_layouts.at(m_currentLayout);
  const auto it = currentLayout.cameraTranslationSyncGroups().find(syncGroupUid);
  if (std::end(currentLayout.cameraTranslationSyncGroups()) != it)
    return it->second;
  return {};
}

uuid_range_t WindowData::cameraZoomGroupViewUids(const uuids::uuid& syncGroupUid) const
{
  const auto& currentLayout = m_layouts.at(m_currentLayout);
  const auto it = currentLayout.cameraZoomSyncGroups().find(syncGroupUid);
  if (std::end(currentLayout.cameraZoomSyncGroups()) != it)
    return it->second;
  return {};
}

void WindowData::applyImageSelectionToAllCurrentViews(const uuids::uuid& referenceViewUid)
{
  static constexpr bool s_filterAgainstDefaults = false;

  const View* referenceView = getCurrentView(referenceViewUid);
  if (!referenceView)
    return;

  const auto renderedImages = referenceView->renderedImages();
  const auto metricImages = referenceView->metricImages();

  for (auto& viewUid : currentViewUids())
  {
    View* view = getCurrentView(viewUid);
    if (!view)
      continue;

    view->setRenderedImages(renderedImages, s_filterAgainstDefaults);
    view->setMetricImages(metricImages);
  }
}

void WindowData::applyViewRenderModeAndProjectionToAllCurrentViews(const uuids::uuid& referenceViewUid
)
{
  const View* referenceView = getCurrentView(referenceViewUid);
  if (!referenceView)
    return;

  const auto renderMode = referenceView->renderMode();
  const auto ipMode = referenceView->intensityProjectionMode();

  for (auto& viewUid : currentViewUids())
  {
    View* view = getCurrentView(viewUid);
    if (!view)
      continue;

    if (ViewType::ThreeD != view->viewType())
    {
      // Don't allow changing render mode of 3D views
      view->setRenderMode(renderMode);
    }

    view->setIntensityProjectionMode(ipMode);
  }
}

std::vector<uuids::uuid> WindowData::findCurrentViewsWithNormal(const glm::vec3& worldNormal) const
{
  // Angle threshold (in degrees) for checking whether two vectors are parallel
  static constexpr float sk_parallelThreshold_degrees = 0.1f;

  std::vector<uuids::uuid> viewUids;

  for (auto& viewUid : currentViewUids())
  {
    const View* view = getCurrentView(viewUid);
    if (!view)
      continue;

    const glm::vec3 viewBackDir = camera::worldDirection(view->camera(), Directions::View::Back);

    if (camera::areVectorsParallel(worldNormal, viewBackDir, sk_parallelThreshold_degrees))
    {
      viewUids.push_back(viewUid);
    }
  }

  return viewUids;
}

uuids::uuid WindowData::findLargestCurrentView() const
{
  uuids::uuid largestViewUid = currentViewUids().front();

  const View* largestView = getCurrentView(largestViewUid);

  if (!largestView)
  {
    spdlog::error("The current layout has no views");
    throw_debug("The current layout has no views")
  }

  float largestArea = largestView->windowClipViewport()[2] * largestView->windowClipViewport()[3];

  for (auto& viewUid : currentViewUids())
  {
    const View* view = getCurrentView(viewUid);
    if (!view)
      continue;

    const float area = view->windowClipViewport()[2] * view->windowClipViewport()[3];

    if (area > largestArea)
    {
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
    for (auto& view : layout.views())
    {
      if (auto& v = view.second)
      {
        // The view camera's aspect ratio is the product of the main window's
        // aspect ratio and the view's aspect ratio:
        const float h = v->windowClipViewport()[3];

        if (glm::epsilonEqual(h, 0.0f, glm::epsilon<float>()))
        {
          spdlog::error("View {} has zero height: setting it to 1.", view.first);
          v->setWindowClipViewport(glm::vec4{glm::vec3{v->windowClipViewport()}, 1.0f});
        }

        const float viewAspect = v->windowClipViewport()[2] / v->windowClipViewport()[3];
        v->camera().setAspectRatio(m_viewport.aspectRatio() * viewAspect);
      }
    }
  }
}

void WindowData::updateAllViews()
{
  recomputeCameraAspectRatios();
}
