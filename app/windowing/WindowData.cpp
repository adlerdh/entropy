#include "windowing/WindowData.h"

#include "common/CoordinateFrame.h"
#include "common/DirectionMaps.h"
#include "common/Exception.hpp"
#include "common/Geometry.h"
#include "common/UuidUtility.h"

#include "image/Image.h"
#include "logic/camera/Camera.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/camera/Projection.h"
#include "logic/app/Data.h"
#include "logic/app/DataHelper.h"
#include "logic/app/ImageSelectionPolicy.h"
#include "logic/camera/Camera3DControls.h"
#include "logic/camera/MathUtility.h"

#include "image/ImageUtility.h"
#include "layout/LayoutKindInfo.h"
#include "layout/LayoutPresetInfo.h"
#include "viewer/FrameHitGeometry.h"
#include "viewer/ViewModes.h"
#include "viewer/ViewTypes.h"
#include "windowing/LayoutSerialization.h"

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/string_cast.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <utility>

#undef min
#undef max

namespace
{
using uuid = uuids::uuid;

struct ViewCameraSnapshot
{
  LayoutKind m_layoutKind = LayoutKind::Custom;
  std::optional<uuid> m_layoutImageUid = std::nullopt;
  std::optional<uuid> m_viewImageUid = std::nullopt;
  ViewType m_viewType = ViewType::Axial;
  std::size_t m_viewIndex = 0;
  Camera m_camera;
  Camera m_threeDCamera;
  camera3d::State m_threeDState;
  bool m_used = false;
};

struct CameraRestoreSummary
{
  std::size_t m_restored = 0;
  std::size_t m_unmatched = 0;
};

struct CameraSnapshotKey
{
  LayoutKind m_layoutKind = LayoutKind::Custom;
  ViewType m_viewType = ViewType::Axial;
  bool m_isLightbox = false;
  std::optional<uuid> m_imageUid = std::nullopt;
  std::size_t m_viewIndex = 0;

  bool operator==(const CameraSnapshotKey& other) const
  {
    return m_layoutKind == other.m_layoutKind && m_viewType == other.m_viewType && m_isLightbox == other.m_isLightbox &&
           m_imageUid == other.m_imageUid && m_viewIndex == other.m_viewIndex;
  }
};

struct CameraSnapshotKeyHash
{
  std::size_t operator()(const CameraSnapshotKey& key) const
  {
    std::size_t seed = 0;
    hashCombine(seed, static_cast<int>(key.m_layoutKind));
    hashCombine(seed, static_cast<int>(key.m_viewType));
    hashCombine(seed, key.m_isLightbox);
    hashCombine(seed, key.m_imageUid.has_value());
    if (key.m_imageUid) {
      hashCombine(seed, *key.m_imageUid);
    }
    hashCombine(seed, key.m_viewIndex);
    return seed;
  }

private:
  template<typename T>
  static void hashCombine(std::size_t& seed, const T& value)
  {
    seed ^= std::hash<T>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }
};

using CameraSnapshotIndex = std::unordered_map<CameraSnapshotKey, std::size_t, CameraSnapshotKeyHash>;

std::optional<uuid> firstImageUid(const std::list<uuid>& imageUids)
{
  if (imageUids.empty()) {
    return std::nullopt;
  }
  return imageUids.front();
}

void copyCameraState(const Camera& source, Camera& target)
{
  target.set_start_T_world(source.start_T_world());
  target.set_camera_T_anatomy(source.camera_T_anatomy());
  target.setDefaultFov(source.projection()->defaultFov());
  target.setNearDistance(source.nearDistance());
  target.setFarDistance(source.farDistance());
  target.setZoom(source.getZoom());
}

void copyViewCameraState(const View& source, View& target)
{
  copyCameraState(source.sliceCamera(), target.sliceCamera());
  copyCameraState(source.threeDCamera(), target.threeDCamera());
  target.threeDState() = source.threeDState();
}

std::optional<uuid> layoutImageUid(const Layout& layout)
{
  return layout.isLightbox() ? firstImageUid(layout.renderedImages()) : std::nullopt;
}

std::optional<uuid> viewImageUid(const View& view)
{
  return firstImageUid(view.renderedImages());
}

CameraSnapshotKey cameraSnapshotKey(
  LayoutKind layoutKind,
  bool isLightbox,
  const std::optional<uuid>& layoutImageUid,
  const std::optional<uuid>& viewImageUid,
  ViewType viewType,
  std::size_t viewIndex)
{
  return CameraSnapshotKey{
    .m_layoutKind = layoutKind,
    .m_viewType = viewType,
    .m_isLightbox = isLightbox,
    .m_imageUid = isLightbox ? layoutImageUid : viewImageUid,
    .m_viewIndex = isLightbox ? viewIndex : 0};
}

CameraSnapshotKey cameraSnapshotKey(const Layout& layout, const View& view, std::size_t viewIndex)
{
  return cameraSnapshotKey(
    layout.kind(),
    layout.isLightbox(),
    layoutImageUid(layout),
    viewImageUid(view),
    view.viewType(),
    viewIndex);
}

CameraSnapshotKey cameraSnapshotKey(const ViewCameraSnapshot& snapshot)
{
  const bool isLightbox = layout::isLightboxLayoutKind(snapshot.m_layoutKind);
  return cameraSnapshotKey(
    snapshot.m_layoutKind,
    isLightbox,
    snapshot.m_layoutImageUid,
    snapshot.m_viewImageUid,
    snapshot.m_viewType,
    snapshot.m_viewIndex);
}

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
  layout.setKind(LayoutKind::FourUp);

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

Layout createThreeUpLayout(
  const CrosshairsState& crosshairs,
  const ViewAlignmentMode& viewAlignment,
  const ViewConvention& viewConvention)
{
  const UiControls uiControls(true);

  const auto noRotationSyncGroup = std::nullopt;
  const auto noTranslationSyncGroup = std::nullopt;
  const auto noZoomSyncGroup = std::nullopt;

  Layout layout(false);
  layout.setKind(LayoutKind::ThreeUp);

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

Layout createOrthogonalByImageLayout(
  std::size_t numRows,
  const CrosshairsState& crosshairs,
  const ViewAlignmentMode& viewAlignment,
  const ViewConvention& viewConvention)
{
  const UiControls uiControls(true);

  Layout layout(false);
  layout.setKind(LayoutKind::AxCorSagByImage);

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

  for (std::size_t r = 0; r < numRows; ++r) {
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
  const std::optional<uuid>& imageUidForLightbox,
  std::optional<float> absoluteOffsetStep = std::nullopt)
{
  static const ViewRenderMode s_shaderType = ViewRenderMode::Image;
  static const IntensityProjectionMode s_ipMode = IntensityProjectionMode::None;

  Layout layout(isLightbox);
  if (!isLightbox && 1 == height && width > 1) {
    if (ViewType::Axial == viewType || ViewType::Coronal == viewType || ViewType::Sagittal == viewType) {
      layout.setKind(LayoutKind::MultiImageGrid);
    }
  }

  if (isLightbox) {
    layout.setViewType(viewType);
    layout.setRenderMode(s_shaderType);
    layout.setIntensityProjectionMode(s_ipMode);

    layout.setPreferredDefaultRenderedImages({imageIndexForLightbox ? *imageIndexForLightbox : 0});
    layout.setDefaultRenderAllImages(false);
  }

  const auto rotSyncGroupUid = layout.addCameraSyncGroup(CameraSyncMode::Rotation);
  auto* rotGroup = layout.getCameraSyncGroup(CameraSyncMode::Rotation, rotSyncGroupUid);

  const auto transSyncGroupUid = layout.addCameraSyncGroup(CameraSyncMode::Translation);
  auto* transGroup = layout.getCameraSyncGroup(CameraSyncMode::Translation, transSyncGroupUid);

  const auto zoomSyncGroupUid = layout.addCameraSyncGroup(CameraSyncMode::Zoom);
  auto* zoomGroup = layout.getCameraSyncGroup(CameraSyncMode::Zoom, zoomSyncGroupUid);

  const float w = 2.0f / static_cast<float>(width);
  const float h = 2.0f / static_cast<float>(height);

  std::size_t count = 0;

  ViewOffsetSetting offsetSetting;
  offsetSetting.m_offsetImage = imageUidForLightbox;

  if (absoluteOffsetStep) {
    offsetSetting.m_offsetMode = ViewOffsetMode::Absolute;
  }
  else if (imageIndexForLightbox) {
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
  else {
    offsetSetting.m_offsetMode = ViewOffsetMode::RelativeToImageScrolls;
  }

  for (std::size_t j = 0; j < height; ++j) {
    for (std::size_t i = 0; i < width; ++i) {
      const float l = -1.0f + static_cast<float>(i) * w;
      const float b = -1.0f + static_cast<float>(j) * h;

      const int counter = static_cast<int>(width * j + i);

      const int offsetStep = (offsetViews) ? (counter - static_cast<int>(width * height) / 2) : 0;
      offsetSetting.m_relativeOffsetSteps = absoluteOffsetStep ? 0 : offsetStep;
      offsetSetting.m_absoluteOffset = absoluteOffsetStep ? static_cast<float>(offsetStep) * *absoluteOffsetStep : 0.0f;

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

  if (isLightbox && imageUidForLightbox) {
    const std::list<uuid> lightboxImageUids{*imageUidForLightbox};
    layout.setRenderedImages(lightboxImageUids, false);
    layout.setMetricImages(lightboxImageUids);
  }

  return layout;
}

ViewType firstViewType(const Layout& layout, ViewType fallback)
{
  const std::vector<const View*> views = layout.orderedViews();
  return views.empty() ? fallback : views.front()->viewType();
}

bool containsApprox(const std::vector<float>& values, float value)
{
  return std::any_of(values.begin(), values.end(), [value](float existing) {
    return std::fabs(existing - value) < 1.0e-4f;
  });
}

std::optional<std::pair<std::size_t, std::size_t>> gridDimensions(const Layout& layout)
{
  std::vector<float> leftEdges;
  std::vector<float> bottomEdges;

  for (const View* view : layout.orderedViews()) {
    if (!view) {
      continue;
    }

    const glm::vec4& viewport = view->windowClipViewport();
    if (!containsApprox(leftEdges, viewport.x)) {
      leftEdges.push_back(viewport.x);
    }
    if (!containsApprox(bottomEdges, viewport.y)) {
      bottomEdges.push_back(viewport.y);
    }
  }

  if (leftEdges.empty() || bottomEdges.empty()) {
    return std::nullopt;
  }
  return std::pair{leftEdges.size(), bottomEdges.size()};
}

std::string tiledLayoutDisplayName(const Layout& layout, const char* fallbackName)
{
  const auto dims = gridDimensions(layout);
  if (!dims) {
    return fallbackName;
  }

  std::ostringstream stream;
  stream << fallbackName << " " << dims->second << "x" << dims->first;
  return stream.str();
}

std::optional<std::size_t> imageIndexInOrder(uuid_range_t orderedImageUids, const std::optional<uuid>& imageUid)
{
  if (!imageUid) {
    return std::nullopt;
  }

  const auto imageIt = std::find(orderedImageUids.begin(), orderedImageUids.end(), *imageUid);
  if (imageIt == orderedImageUids.end()) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(std::distance(orderedImageUids.begin(), imageIt));
}

std::vector<std::size_t> imageIndicesInOrder(uuid_range_t orderedImageUids, const std::list<uuid>& imageUids)
{
  std::vector<std::size_t> imageIndices;
  imageIndices.reserve(imageUids.size());

  for (const auto& imageUid : imageUids) {
    if (auto imageIndex = imageIndexInOrder(orderedImageUids, imageUid)) {
      imageIndices.push_back(*imageIndex);
    }
  }

  return imageIndices;
}

std::optional<uuid> presetImageUid(const AppData& appData, const layout::LayoutPreset& preset)
{
  const auto imageIndices = layout::presetImageIndicesOrDefault(preset);
  return appData.imageUid(imageIndices.front());
}

void applyViewImageIndices(Layout& layout, const std::vector<std::size_t>& imageIndices)
{
  const std::vector<View*> views = layout.orderedViews();
  for (std::size_t i = 0; i < views.size(); ++i) {
    if (!views.at(i)) {
      continue;
    }

    const std::size_t imageIndex = imageIndices.at(std::min(i, imageIndices.size() - 1));
    views.at(i)->setPreferredDefaultRenderedImages({imageIndex});
    views.at(i)->setDefaultRenderAllImages(false);
  }
}

void applyRowImageIndices(Layout& layout, const std::vector<std::size_t>& imageIndices, std::size_t viewsPerRow)
{
  const std::vector<View*> views = layout.orderedViews();
  for (std::size_t i = 0; i < views.size(); ++i) {
    if (!views.at(i)) {
      continue;
    }

    const std::size_t row = i / viewsPerRow;
    const std::size_t imageIndex = imageIndices.at(std::min(row, imageIndices.size() - 1));
    views.at(i)->setPreferredDefaultRenderedImages({imageIndex});
    views.at(i)->setDefaultRenderAllImages(false);
  }
}

void configureOneUpDefaultImageSelection(Layout& layout)
{
  layout.setPreferredDefaultRenderedImages({});
  layout.setDefaultRenderAllImages(true);

  for (View* view : layout.orderedViews()) {
    if (!view) {
      continue;
    }

    view->setPreferredDefaultRenderedImages({});
    view->setDefaultRenderAllImages(true);
  }
}

Layout createOneUpLayout(
  const CrosshairsState& crosshairs,
  const ViewAlignmentMode& viewAlignment,
  const ViewConvention& viewConvention)
{
  static constexpr std::size_t k_refImage = 0;

  Layout layout = createGridLayout(
    ViewType::Axial,
    1,
    1,
    false,
    false,
    crosshairs,
    viewAlignment,
    viewConvention,
    k_refImage,
    std::nullopt);
  layout.setKind(LayoutKind::OneUp);
  configureOneUpDefaultImageSelection(layout);
  return layout;
}

std::optional<layout::LayoutPreset> createLayoutPreset(const Layout& layout, uuid_range_t orderedImageUids)
{
  switch (layout.kind()) {
    case LayoutKind::FourUp:
      return layout::LayoutPreset{.m_type = "fourUp", .m_view = {}, .m_images = {}, .m_imageIndices = {}};
    case LayoutKind::ThreeUp:
      return layout::LayoutPreset{.m_type = "threeUp", .m_view = {}, .m_images = {}, .m_imageIndices = {}};
    case LayoutKind::OneUp:
      return layout::LayoutPreset{
        .m_type = "oneUp",
        .m_view = std::string{layout::layoutPresetViewName(firstViewType(layout, layout.viewType()))},
        .m_images = {},
        .m_imageIndices = imageIndicesInOrder(
          orderedImageUids,
          layout.orderedViews().empty() ? std::list<uuid>{} : layout.orderedViews().front()->renderedImages())};
    case LayoutKind::MultiImageGrid:
      return layout::LayoutPreset{
        .m_type = "grid",
        .m_view = std::string{layout::layoutPresetViewName(firstViewType(layout, layout.viewType()))},
        .m_images = "all",
        .m_imageIndices = {}};
    case LayoutKind::AxCorSagByImage:
      return layout::LayoutPreset{.m_type = "orthogonalByImage", .m_view = {}, .m_images = "all", .m_imageIndices = {}};
    case LayoutKind::Lightbox:
      return layout::LayoutPreset{
        .m_type = "lightbox",
        .m_view = std::string{layout::layoutPresetViewName(firstViewType(layout, layout.viewType()))},
        .m_images = {},
        .m_imageIndices = {imageIndexInOrder(orderedImageUids, layoutImageUid(layout)).value_or(0)}};
    case LayoutKind::Custom:
    case LayoutKind::NumElements:
      break;
  }
  return std::nullopt;
}

std::optional<Layout> createLightboxLayoutForImage(
  const AppData& appData,
  const CrosshairsState& crosshairs,
  const ViewAlignmentMode& viewAlignment,
  const ViewConvention& viewConvention,
  const ViewType& viewType,
  const uuid& imageUid);

std::optional<Layout> instantiateLayoutPreset(
  const AppData& appData,
  const layout::LayoutPreset& preset,
  const CrosshairsState& crosshairs,
  const ViewAlignmentMode& viewAlignment,
  const ViewConvention& viewConvention)
{
  const auto viewType = layout::layoutPresetViewType(preset.m_view);
  if (!viewType) {
    spdlog::warn("Skipping layout preset with unsupported view '{}'", preset.m_view);
    return std::nullopt;
  }

  if ("fourUp" == preset.m_type) {
    return createFourUpLayout(crosshairs, viewAlignment, viewConvention);
  }
  if ("threeUp" == preset.m_type) {
    return createThreeUpLayout(crosshairs, viewAlignment, viewConvention);
  }
  if ("oneUp" == preset.m_type) {
    const auto imageIndices = layout::presetImageIndicesOrDefault(preset);
    Layout layout = createGridLayout(
      *viewType,
      1,
      1,
      false,
      false,
      crosshairs,
      viewAlignment,
      viewConvention,
      imageIndices.front(),
      appData.imageUid(imageIndices.front()));
    applyViewImageIndices(layout, imageIndices);
    if (ViewType::Axial == *viewType || ViewType::Coronal == *viewType || ViewType::Sagittal == *viewType) {
      layout.setKind(LayoutKind::OneUp);
    }
    else {
      layout.setKind(LayoutKind::Custom);
    }
    return std::optional<Layout>{std::move(layout)};
  }
  if ("grid" == preset.m_type) {
    const auto imageIndices =
      ("all" == preset.m_images) ? std::vector<std::size_t>{} : layout::presetImageIndicesOrDefault(preset);
    const std::size_t width = imageIndices.empty() ? appData.numImages() : imageIndices.size();
    if (0 == width) {
      return std::nullopt;
    }
    Layout layout = createGridLayout(
      *viewType,
      width,
      1,
      false,
      false,
      crosshairs,
      viewAlignment,
      viewConvention,
      0,
      appData.refImageUid());
    if (!imageIndices.empty()) {
      applyViewImageIndices(layout, imageIndices);
    }
    if (ViewType::Axial == *viewType || ViewType::Coronal == *viewType || ViewType::Sagittal == *viewType) {
      layout.setKind(LayoutKind::MultiImageGrid);
    }
    else {
      layout.setKind(LayoutKind::Custom);
    }
    return std::optional<Layout>{std::move(layout)};
  }
  if ("orthogonalByImage" == preset.m_type) {
    const auto imageIndices =
      ("all" == preset.m_images) ? std::vector<std::size_t>{} : layout::presetImageIndicesOrDefault(preset);
    const std::size_t numRows = imageIndices.empty() ? appData.numImages() : imageIndices.size();
    if (0 == numRows) {
      return std::nullopt;
    }
    Layout layout = createOrthogonalByImageLayout(numRows, crosshairs, viewAlignment, viewConvention);
    if (!imageIndices.empty()) {
      applyRowImageIndices(layout, imageIndices, 3);
    }
    return std::optional<Layout>{std::move(layout)};
  }
  if ("lightbox" == preset.m_type) {
    if (preset.m_imageIndices.size() > 1) {
      spdlog::warn("Skipping lightbox layout preset because lightboxes support exactly one image");
      return std::nullopt;
    }
    const auto imageUid = presetImageUid(appData, preset);
    if (!imageUid) {
      spdlog::warn(
        "Skipping lightbox layout preset for missing image index {}",
        layout::presetImageIndicesOrDefault(preset).front());
      return std::nullopt;
    }
    return createLightboxLayoutForImage(appData, crosshairs, viewAlignment, viewConvention, *viewType, *imageUid);
  }

  spdlog::warn("Skipping layout preset with unsupported type '{}'", preset.m_type);
  return std::nullopt;
}

std::size_t fixedManagedLayoutPrefixLength(const std::vector<Layout>& layouts)
{
  std::size_t index = 0;
  while (index < layouts.size() && layout::isFixedManagedLayoutKind(layouts.at(index).kind())) {
    ++index;
  }
  return index;
}

bool isOneUpLayoutKind(LayoutKind kind)
{
  switch (kind) {
    case LayoutKind::OneUp:
      return true;
    case LayoutKind::Custom:
    case LayoutKind::FourUp:
    case LayoutKind::ThreeUp:
    case LayoutKind::MultiImageGrid:
    case LayoutKind::AxCorSagByImage:
    case LayoutKind::Lightbox:
    case LayoutKind::NumElements:
      return false;
  }
  return false;
}

bool shouldDefaultToOneUpLayout(const AppData& appData)
{
  if (0 == appData.numImages()) {
    return false;
  }

  for (const uuid& imageUid : appData.imageUidsOrdered()) {
    const Image* image = appData.image(imageUid);
    if (!image || !isLinearOrPlanarImage(image->header())) {
      return false;
    }
  }

  return true;
}

std::optional<std::size_t> firstLayoutIndexWithKind(const std::vector<Layout>& layouts, LayoutKind kind)
{
  const auto layoutIt =
    std::find_if(layouts.begin(), layouts.end(), [kind](const Layout& layout) { return layout.kind() == kind; });

  if (layoutIt == layouts.end()) {
    return std::nullopt;
  }

  return static_cast<std::size_t>(std::distance(layouts.begin(), layoutIt));
}

std::size_t defaultLayoutIndexForImages(const std::vector<Layout>& layouts, const AppData& appData)
{
  const LayoutKind preferredKind = shouldDefaultToOneUpLayout(appData) ? LayoutKind::OneUp : LayoutKind::ThreeUp;
  if (const auto preferredIndex = firstLayoutIndexWithKind(layouts, preferredKind)) {
    return *preferredIndex;
  }

  return layouts.empty() ? std::size_t{0} : 0;
}

LayoutKind oneUpLayoutKindForViewType(ViewType viewType)
{
  switch (viewType) {
    case ViewType::Axial:
    case ViewType::Coronal:
    case ViewType::Sagittal:
      return LayoutKind::OneUp;
    case ViewType::Oblique:
    case ViewType::ThreeD:
    case ViewType::NumElements:
      return LayoutKind::Custom;
  }
  return LayoutKind::Custom;
}

LayoutKind gridLayoutKindForViewType(ViewType viewType)
{
  switch (viewType) {
    case ViewType::Axial:
    case ViewType::Coronal:
    case ViewType::Sagittal:
      return LayoutKind::MultiImageGrid;
    case ViewType::Oblique:
    case ViewType::ThreeD:
    case ViewType::NumElements:
      return LayoutKind::Custom;
  }
  return LayoutKind::Custom;
}

std::vector<ViewType> managedSliceViewTypes(
  std::size_t numImages,
  const std::unordered_map<uuid, ViewType>& dicomNativeViewTypesByImage)
{
  if (dicomNativeViewTypesByImage.empty()) {
    return {ViewType::Axial};
  }

  std::vector<ViewType> viewTypes;
  viewTypes.reserve(3);
  if (dicomNativeViewTypesByImage.size() < numImages) {
    viewTypes.push_back(ViewType::Axial);
  }

  for (const ViewType viewType : {ViewType::Axial, ViewType::Coronal, ViewType::Sagittal}) {
    const bool hasViewType =
      std::any_of(dicomNativeViewTypesByImage.begin(), dicomNativeViewTypesByImage.end(), [viewType](const auto& item) {
        return item.second == viewType;
      });
    if (hasViewType && std::find(viewTypes.begin(), viewTypes.end(), viewType) == viewTypes.end()) {
      viewTypes.push_back(viewType);
    }
  }
  return viewTypes.empty() ? std::vector<ViewType>{ViewType::Axial} : viewTypes;
}

ViewType managedLightboxViewTypeForImage(
  const uuid& imageUid,
  const std::unordered_map<uuid, ViewType>& dicomNativeViewTypesByImage)
{
  const auto viewTypeIt = dicomNativeViewTypesByImage.find(imageUid);
  return (viewTypeIt == dicomNativeViewTypesByImage.end()) ? ViewType::Axial : viewTypeIt->second;
}

std::optional<Layout> createLightboxLayoutForImage(
  const AppData& appData,
  const CrosshairsState& crosshairs,
  const ViewAlignmentMode& viewAlignment,
  const ViewConvention& viewConvention,
  const ViewType& viewType,
  const uuid& imageUid)
{
  const Image* image = appData.image(imageUid);
  const auto imageIndex = appData.imageIndex(imageUid);
  if (!image || !imageIndex) {
    return std::nullopt;
  }

  glm::vec3 worldDirection{0.0f};
  switch (viewType) {
    case ViewType::Axial:
      worldDirection = Directions::get(Directions::Anatomy::Inferior);
      break;
    case ViewType::Coronal:
      worldDirection = Directions::get(Directions::Anatomy::Anterior);
      break;
    case ViewType::Sagittal:
      worldDirection = Directions::get(Directions::Anatomy::Right);
      break;
    case ViewType::Oblique:
    case ViewType::ThreeD:
    case ViewType::NumElements:
      return std::nullopt;
  }

  const std::size_t numSlices = data::computeNumImageSlicesAlongWorldDirection(*image, worldDirection);
  if (0 == numSlices) {
    spdlog::warn(
      "Skipping {} lightbox layout for image {} because it has no slices",
      to_string(viewType, false),
      imageUid);
    return std::nullopt;
  }

  const int w = static_cast<int>(std::sqrt(numSlices + 1));
  const auto div = std::div(static_cast<int>(numSlices), w);
  const int h = div.quot + (div.rem > 0 ? 1 : 0);

  Layout layout = createGridLayout(
    viewType,
    static_cast<std::size_t>(w),
    static_cast<std::size_t>(h),
    true,
    true,
    crosshairs,
    viewAlignment,
    viewConvention,
    *imageIndex,
    imageUid);
  layout.setKind(layout::lightboxLayoutKindForViewType(viewType));
  return std::optional<Layout>{std::move(layout)};
}

std::vector<ViewCameraSnapshot> createManagedLayoutCameraSnapshots(const std::vector<Layout>& layouts)
{
  std::vector<ViewCameraSnapshot> snapshots;

  for (const auto& layout : layouts) {
    if (!layout::isImageDependentManagedLayoutKind(layout.kind())) {
      continue;
    }

    const std::optional<uuid> imageUid = layoutImageUid(layout);
    std::size_t viewIndex = 0;
    for (const View* view : layout.orderedViews()) {
      snapshots.push_back(ViewCameraSnapshot{
        .m_layoutKind = layout.kind(),
        .m_layoutImageUid = imageUid,
        .m_viewImageUid = viewImageUid(*view),
        .m_viewType = view->viewType(),
        .m_viewIndex = viewIndex,
        .m_camera = view->sliceCamera(),
        .m_threeDCamera = view->threeDCamera(),
        .m_threeDState = view->threeDState(),
        .m_used = false});
      ++viewIndex;
    }
  }

  return snapshots;
}

CameraSnapshotIndex createCameraSnapshotIndex(const std::vector<ViewCameraSnapshot>& snapshots)
{
  CameraSnapshotIndex index;
  index.reserve(snapshots.size());
  for (std::size_t i = 0; i < snapshots.size(); ++i) {
    index.emplace(cameraSnapshotKey(snapshots.at(i)), i);
  }
  return index;
}

View* viewInLayout(Layout& layout, const uuid& viewUid)
{
  const auto& views = layout.views();
  const auto viewIt = views.find(viewUid);
  if (viewIt == views.end() || !viewIt->second) {
    return nullptr;
  }
  return viewIt->second.get();
}

std::optional<std::size_t> orderedViewIndex(const Layout& layout, const uuid& viewUid)
{
  const auto& orderedViewUids = layout.orderedViewUids();
  for (std::size_t i = 0; i < orderedViewUids.size(); ++i) {
    if (orderedViewUids.at(i) == viewUid) {
      return i;
    }
  }
  return std::nullopt;
}

bool viewHadRestoredCamera(
  const View& view,
  const Layout& layout,
  const std::vector<ViewCameraSnapshot>& snapshots,
  const CameraSnapshotIndex& snapshotIndex,
  std::size_t viewIndex)
{
  const auto snapshotIt = snapshotIndex.find(cameraSnapshotKey(layout, view, viewIndex));
  return snapshotIt != snapshotIndex.end() && snapshots.at(snapshotIt->second).m_used;
}

View* restoredSyncedView(
  Layout& layout,
  const View& view,
  const std::vector<ViewCameraSnapshot>& snapshots,
  const CameraSnapshotIndex& snapshotIndex,
  CameraSyncMode syncMode,
  const std::optional<uuid>& syncGroupUid)
{
  if (!syncGroupUid) {
    return nullptr;
  }

  const auto* group = layout.getCameraSyncGroup(syncMode, *syncGroupUid);
  if (!group) {
    return nullptr;
  }

  for (const auto& syncedViewUid : *group) {
    if (syncedViewUid == view.uid()) {
      continue;
    }

    View* candidate = viewInLayout(layout, syncedViewUid);
    if (!candidate || candidate->viewType() != view.viewType()) {
      continue;
    }

    const std::optional<std::size_t> candidateIndex = orderedViewIndex(layout, candidate->uid());
    if (!candidateIndex) {
      continue;
    }

    if (viewHadRestoredCamera(*candidate, layout, snapshots, snapshotIndex, *candidateIndex)) {
      return candidate;
    }
  }

  return nullptr;
}

bool initializeFromSyncedRestoredCamera(
  Layout& layout,
  View& view,
  const std::vector<ViewCameraSnapshot>& snapshots,
  const CameraSnapshotIndex& snapshotIndex)
{
  const std::array candidates{
    std::pair{CameraSyncMode::Zoom, view.cameraZoomSyncGroupUid()},
    std::pair{CameraSyncMode::Translation, view.cameraTranslationSyncGroupUid()},
    std::pair{CameraSyncMode::Rotation, view.cameraRotationSyncGroupUid()}};

  for (const auto& [syncMode, syncGroupUid] : candidates) {
    if (View* source = restoredSyncedView(layout, view, snapshots, snapshotIndex, syncMode, syncGroupUid)) {
      copyViewCameraState(*source, view);
      return true;
    }
  }

  return false;
}

CameraRestoreSummary restoreManagedLayoutCameraSnapshots(
  std::vector<Layout>& layouts,
  std::vector<ViewCameraSnapshot>& snapshots,
  const CameraSnapshotIndex& snapshotIndex)
{
  CameraRestoreSummary summary;

  for (auto& layout : layouts) {
    if (!layout::isImageDependentManagedLayoutKind(layout.kind())) {
      continue;
    }

    std::size_t viewIndex = 0;
    for (View* view : layout.orderedViews()) {
      auto snapshotIt = snapshotIndex.find(cameraSnapshotKey(layout, *view, viewIndex));

      if (snapshotIt != snapshotIndex.end()) {
        ViewCameraSnapshot& snapshot = snapshots.at(snapshotIt->second);
        copyCameraState(snapshot.m_camera, view->sliceCamera());
        copyCameraState(snapshot.m_threeDCamera, view->threeDCamera());
        view->threeDState() = snapshot.m_threeDState;
        snapshot.m_used = true;
        ++summary.m_restored;
      }
      else {
        ++summary.m_unmatched;
      }

      ++viewIndex;
    }
  }

  return summary;
}

void initializeUnmatchedManagedLayoutCameras(
  WindowData& windowData,
  std::vector<Layout>& layouts,
  const std::vector<ViewCameraSnapshot>& snapshots,
  const CameraSnapshotIndex& snapshotIndex,
  const glm::vec3& worldCenter,
  const glm::vec3& worldFov)
{
  for (auto& layout : layouts) {
    if (!layout::isImageDependentManagedLayoutKind(layout.kind())) {
      continue;
    }

    std::size_t viewIndex = 0;
    for (View* view : layout.orderedViews()) {
      const auto snapshotIt = snapshotIndex.find(cameraSnapshotKey(layout, *view, viewIndex));

      if (snapshotIt == snapshotIndex.end()) {
        if (layout.isLightbox() || !initializeFromSyncedRestoredCamera(layout, *view, snapshots, snapshotIndex)) {
          windowData.recenterView(*view, worldCenter, worldFov, false, true);
        }
      }

      ++viewIndex;
    }
  }
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
  m_layouts.emplace_back(createOneUpLayout(m_crosshairs, m_viewAlignment, m_viewConvention));
  m_layouts.emplace_back(createThreeUpLayout(m_crosshairs, m_viewAlignment, m_viewConvention));
  m_layouts.emplace_back(createFourUpLayout(m_crosshairs, m_viewAlignment, m_viewConvention));

  updateAllViews();
}

void WindowData::addGridLayout(
  const ViewType& viewType,
  std::size_t width,
  std::size_t height,
  bool offsetViews,
  bool isLightbox,
  std::size_t imageIndexForLightbox,
  const uuid& imageUidForLightbox,
  std::optional<float> absoluteOffsetStep)
{
  m_layouts.emplace_back(createGridLayout(
    viewType,
    width,
    height,
    offsetViews,
    isLightbox,
    m_crosshairs,
    m_viewAlignment,
    m_viewConvention,
    imageIndexForLightbox,
    imageUidForLightbox,
    absoluteOffsetStep));

  updateAllViews();
}

void WindowData::addLightboxLayoutForImage(
  const ViewType& viewType,
  std::size_t numSlices,
  std::size_t imageIndex,
  const uuid& imageUid)
{
  static constexpr bool k_offsetViews = true;
  static constexpr bool k_isLightbox = true;

  if (0 == numSlices) {
    spdlog::warn(
      "Skipping {} lightbox layout for image {} because it has no slices",
      to_string(viewType, false),
      imageUid);
    return;
  }

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
    imageUid);
  m_layouts.back().setKind(layout::lightboxLayoutKindForViewType(viewType));
}

void WindowData::addAxCorSagLayout(std::size_t numImages)
{
  m_layouts.emplace_back(createOrthogonalByImageLayout(numImages, m_crosshairs, m_viewAlignment, m_viewConvention));
  updateAllViews();
}

void WindowData::setCurrentLayoutViewType(const AppData& appData, const ViewType& viewType)
{
  if (m_currentLayout >= m_layouts.size()) {
    return;
  }

  Layout& layout = m_layouts.at(m_currentLayout);
  if (!layout.isLightbox() || LayoutKind::Lightbox != layout.kind()) {
    layout.setViewType(viewType);
    return;
  }

  const std::optional<uuid> imageUid = layoutImageUid(layout);
  if (!imageUid) {
    spdlog::warn("Cannot rebuild managed lightbox layout because it has no source image");
    return;
  }

  auto rebuiltLayout =
    createLightboxLayoutForImage(appData, m_crosshairs, m_viewAlignment, m_viewConvention, viewType, *imageUid);
  if (!rebuiltLayout) {
    spdlog::warn("Cannot rebuild managed lightbox layout for unsupported view type {}", to_string(viewType, false));
    return;
  }

  setDefaultRenderedImagesForLayout(*rebuiltLayout, appData);
  m_layouts.at(m_currentLayout).replaceContentsPreservingUid(std::move(*rebuiltLayout));
  if (m_activeViewUid && !getCurrentView(*m_activeViewUid)) {
    m_activeViewUid = std::nullopt;
  }
  updateAllViews();
}

void WindowData::reconcileImageDependentLayouts(
  const AppData& appData,
  const std::unordered_map<uuid, ViewType>& dicomNativeViewTypesByImage)
{
  const uuid currentLayoutUid = (m_currentLayout < m_layouts.size()) ? m_layouts.at(m_currentLayout).uid() : uuid{};
  std::vector<ViewCameraSnapshot> cameraSnapshots = createManagedLayoutCameraSnapshots(m_layouts);

  m_layouts.erase(
    std::remove_if(
      m_layouts.begin(),
      m_layouts.end(),
      [](const Layout& layout) { return layout::isImageDependentManagedLayoutKind(layout.kind()); }),
    m_layouts.end());

  std::vector<Layout> generatedLayouts;
  generatedLayouts.reserve(5 + 3 * appData.numImages());

  const uuid_range_t orderedImageUids = appData.imageUidsOrdered();
  const std::vector<ViewType> managedViewTypes =
    managedSliceViewTypes(appData.numImages(), dicomNativeViewTypesByImage);

  for (const ViewType viewType : managedViewTypes) {
    generatedLayouts.emplace_back(
      createGridLayout(viewType, 1, 1, false, false, m_crosshairs, m_viewAlignment, m_viewConvention, 0, std::nullopt));
    generatedLayouts.back().setKind(oneUpLayoutKindForViewType(viewType));
  }

  if (orderedImageUids.size() > 1) {
    if (const auto& refUid = appData.refImageUid()) {
      for (const ViewType viewType : managedViewTypes) {
        generatedLayouts.emplace_back(createGridLayout(
          viewType,
          orderedImageUids.size(),
          1,
          false,
          false,
          m_crosshairs,
          m_viewAlignment,
          m_viewConvention,
          0,
          *refUid));
        generatedLayouts.back().setKind(gridLayoutKindForViewType(viewType));
      }
    }
  }

  generatedLayouts.emplace_back(
    createOrthogonalByImageLayout(orderedImageUids.size(), m_crosshairs, m_viewAlignment, m_viewConvention));

  for (const auto& imageUid : orderedImageUids) {
    const ViewType viewType = managedLightboxViewTypeForImage(imageUid, dicomNativeViewTypesByImage);
    if (
      auto layout =
        createLightboxLayoutForImage(appData, m_crosshairs, m_viewAlignment, m_viewConvention, viewType, imageUid))
    {
      generatedLayouts.emplace_back(std::move(*layout));
    }
  }

  for (auto& layout : generatedLayouts) {
    setDefaultRenderedImagesForLayout(layout, appData);
  }
  const CameraSnapshotIndex cameraSnapshotIndex = createCameraSnapshotIndex(cameraSnapshots);
  const CameraRestoreSummary restoreSummary =
    restoreManagedLayoutCameraSnapshots(generatedLayouts, cameraSnapshots, cameraSnapshotIndex);
  if (!cameraSnapshots.empty() && restoreSummary.m_unmatched > 0) {
    constexpr float viewAABBoxScaleFactor = 1.10f;
    const auto worldBox = data::computeWorldAABBoxEnclosingImages(appData, ImageSelection::AllLoadedImages);
    initializeUnmatchedManagedLayoutCameras(
      *this,
      generatedLayouts,
      cameraSnapshots,
      cameraSnapshotIndex,
      m_crosshairs.worldCrosshairs.worldOrigin(),
      viewAABBoxScaleFactor * math::computeAABBoxSize(worldBox));
  }

  const std::size_t fixedPrefixLength = fixedManagedLayoutPrefixLength(m_layouts);
  std::vector<Layout> orderedLayouts;
  orderedLayouts.reserve(m_layouts.size() + generatedLayouts.size());

  for (auto& layout : generatedLayouts) {
    if (isOneUpLayoutKind(layout.kind())) {
      orderedLayouts.emplace_back(std::move(layout));
    }
  }

  orderedLayouts.insert(
    orderedLayouts.end(),
    std::make_move_iterator(m_layouts.begin()),
    std::make_move_iterator(m_layouts.begin() + static_cast<std::ptrdiff_t>(fixedPrefixLength)));

  for (auto& layout : generatedLayouts) {
    if (!isOneUpLayoutKind(layout.kind())) {
      orderedLayouts.emplace_back(std::move(layout));
    }
  }

  orderedLayouts.insert(
    orderedLayouts.end(),
    std::make_move_iterator(m_layouts.begin() + static_cast<std::ptrdiff_t>(fixedPrefixLength)),
    std::make_move_iterator(m_layouts.end()));
  m_layouts = std::move(orderedLayouts);

  auto currentLayoutIt = std::find_if(m_layouts.begin(), m_layouts.end(), [&currentLayoutUid](const Layout& layout) {
    return layout.uid() == currentLayoutUid;
  });
  m_currentLayout = (currentLayoutIt != m_layouts.end())
                      ? static_cast<std::size_t>(std::distance(m_layouts.begin(), currentLayoutIt))
                      : std::min(m_currentLayout, m_layouts.empty() ? std::size_t{0} : m_layouts.size() - 1);
  if (m_activeViewUid && !getCurrentView(*m_activeViewUid)) {
    m_activeViewUid = std::nullopt;
  }
  updateAllViews();
}

std::vector<layout::LayoutSpec> WindowData::createProjectLayoutSnapshots(uuid_range_t orderedImageUids) const
{
  return layout::createLayoutSpecs(m_layouts, orderedImageUids);
}

bool WindowData::applyProjectLayoutSnapshots(
  const std::vector<layout::LayoutSpec>& layouts,
  uuid_range_t orderedImageUids,
  std::optional<std::size_t> currentLayoutIndex)
{
  if (layouts.empty()) {
    return false;
  }

  std::vector<Layout> restoredLayouts =
    layout::instantiateLayoutSpecs(layouts, orderedImageUids, m_crosshairs, m_viewAlignment, m_viewConvention);

  if (restoredLayouts.empty()) {
    return false;
  }

  m_layouts = std::move(restoredLayouts);
  m_currentLayout = (currentLayoutIndex && *currentLayoutIndex < m_layouts.size()) ? *currentLayoutIndex : 0;
  m_activeViewUid = std::nullopt;
  updateAllViews();
  return true;
}

std::vector<layout::LayoutPreset> WindowData::createLayoutPresets(uuid_range_t orderedImageUids) const
{
  std::vector<layout::LayoutPreset> presets;
  presets.reserve(m_layouts.size());

  for (const auto& layout : m_layouts) {
    if (auto preset = createLayoutPreset(layout, orderedImageUids)) {
      presets.push_back(std::move(*preset));
    }
    else {
      spdlog::warn(
        "Skipping layout of kind {} because it cannot be represented as a compact layout preset",
        static_cast<int>(layout.kind()));
    }
  }

  return presets;
}

bool WindowData::applyLayoutPresets(
  const AppData& appData,
  const std::vector<layout::LayoutPreset>& presets,
  std::optional<std::size_t> currentLayoutIndex)
{
  if (presets.empty()) {
    clearLayouts();
    if (shouldDefaultToOneUpLayout(appData)) {
      m_layouts.emplace_back(createOneUpLayout(m_crosshairs, m_viewAlignment, m_viewConvention));
      setCurrentLayoutIndex(0);
      setWindowSize(m_windowSize.x, m_windowSize.y);
      setFramebufferSize(m_framebufferSize.x, m_framebufferSize.y);
    }
    else {
      resetToThreeUpLayout();
    }
    setDefaultRenderedImagesForLayout(currentLayout(), appData);
    return true;
  }

  std::vector<Layout> restoredLayouts;
  restoredLayouts.reserve(presets.size());

  for (const auto& preset : presets) {
    if (auto layout = instantiateLayoutPreset(appData, preset, m_crosshairs, m_viewAlignment, m_viewConvention)) {
      restoredLayouts.push_back(std::move(*layout));
    }
  }

  if (restoredLayouts.empty()) {
    return false;
  }

  for (auto& layout : restoredLayouts) {
    setDefaultRenderedImagesForLayout(layout, appData);
  }

  m_layouts = std::move(restoredLayouts);
  m_currentLayout = (currentLayoutIndex && *currentLayoutIndex < m_layouts.size()) ? *currentLayoutIndex : 0;
  m_activeViewUid = std::nullopt;
  updateAllViews();
  return true;
}

void WindowData::removeLayout(std::size_t index)
{
  if (index >= m_layouts.size() || m_layouts.size() <= 1) {
    return;
  }

  m_layouts.erase(std::begin(m_layouts) + static_cast<long>(index));
  if (m_currentLayout >= m_layouts.size()) {
    m_currentLayout = 0;
  }
  if (m_activeViewUid && !getCurrentView(*m_activeViewUid)) {
    m_activeViewUid = std::nullopt;
  }
}

void WindowData::clearLayouts()
{
  m_layouts.clear();
  m_currentLayout = 0;
  m_activeViewUid = std::nullopt;
}

void WindowData::resetDefaultLayouts()
{
  clearLayouts();
  setupViews();
  setCurrentLayoutIndex(0);
}

void WindowData::setCurrentLayoutToDefaultForImages(const AppData& appData)
{
  setCurrentLayoutIndex(defaultLayoutIndexForImages(m_layouts, appData));
}

void WindowData::resetToThreeUpLayout()
{
  clearLayouts();
  m_layouts.emplace_back(createThreeUpLayout(m_crosshairs, m_viewAlignment, m_viewConvention));
  setCurrentLayoutIndex(0);
  setWindowSize(m_windowSize.x, m_windowSize.y);
  setFramebufferSize(m_framebufferSize.x, m_framebufferSize.y);
}

void WindowData::setDefaultRenderedImagesForLayout(Layout& layout, const AppData& appData)
{
  static constexpr bool s_filterAgainstDefaults = true;

  std::list<uuid> renderedImages;
  const uuid_range_t orderedImageUids = appData.imageUidsOrdered();

  for (const auto& uid : orderedImageUids) {
    renderedImages.push_back(uid);
  }

  const std::list<uuid> metricImages = app::image_selection_policy::defaultMetricImageUids(
    orderedImageUids,
    appData.refImageUid(),
    appData.activeImageUid());

  if (isOneUpLayoutKind(layout.kind())) {
    configureOneUpDefaultImageSelection(layout);
  }

  if (layout.isLightbox()) {
    layout.setRenderedImages(renderedImages, s_filterAgainstDefaults);
    layout.setMetricImages(metricImages);
    return;
  }

  for (View* view : layout.orderedViews()) {
    view->setRenderedImages(renderedImages, s_filterAgainstDefaults);
    view->setMetricImages(metricImages);
  }
}

void WindowData::setDefaultRenderedImagesForAllLayouts(const AppData& appData)
{
  static constexpr bool s_filterAgainstDefaults = true;

  std::list<uuid> renderedImages;
  const uuid_range_t orderedImageUids = appData.imageUidsOrdered();

  for (const auto& uid : orderedImageUids) {
    renderedImages.push_back(uid);
  }

  const std::list<uuid> metricImages = app::image_selection_policy::defaultMetricImageUids(
    orderedImageUids,
    appData.refImageUid(),
    appData.activeImageUid());

  for (auto& layout : m_layouts) {
    if (isOneUpLayoutKind(layout.kind())) {
      configureOneUpDefaultImageSelection(layout);
    }

    if (layout.isLightbox()) {
      layout.setRenderedImages(renderedImages, s_filterAgainstDefaults);
      layout.setMetricImages(metricImages);
      continue;
    }

    for (View* view : layout.orderedViews()) {
      view->setRenderedImages(renderedImages, s_filterAgainstDefaults);
      view->setMetricImages(metricImages);
    }
  }
}

void WindowData::updateImageOrdering(uuid_range_t orderedImageUids)
{
  for (auto& layout : m_layouts) {
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

void WindowData::removeImageFromLayouts(const uuid& imageUid, uuid_range_t orderedImageUids)
{
  for (std::size_t i = m_layouts.size(); i > 0; --i) {
    const std::size_t layoutIndex = i - 1;
    const auto& layout = m_layouts.at(layoutIndex);

    if (layout.isLightbox() && layout.renderedImages().size() == 1 && layout.renderedImages().front() == imageUid) {
      removeLayout(layoutIndex);
    }
  }

  updateImageOrdering(orderedImageUids);
}

void WindowData::appendImageToDefaultRenderedImages(const AppData& appData, const uuid& imageUid)
{
  const auto appendToFrame = [&appData, &imageUid](ControlFrame& frame) {
    if (!frame.defaultRenderAllImages()) {
      return;
    }

    frame.setImageRendered(appData, imageUid, true);

    if (frame.metricImages().size() < 2) {
      if (const auto imageIndex = appData.imageIndex(imageUid)) {
        frame.setImageUsedForMetric(appData, *imageIndex, true);
      }
    }
  };

  for (auto& layout : m_layouts) {
    if (layout.isLightbox()) {
      continue;
    }

    appendToFrame(layout);

    for (auto& [viewUid, view] : layout.views()) {
      if (view) {
        appendToFrame(*view);
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

  if (resetObliqueOrientation) {
    if (ViewType::Oblique == view.viewType()) {
      // Reset the view orientation for oblique views:
      helper::resetViewTransformation(view.camera());
    }
  }

  if (ViewType::ThreeD == view.viewType()) {
    const camera3d::SceneFrame scene{.m_center = worldCenter, .m_size = worldFov};
    const glm::vec3 orbitTarget =
      (camera3d::OrbitTargetMode::Crosshairs == view.threeDState().m_orbitTargetMode) ? worldCenter : scene.m_center;
    if (view.threeDState().m_viewPositionFollowsCrosshairs) {
      if (resetZoom) {
        view.initializeThreeDCameraIfNeeded(scene);
        camera3d::resetFollowing(view.threeDCamera(), view.threeDState(), scene, worldCenter, orbitTarget);
        return;
      }
      view.initializeThreeDCameraIfNeeded(scene);
      camera3d::recenterFollowing(view.threeDCamera(), view.threeDState(), scene, worldCenter, orbitTarget);
      return;
    }
    if (resetZoom) {
      view.resetThreeDCamera(scene, orbitTarget);
      return;
    }
    view.recenterThreeDCamera(scene, orbitTarget);
    return;
  }

  helper::positionCameraForWorldTargetAndFov(view.camera(), worldFov, worldCenter);
}

uuid_range_t WindowData::currentViewUids() const
{
  if (m_currentLayout >= m_layouts.size()) {
    return {};
  }
  const auto& orderedViewUids = m_layouts.at(m_currentLayout).orderedViewUids();
  return uuid_range_t{orderedViewUids.begin(), orderedViewUids.end()};
}

const View* WindowData::getCurrentView(const uuid& uid) const
{
  if (m_currentLayout >= m_layouts.size()) {
    return nullptr;
  }

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
  if (m_currentLayout >= m_layouts.size()) {
    return nullptr;
  }

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
  for (const auto& layout : m_layouts) {
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
  for (const auto& layout : m_layouts) {
    auto it = layout.views().find(uid);
    if (std::end(layout.views()) != it) {
      if (it->second) return it->second.get();
    }
  }
  return nullptr;
}

std::optional<uuid> WindowData::currentViewUidAtCursor(const glm::vec2& windowPos) const
{
  if (m_layouts.empty()) {
    return std::nullopt;
  }

  if (m_currentLayout >= m_layouts.size()) {
    return std::nullopt;
  }

  const auto& layout = m_layouts.at(m_currentLayout);
  std::vector<viewer::FrameHitTarget> frames;
  frames.reserve(layout.orderedViewUids().size());

  for (const auto& viewUid : layout.orderedViewUids()) {
    const auto viewIt = layout.views().find(viewUid);
    if (viewIt == layout.views().end() || !viewIt->second) {
      continue;
    }

    frames.push_back({.m_uid = viewUid, .m_windowClipViewport = viewIt->second->windowClipViewport()});
  }

  return viewer::findFrameAtWindowPosition(m_viewport, windowPos, frames);
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

std::string WindowData::layoutDisplayName(std::size_t index) const
{
  if (index >= m_layouts.size()) {
    return "Layout";
  }

  const Layout& layout = m_layouts.at(index);
  if (LayoutKind::MultiImageGrid == layout.kind() || LayoutKind::AxCorSagByImage == layout.kind()) {
    return tiledLayoutDisplayName(layout, "Grid");
  }
  if (LayoutKind::Custom == layout.kind() && layout.isLightbox()) {
    return tiledLayoutDisplayName(layout, "Lightbox");
  }
  if (LayoutKind::Custom == layout.kind()) {
    return layout.displayName();
  }
  return std::string{layout::layoutDisplayName(layout.kind(), layout.isLightbox())};
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
  if (m_activeViewUid && !getCurrentView(*m_activeViewUid)) {
    m_activeViewUid = std::nullopt;
  }
}

void WindowData::cycleCurrentLayout(int step)
{
  if (m_layouts.empty()) {
    return;
  }

  const int i = static_cast<int>(currentLayoutIndex());
  const int N = static_cast<int>(numLayouts());
  setCurrentLayoutIndex(static_cast<size_t>((N + i + step) % N));
}

void WindowData::moveLayout(std::size_t fromIndex, std::size_t toIndex)
{
  if (fromIndex >= m_layouts.size() || toIndex >= m_layouts.size() || fromIndex == toIndex) {
    return;
  }

  const uuid currentLayoutUid = m_layouts.at(m_currentLayout).uid();
  Layout layout = std::move(m_layouts.at(fromIndex));
  m_layouts.erase(m_layouts.begin() + static_cast<std::ptrdiff_t>(fromIndex));
  m_layouts.insert(m_layouts.begin() + static_cast<std::ptrdiff_t>(toIndex), std::move(layout));

  auto currentLayoutIt = std::find_if(m_layouts.begin(), m_layouts.end(), [&currentLayoutUid](const Layout& candidate) {
    return candidate.uid() == currentLayoutUid;
  });
  m_currentLayout = currentLayoutIt == m_layouts.end()
                      ? 0
                      : static_cast<std::size_t>(std::distance(m_layouts.begin(), currentLayoutIt));
  if (m_activeViewUid && !getCurrentView(*m_activeViewUid)) {
    m_activeViewUid = std::nullopt;
  }
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

  SPDLOG_TRACE("Setting content scale ratio to {}x{}", scale.x, scale.y);
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

uuid_range_t WindowData::cameraSyncGroupViewUids(CameraSyncMode mode, const uuid& syncGroupUid) const
{
  if (m_currentLayout >= m_layouts.size()) {
    return {};
  }

  const auto& currentLayout = m_layouts.at(m_currentLayout);
  if (const auto* group = currentLayout.getCameraSyncGroup(mode, syncGroupUid)) {
    return uuid_range_t{group->begin(), group->end()};
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

  for (auto& viewUid : currentViewUids()) {
    View* view = getCurrentView(viewUid);
    if (!view) {
      continue;
    }

    view->setRenderedImages(renderedImages, s_filterAgainstDefaults);
    view->setMetricImages(metricImages);
  }
}

void WindowData::applyViewRenderModeAndProjectionToAllCurrentViews(const uuid& referenceViewUid)
{
  const View* referenceView = getCurrentView(referenceViewUid);
  if (!referenceView) {
    return;
  }

  const auto renderMode = referenceView->renderMode();
  const auto ipMode = referenceView->intensityProjectionMode();

  for (auto& viewUid : currentViewUids()) {
    View* view = getCurrentView(viewUid);
    if (!view) {
      continue;
    }

    view->setRenderMode(renderMode);
    view->setIntensityProjectionMode(ipMode);
  }
}

std::vector<uuid> WindowData::findCurrentViewsWithNormal(const glm::vec3& worldNormal) const
{
  // Angle threshold (in degrees) for checking whether two vectors are parallel
  static constexpr float sk_parallelThreshold_degrees = 0.1f;

  std::vector<uuid> viewUids;

  for (auto& viewUid : currentViewUids()) {
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
  const auto viewUids = currentViewUids();
  if (viewUids.empty()) {
    spdlog::error("The current layout has no views");
    throw_debug("The current layout has no views")
  }

  uuid largestViewUid = viewUids.front();
  const View* largestView = getCurrentView(largestViewUid);

  if (!largestView) {
    spdlog::error("The current layout has no views");
    throw_debug("The current layout has no views")
  }

  std::vector<viewer::FrameHitTarget> frames;
  frames.reserve(viewUids.size());
  frames.push_back({.m_uid = largestViewUid, .m_windowClipViewport = largestView->windowClipViewport()});

  for (auto& viewUid : viewUids) {
    if (viewUid == largestViewUid) {
      continue;
    }

    const View* view = getCurrentView(viewUid);
    if (!view) {
      continue;
    }

    frames.push_back({.m_uid = viewUid, .m_windowClipViewport = view->windowClipViewport()});
  }

  return viewer::findLargestFrameByArea(frames).value();
}

void WindowData::recomputeCameraAspectRatios()
{
  for (auto& layout : m_layouts) {
    for (auto& [viewUid, view] : layout.views()) {
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
      const float cameraAspectRatio = m_viewport.aspectRatio() * viewAspect;
      view->sliceCamera().setAspectRatio(cameraAspectRatio);
      view->threeDCamera().setAspectRatio(cameraAspectRatio);
    }
  }
}

void WindowData::updateAllViews()
{
  recomputeCameraAspectRatios();
}
