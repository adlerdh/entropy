#include "windowing/LayoutSerialization.h"

#include "layout/ImageIndexMapping.h"
#include "layout/LayoutKindInfo.h"
#include "layout/SyncGroupIndexMap.h"
#include "logic/app/CrosshairsState.h"
#include "logic/camera/Camera.h"
#include "logic/camera/Camera3DControls.h"
#include "logic/camera/CameraTypes.h"
#include "ui/UiControls.h"
#include "viewer/LayoutTypes.h"
#include "viewer/ViewModes.h"
#include "viewer/ViewTypes.h"
#include "windowing/ControlFrame.h"
#include "windowing/View.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
using uuid = uuids::uuid;

template<typename Enum>
Enum enumFromInt(int value, Enum fallback)
{
  if (value < 0 || value >= static_cast<int>(Enum::NumElements)) {
    return fallback;
  }
  return static_cast<Enum>(value);
}

ViewOffsetMode viewOffsetModeFromInt(int value)
{
  switch (static_cast<ViewOffsetMode>(value)) {
    case ViewOffsetMode::RelativeToRefImageScrolls:
    case ViewOffsetMode::RelativeToImageScrolls:
    case ViewOffsetMode::Absolute:
    case ViewOffsetMode::None:
      return static_cast<ViewOffsetMode>(value);
  }
  return ViewOffsetMode::None;
}

ProjectionType projectionTypeFromInt(int value)
{
  switch (static_cast<ProjectionType>(value)) {
    case ProjectionType::Orthographic:
    case ProjectionType::Perspective:
      return static_cast<ProjectionType>(value);
  }
  return ProjectionType::Perspective;
}

camera3d::OrbitTargetMode orbitTargetModeFromInt(int value)
{
  switch (static_cast<camera3d::OrbitTargetMode>(value)) {
    case camera3d::OrbitTargetMode::VisibleImages:
    case camera3d::OrbitTargetMode::Crosshairs:
      return static_cast<camera3d::OrbitTargetMode>(value);
  }
  return camera3d::OrbitTargetMode::VisibleImages;
}

float saneZoom(float value)
{
  return std::isfinite(value) ? std::clamp(value, 0.01f, 100.0f) : 1.0f;
}

std::optional<uuid> syncGroupUidForIndex(
  Layout& layout,
  CameraSyncMode mode,
  layout::SyncGroupIndexMap& syncGroups,
  const std::optional<std::size_t>& syncGroupIndex)
{
  if (!syncGroupIndex) {
    return std::nullopt;
  }

  if (const auto existingGroupUid = syncGroups.groupUidForIndex(syncGroupIndex)) {
    return existingGroupUid;
  }

  const uuid syncGroupUid = layout.addCameraSyncGroup(mode);
  return syncGroups.bindGroupIndex(*syncGroupIndex, syncGroupUid);
}

layout::ImageSelectionSpec createImageSelectionSpec(const uuid_range_t& orderedImageUids, const ControlFrame& frame)
{
  layout::ImageSelectionSpec selection;
  selection.m_renderedImageIndices = layout::imageIndicesForUids(orderedImageUids, frame.renderedImages());
  selection.m_volumeRenderedImageIndices = layout::imageIndicesForUids(orderedImageUids, frame.volumeRenderedImages());
  selection.m_metricImageIndices = layout::imageIndicesForUids(orderedImageUids, frame.metricImages());
  return selection;
}

void applyImageSelectionSpec(
  ControlFrame& frame,
  const uuid_range_t& orderedImageUids,
  const layout::ImageSelectionSpec& selection)
{
  frame.setRenderedImages(layout::imageUidsForIndices(orderedImageUids, selection.m_renderedImageIndices), false);
  frame.setVolumeRenderedImages(layout::imageUidsForIndices(orderedImageUids, selection.m_volumeRenderedImageIndices));
  if (ViewRenderMode::VolumeRender == frame.renderMode() && frame.volumeRenderedImages().empty()) {
    frame.setVolumeRenderedImages(frame.renderedImages());
  }
  frame.setMetricImages(layout::imageUidsForIndices(orderedImageUids, selection.m_metricImageIndices));
}

std::optional<std::size_t> firstRenderedImageIndex(const layout::ImageSelectionSpec& selection)
{
  if (selection.m_renderedImageIndices.empty()) {
    return std::nullopt;
  }
  return selection.m_renderedImageIndices.front();
}

std::optional<std::size_t> lightboxOffsetImageIndex(
  const layout::LayoutSpec& layoutSpec,
  const layout::ViewSpec& viewSpec)
{
  if (viewSpec.m_offsetImageIndex) {
    return viewSpec.m_offsetImageIndex;
  }
  if (const auto viewImageIndex = firstRenderedImageIndex(viewSpec.m_imageSelection)) {
    return viewImageIndex;
  }
  if (const auto layoutImageIndex = firstRenderedImageIndex(layoutSpec.m_imageSelection)) {
    return layoutImageIndex;
  }
  return std::nullopt;
}

ViewOffsetMode lightboxOffsetModeForImageIndex(const std::optional<std::size_t>& imageIndex)
{
  return (imageIndex && 0 == *imageIndex) ? ViewOffsetMode::RelativeToRefImageScrolls
                                          : ViewOffsetMode::RelativeToImageScrolls;
}

void normalizeLightboxOffsetSpec(layout::ViewSpec& viewSpec, const layout::LayoutSpec& layoutSpec)
{
  if (!layoutSpec.m_isLightbox) {
    return;
  }

  const std::optional<std::size_t> imageIndex = lightboxOffsetImageIndex(layoutSpec, viewSpec);
  viewSpec.m_offsetMode = static_cast<int>(lightboxOffsetModeForImageIndex(imageIndex));
  viewSpec.m_offsetImageIndex = imageIndex;
  viewSpec.m_absoluteOffset = 0.0f;
}

bool containsApprox(const std::vector<float>& values, float value)
{
  return std::any_of(values.begin(), values.end(), [value](float existing) {
    return std::fabs(existing - value) < 1.0e-4f;
  });
}

std::optional<std::pair<std::size_t, std::size_t>> regularGridDimensions(const Layout& layout)
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

  const std::size_t columns = leftEdges.size();
  const std::size_t rows = bottomEdges.size();
  return columns * rows == layout.orderedViewUids().size() ? std::optional{std::pair{columns, rows}} : std::nullopt;
}

std::optional<std::size_t> firstImageIndex(const layout::ImageSelectionSpec& selection)
{
  if (!selection.m_renderedImageIndices.empty()) {
    return selection.m_renderedImageIndices.front();
  }
  return std::nullopt;
}

bool isOneUpLayoutSpec(const layout::LayoutSpec& layoutSpec)
{
  return static_cast<int>(LayoutKind::OneUp) == layoutSpec.m_kind;
}

layout::GridSpec gridSpecForLayout(const Layout& layout, const layout::LayoutSpec& layoutSpec)
{
  layout::GridSpec grid;
  if (const auto dimensions = regularGridDimensions(layout)) {
    grid.m_columns = dimensions->first;
    grid.m_rows = dimensions->second;
  }

  grid.m_offsetViews = layoutSpec.m_isLightbox;
  if (!isOneUpLayoutSpec(layoutSpec)) {
    grid.m_imageIndex = firstImageIndex(layoutSpec.m_imageSelection);
    if (!grid.m_imageIndex && !layoutSpec.m_preferredDefaultRenderedImages.empty()) {
      grid.m_imageIndex = *layoutSpec.m_preferredDefaultRenderedImages.begin();
    }
  }
  return grid;
}

layout::ImageSelectionSpec defaultMetricImageSelection(std::size_t imageCount)
{
  layout::ImageSelectionSpec selection;
  if (imageCount > 0) {
    selection.m_metricImageIndices.push_back(0);
  }
  if (imageCount > 1) {
    selection.m_metricImageIndices.push_back(1);
  }
  return selection;
}

layout::ViewSpec expectedGridViewSpec(
  const layout::LayoutSpec& layoutSpec,
  const layout::GridSpec& grid,
  std::size_t imageCount,
  std::size_t index)
{
  layout::ViewSpec viewSpec;
  const std::size_t column = index % grid.m_columns;
  const std::size_t row = index / grid.m_columns;
  const float width = 2.0f / static_cast<float>(grid.m_columns);
  const float height = 2.0f / static_cast<float>(grid.m_rows);

  viewSpec.m_left = -1.0f + static_cast<float>(column) * width;
  viewSpec.m_bottom = -1.0f + static_cast<float>(row) * height;
  viewSpec.m_width = width;
  viewSpec.m_height = height;
  viewSpec.m_viewType = layoutSpec.m_viewType;
  viewSpec.m_renderMode = layoutSpec.m_renderMode;
  viewSpec.m_intensityProjectionMode = layoutSpec.m_intensityProjectionMode;

  viewSpec.m_offsetMode = 0;
  viewSpec.m_offsetImageIndex =
    isOneUpLayoutSpec(layoutSpec) ? std::nullopt : std::optional<std::size_t>{grid.m_imageIndex.value_or(0)};
  if (grid.m_offsetViews) {
    const int offsetStep = static_cast<int>(index) - static_cast<int>(grid.m_columns * grid.m_rows) / 2;
    viewSpec.m_relativeOffsetSteps = grid.m_absoluteOffsetStep ? 0 : offsetStep;
    viewSpec.m_absoluteOffset =
      grid.m_absoluteOffsetStep ? static_cast<float>(offsetStep) * *grid.m_absoluteOffsetStep : 0.0f;
    if (grid.m_absoluteOffsetStep) {
      viewSpec.m_offsetMode = 2;
    }
    else if (grid.m_imageIndex && 0 != *grid.m_imageIndex) {
      viewSpec.m_offsetMode = 1;
    }
  }

  viewSpec.m_rotationSyncGroup = 0;
  viewSpec.m_translationSyncGroup = 0;
  viewSpec.m_zoomSyncGroup = 0;
  viewSpec.m_rotationSyncMembershipGroup = 0;
  viewSpec.m_translationSyncMembershipGroup = 0;
  viewSpec.m_zoomSyncMembershipGroup = 0;

  if (layoutSpec.m_isLightbox) {
    viewSpec.m_defaultRenderAllImages = true;
  }
  else if (isOneUpLayoutSpec(layoutSpec)) {
    viewSpec.m_preferredDefaultRenderedImages = layoutSpec.m_preferredDefaultRenderedImages;
    viewSpec.m_defaultRenderAllImages = true;
    viewSpec.m_imageSelection = layoutSpec.m_imageSelection;
  }
  else {
    viewSpec.m_preferredDefaultRenderedImages = {index};
    viewSpec.m_defaultRenderAllImages = false;
    if (index < imageCount) {
      viewSpec.m_imageSelection.m_renderedImageIndices = {index};
    }
    viewSpec.m_imageSelection.m_metricImageIndices = defaultMetricImageSelection(imageCount).m_metricImageIndices;
  }

  return viewSpec;
}

std::vector<layout::ViewSpec>
expectedGridViewSpecs(const layout::LayoutSpec& layoutSpec, const layout::GridSpec& grid, std::size_t imageCount)
{
  std::vector<layout::ViewSpec> views;
  views.reserve(grid.m_columns * grid.m_rows);
  for (std::size_t index = 0; index < grid.m_columns * grid.m_rows; ++index) {
    views.emplace_back(expectedGridViewSpec(layoutSpec, grid, imageCount, index));
  }
  return views;
}

std::vector<layout::ViewSpec>
mergeGridViewOverrides(const layout::LayoutSpec& spec, const layout::GridSpec& grid, std::size_t imageCount)
{
  std::vector<layout::ViewSpec> views = expectedGridViewSpecs(spec, grid, imageCount);
  for (std::size_t overrideIndex = 0; overrideIndex < spec.m_views.size(); ++overrideIndex) {
    const auto viewIndex = spec.m_views.at(overrideIndex).m_index.value_or(overrideIndex);
    if (viewIndex >= views.size()) {
      spdlog::warn("Skipping serialized grid view override {} because the grid has {} views", viewIndex, views.size());
      continue;
    }

    const layout::ViewSpec defaults;
    const layout::ViewSpec& override = spec.m_views.at(overrideIndex);
    layout::ViewSpec& view = views.at(viewIndex);

    // Grid layouts derive their viewport from rows and columns. Every other field below is a sparse override:
    // the default ViewSpec value means "keep the generated grid value".
    if (override.m_viewType != defaults.m_viewType) {
      view.m_viewType = override.m_viewType;
    }
    if (override.m_renderMode != defaults.m_renderMode) {
      view.m_renderMode = override.m_renderMode;
    }
    if (override.m_intensityProjectionMode != defaults.m_intensityProjectionMode) {
      view.m_intensityProjectionMode = override.m_intensityProjectionMode;
    }
    if (
      override.m_offsetMode != defaults.m_offsetMode || override.m_absoluteOffset != defaults.m_absoluteOffset ||
      override.m_relativeOffsetSteps != defaults.m_relativeOffsetSteps ||
      override.m_offsetImageIndex != defaults.m_offsetImageIndex)
    {
      view.m_offsetMode = override.m_offsetMode;
      view.m_absoluteOffset = override.m_absoluteOffset;
      view.m_relativeOffsetSteps = override.m_relativeOffsetSteps;
      view.m_offsetImageIndex = override.m_offsetImageIndex;
    }
    if (
      override.m_rotationSyncGroup != defaults.m_rotationSyncGroup ||
      override.m_translationSyncGroup != defaults.m_translationSyncGroup ||
      override.m_zoomSyncGroup != defaults.m_zoomSyncGroup ||
      override.m_rotationSyncMembershipGroup != defaults.m_rotationSyncMembershipGroup ||
      override.m_translationSyncMembershipGroup != defaults.m_translationSyncMembershipGroup ||
      override.m_zoomSyncMembershipGroup != defaults.m_zoomSyncMembershipGroup)
    {
      view.m_rotationSyncGroup = override.m_rotationSyncGroup;
      view.m_translationSyncGroup = override.m_translationSyncGroup;
      view.m_zoomSyncGroup = override.m_zoomSyncGroup;
      view.m_rotationSyncMembershipGroup = override.m_rotationSyncMembershipGroup;
      view.m_translationSyncMembershipGroup = override.m_translationSyncMembershipGroup;
      view.m_zoomSyncMembershipGroup = override.m_zoomSyncMembershipGroup;
    }
    if (
      override.m_preferredDefaultRenderedImages != defaults.m_preferredDefaultRenderedImages ||
      override.m_defaultRenderAllImages != defaults.m_defaultRenderAllImages)
    {
      view.m_preferredDefaultRenderedImages = override.m_preferredDefaultRenderedImages;
      view.m_defaultRenderAllImages = override.m_defaultRenderAllImages;
    }
    if (override.m_imageSelection != defaults.m_imageSelection) {
      view.m_imageSelection = override.m_imageSelection;
    }
    if (
      override.m_threeDProjectionType != defaults.m_threeDProjectionType ||
      override.m_threeDOrbitTargetMode != defaults.m_threeDOrbitTargetMode ||
      override.m_threeDCameraFollowsCrosshairs != defaults.m_threeDCameraFollowsCrosshairs ||
      override.m_threeDPerspectiveZoom != defaults.m_threeDPerspectiveZoom ||
      override.m_threeDOrthographicZoom != defaults.m_threeDOrthographicZoom)
    {
      view.m_threeDProjectionType = override.m_threeDProjectionType;
      view.m_threeDOrbitTargetMode = override.m_threeDOrbitTargetMode;
      view.m_threeDCameraFollowsCrosshairs = override.m_threeDCameraFollowsCrosshairs;
      view.m_threeDPerspectiveZoom = override.m_threeDPerspectiveZoom;
      view.m_threeDOrthographicZoom = override.m_threeDOrthographicZoom;
    }
  }
  return views;
}

std::optional<layout::ViewSpec>
gridViewOverride(const layout::ViewSpec& viewSpec, const layout::ViewSpec& expected, std::size_t index)
{
  layout::ViewSpec override;
  bool hasOverride = false;

  if (viewSpec.m_viewType != expected.m_viewType) {
    override.m_viewType = viewSpec.m_viewType;
    hasOverride = true;
  }
  if (viewSpec.m_renderMode != expected.m_renderMode) {
    override.m_renderMode = viewSpec.m_renderMode;
    hasOverride = true;
  }
  if (viewSpec.m_intensityProjectionMode != expected.m_intensityProjectionMode) {
    override.m_intensityProjectionMode = viewSpec.m_intensityProjectionMode;
    hasOverride = true;
  }
  if (
    viewSpec.m_offsetMode != expected.m_offsetMode || viewSpec.m_absoluteOffset != expected.m_absoluteOffset ||
    viewSpec.m_relativeOffsetSteps != expected.m_relativeOffsetSteps ||
    viewSpec.m_offsetImageIndex != expected.m_offsetImageIndex)
  {
    override.m_offsetMode = viewSpec.m_offsetMode;
    override.m_absoluteOffset = viewSpec.m_absoluteOffset;
    override.m_relativeOffsetSteps = viewSpec.m_relativeOffsetSteps;
    override.m_offsetImageIndex = viewSpec.m_offsetImageIndex;
    hasOverride = true;
  }
  if (
    viewSpec.m_rotationSyncGroup != expected.m_rotationSyncGroup ||
    viewSpec.m_translationSyncGroup != expected.m_translationSyncGroup ||
    viewSpec.m_zoomSyncGroup != expected.m_zoomSyncGroup ||
    viewSpec.m_rotationSyncMembershipGroup != expected.m_rotationSyncMembershipGroup ||
    viewSpec.m_translationSyncMembershipGroup != expected.m_translationSyncMembershipGroup ||
    viewSpec.m_zoomSyncMembershipGroup != expected.m_zoomSyncMembershipGroup)
  {
    override.m_rotationSyncGroup = viewSpec.m_rotationSyncGroup;
    override.m_translationSyncGroup = viewSpec.m_translationSyncGroup;
    override.m_zoomSyncGroup = viewSpec.m_zoomSyncGroup;
    override.m_rotationSyncMembershipGroup = viewSpec.m_rotationSyncMembershipGroup;
    override.m_translationSyncMembershipGroup = viewSpec.m_translationSyncMembershipGroup;
    override.m_zoomSyncMembershipGroup = viewSpec.m_zoomSyncMembershipGroup;
    hasOverride = true;
  }
  if (
    viewSpec.m_preferredDefaultRenderedImages != expected.m_preferredDefaultRenderedImages ||
    viewSpec.m_defaultRenderAllImages != expected.m_defaultRenderAllImages)
  {
    override.m_preferredDefaultRenderedImages = viewSpec.m_preferredDefaultRenderedImages;
    override.m_defaultRenderAllImages = viewSpec.m_defaultRenderAllImages;
    hasOverride = true;
  }
  if (viewSpec.m_imageSelection != expected.m_imageSelection) {
    override.m_imageSelection = viewSpec.m_imageSelection;
    hasOverride = true;
  }
  if (
    viewSpec.m_threeDProjectionType != expected.m_threeDProjectionType ||
    viewSpec.m_threeDOrbitTargetMode != expected.m_threeDOrbitTargetMode ||
    viewSpec.m_threeDCameraFollowsCrosshairs != expected.m_threeDCameraFollowsCrosshairs ||
    viewSpec.m_threeDPerspectiveZoom != expected.m_threeDPerspectiveZoom ||
    viewSpec.m_threeDOrthographicZoom != expected.m_threeDOrthographicZoom)
  {
    override.m_threeDProjectionType = viewSpec.m_threeDProjectionType;
    override.m_threeDOrbitTargetMode = viewSpec.m_threeDOrbitTargetMode;
    override.m_threeDCameraFollowsCrosshairs = viewSpec.m_threeDCameraFollowsCrosshairs;
    override.m_threeDPerspectiveZoom = viewSpec.m_threeDPerspectiveZoom;
    override.m_threeDOrthographicZoom = viewSpec.m_threeDOrthographicZoom;
    hasOverride = true;
  }

  if (!hasOverride) {
    return std::nullopt;
  }

  override.m_index = index;
  return override;
}

} // namespace

namespace layout
{

LayoutSpec createLayoutSpec(const Layout& layout, const uuid_range_t& orderedImageUids)
{
  LayoutSpec layoutSpec;
  layoutSpec.m_kind = static_cast<int>(layout.kind());
  if (LayoutKind::Custom == layout.kind() && !layout.isLightbox()) {
    layoutSpec.m_displayName = layout.displayName();
  }
  layoutSpec.m_isLightbox = layout.isLightbox();
  layoutSpec.m_viewType = static_cast<int>(layout.viewType());
  layoutSpec.m_renderMode = static_cast<int>(layout.renderMode());
  layoutSpec.m_intensityProjectionMode = static_cast<int>(layout.intensityProjectionMode());
  layoutSpec.m_preferredDefaultRenderedImages = layout.preferredDefaultRenderedImages();
  layoutSpec.m_defaultRenderAllImages = layout.defaultRenderAllImages();
  layoutSpec.m_imageSelection = createImageSelectionSpec(orderedImageUids, layout);

  SyncGroupIndexMap rotationGroups;
  SyncGroupIndexMap translationGroups;
  SyncGroupIndexMap zoomGroups;

  for (const auto& viewUid : layout.orderedViewUids()) {
    const auto viewIt = layout.views().find(viewUid);
    if (viewIt == layout.views().end() || !viewIt->second) {
      continue;
    }

    const View& view = *viewIt->second;
    const glm::vec4& viewport = view.windowClipViewport();

    ViewSpec viewSpec;
    viewSpec.m_left = viewport.x;
    viewSpec.m_bottom = viewport.y;
    viewSpec.m_width = viewport.z;
    viewSpec.m_height = viewport.w;
    viewSpec.m_viewType = static_cast<int>(view.viewType());
    viewSpec.m_renderMode = static_cast<int>(view.renderMode());
    viewSpec.m_intensityProjectionMode = static_cast<int>(view.intensityProjectionMode());

    const ViewOffsetSetting& offset = view.offsetSetting();
    viewSpec.m_offsetMode = static_cast<int>(offset.m_offsetMode);
    viewSpec.m_absoluteOffset = offset.m_absoluteOffset;
    viewSpec.m_relativeOffsetSteps = offset.m_relativeOffsetSteps;
    viewSpec.m_offsetImageIndex = imageIndexForUid(orderedImageUids, offset.m_offsetImage);
    normalizeLightboxOffsetSpec(viewSpec, layoutSpec);

    viewSpec.m_rotationSyncGroup = rotationGroups.indexForGroupUid(view.cameraRotationSyncGroupUid());
    viewSpec.m_translationSyncGroup = translationGroups.indexForGroupUid(view.cameraTranslationSyncGroupUid());
    viewSpec.m_zoomSyncGroup = zoomGroups.indexForGroupUid(view.cameraZoomSyncGroupUid());
    viewSpec.m_rotationSyncMembershipGroup =
      rotationGroups.indexForGroupUid(layout.cameraSyncGroupUidContainingView(CameraSyncMode::Rotation, viewUid));
    viewSpec.m_translationSyncMembershipGroup =
      translationGroups.indexForGroupUid(layout.cameraSyncGroupUidContainingView(CameraSyncMode::Translation, viewUid));
    viewSpec.m_zoomSyncMembershipGroup =
      zoomGroups.indexForGroupUid(layout.cameraSyncGroupUidContainingView(CameraSyncMode::Zoom, viewUid));

    viewSpec.m_preferredDefaultRenderedImages = view.preferredDefaultRenderedImages();
    viewSpec.m_defaultRenderAllImages = view.defaultRenderAllImages();
    viewSpec.m_imageSelection = createImageSelectionSpec(orderedImageUids, view);
    viewSpec.m_threeDProjectionType = static_cast<int>(view.threeDState().m_projectionType);
    viewSpec.m_threeDOrbitTargetMode = static_cast<int>(view.threeDState().m_orbitTargetMode);
    viewSpec.m_threeDCameraFollowsCrosshairs = view.threeDState().m_viewPositionFollowsCrosshairs;
    viewSpec.m_threeDPerspectiveZoom = view.threeDState().m_perspectiveZoom;
    viewSpec.m_threeDOrthographicZoom = view.threeDState().m_orthographicZoom;

    layoutSpec.m_views.emplace_back(std::move(viewSpec));
  }

  const auto dimensions = regularGridDimensions(layout);
  if (dimensions && layout::canUseRegularGridRecipe(layout.kind())) {
    layoutSpec.m_grid = gridSpecForLayout(layout, layoutSpec);
    const auto expectedViews = expectedGridViewSpecs(layoutSpec, *layoutSpec.m_grid, orderedImageUids.size());
    std::vector<ViewSpec> overrides;
    overrides.reserve(layoutSpec.m_views.size());
    for (std::size_t viewIndex = 0; viewIndex < layoutSpec.m_views.size() && viewIndex < expectedViews.size();
         ++viewIndex)
    {
      if (
        auto viewOverride = gridViewOverride(layoutSpec.m_views.at(viewIndex), expectedViews.at(viewIndex), viewIndex))
      {
        overrides.emplace_back(std::move(*viewOverride));
      }
    }
    layoutSpec.m_views = std::move(overrides);
  }

  return layoutSpec;
}

std::vector<LayoutSpec> createLayoutSpecs(const std::vector<Layout>& layouts, const uuid_range_t& orderedImageUids)
{
  std::vector<LayoutSpec> specs;
  specs.reserve(layouts.size());

  for (const auto& layout : layouts) {
    specs.emplace_back(createLayoutSpec(layout, orderedImageUids));
  }

  return specs;
}

Layout instantiateLayoutSpec(
  const LayoutSpec& spec,
  const uuid_range_t& orderedImageUids,
  const CrosshairsState& crosshairs,
  const ViewAlignmentMode& viewAlignment,
  const ViewConvention& viewConvention)
{
  Layout layout(spec.m_isLightbox);
  layout.setKind(enumFromInt(spec.m_kind, LayoutKind::Custom));
  if (!spec.m_displayName.empty()) {
    layout.setDisplayName(spec.m_displayName);
  }
  layout.setViewType(enumFromInt(spec.m_viewType, ViewType::Axial));
  layout.setRenderMode(enumFromInt(spec.m_renderMode, ViewRenderMode::Image));
  layout.setIntensityProjectionMode(enumFromInt(spec.m_intensityProjectionMode, IntensityProjectionMode::None));
  layout.setPreferredDefaultRenderedImages(spec.m_preferredDefaultRenderedImages);
  layout.setDefaultRenderAllImages(spec.m_defaultRenderAllImages);

  SyncGroupIndexMap rotationGroups;
  SyncGroupIndexMap translationGroups;
  SyncGroupIndexMap zoomGroups;

  const std::vector<ViewSpec> views =
    spec.m_grid ? mergeGridViewOverrides(spec, *spec.m_grid, orderedImageUids.size()) : spec.m_views;
  for (const auto& viewSpec : views) {
    auto normalizedViewSpec = viewSpec;
    normalizeLightboxOffsetSpec(normalizedViewSpec, spec);

    ViewOffsetSetting offsetSetting;
    offsetSetting.m_offsetMode = viewOffsetModeFromInt(normalizedViewSpec.m_offsetMode);
    offsetSetting.m_absoluteOffset = normalizedViewSpec.m_absoluteOffset;
    offsetSetting.m_relativeOffsetSteps = normalizedViewSpec.m_relativeOffsetSteps;
    offsetSetting.m_offsetImage = imageUidForIndex(orderedImageUids, normalizedViewSpec.m_offsetImageIndex);

    const auto rotationGroupUid =
      syncGroupUidForIndex(layout, CameraSyncMode::Rotation, rotationGroups, normalizedViewSpec.m_rotationSyncGroup);
    const auto translationGroupUid = syncGroupUidForIndex(
      layout,
      CameraSyncMode::Translation,
      translationGroups,
      normalizedViewSpec.m_translationSyncGroup);
    const auto zoomGroupUid =
      syncGroupUidForIndex(layout, CameraSyncMode::Zoom, zoomGroups, normalizedViewSpec.m_zoomSyncGroup);
    const auto rotationMembershipGroupUid = syncGroupUidForIndex(
      layout,
      CameraSyncMode::Rotation,
      rotationGroups,
      normalizedViewSpec.m_rotationSyncMembershipGroup ? normalizedViewSpec.m_rotationSyncMembershipGroup
                                                       : normalizedViewSpec.m_rotationSyncGroup);
    const auto translationMembershipGroupUid = syncGroupUidForIndex(
      layout,
      CameraSyncMode::Translation,
      translationGroups,
      normalizedViewSpec.m_translationSyncMembershipGroup ? normalizedViewSpec.m_translationSyncMembershipGroup
                                                          : normalizedViewSpec.m_translationSyncGroup);
    const auto zoomMembershipGroupUid = syncGroupUidForIndex(
      layout,
      CameraSyncMode::Zoom,
      zoomGroups,
      normalizedViewSpec.m_zoomSyncMembershipGroup ? normalizedViewSpec.m_zoomSyncMembershipGroup
                                                   : normalizedViewSpec.m_zoomSyncGroup);

    auto view = std::make_unique<View>(
      glm::vec4{
        normalizedViewSpec.m_left,
        normalizedViewSpec.m_bottom,
        normalizedViewSpec.m_width,
        normalizedViewSpec.m_height},
      offsetSetting,
      enumFromInt(normalizedViewSpec.m_viewType, ViewType::Axial),
      enumFromInt(normalizedViewSpec.m_renderMode, ViewRenderMode::Image),
      enumFromInt(normalizedViewSpec.m_intensityProjectionMode, IntensityProjectionMode::None),
      UiControls(!spec.m_isLightbox),
      viewConvention,
      crosshairs,
      viewAlignment,
      rotationGroupUid,
      translationGroupUid,
      zoomGroupUid);

    view->setPreferredDefaultRenderedImages(normalizedViewSpec.m_preferredDefaultRenderedImages);
    view->setDefaultRenderAllImages(normalizedViewSpec.m_defaultRenderAllImages);
    applyImageSelectionSpec(*view, orderedImageUids, normalizedViewSpec.m_imageSelection);
    view->setThreeDProjectionType(projectionTypeFromInt(normalizedViewSpec.m_threeDProjectionType));
    view->threeDState().m_projectionType = projectionTypeFromInt(normalizedViewSpec.m_threeDProjectionType);
    view->threeDState().m_orbitTargetMode = orbitTargetModeFromInt(normalizedViewSpec.m_threeDOrbitTargetMode);
    view->threeDState().m_viewPositionFollowsCrosshairs =
      ProjectionType::Perspective == view->threeDState().m_projectionType &&
      normalizedViewSpec.m_threeDCameraFollowsCrosshairs;
    view->threeDState().m_perspectiveZoom = saneZoom(normalizedViewSpec.m_threeDPerspectiveZoom);
    view->threeDState().m_orthographicZoom = saneZoom(normalizedViewSpec.m_threeDOrthographicZoom);
    view->threeDCamera().setZoom(
      ProjectionType::Orthographic == view->threeDState().m_projectionType ? view->threeDState().m_orthographicZoom
                                                                           : view->threeDState().m_perspectiveZoom);
    view->threeDState().m_userMovedCamera = false;

    const uuid viewUid = view->uid();
    layout.addView(std::move(view));
    layout.addViewToCameraSyncGroup(CameraSyncMode::Rotation, rotationMembershipGroupUid, viewUid);
    layout.addViewToCameraSyncGroup(CameraSyncMode::Translation, translationMembershipGroupUid, viewUid);
    layout.addViewToCameraSyncGroup(CameraSyncMode::Zoom, zoomMembershipGroupUid, viewUid);
  }

  layout.ControlFrame::setRenderedImages(
    imageUidsForIndices(orderedImageUids, spec.m_imageSelection.m_renderedImageIndices),
    false);
  layout.ControlFrame::setVolumeRenderedImages(
    imageUidsForIndices(orderedImageUids, spec.m_imageSelection.m_volumeRenderedImageIndices));
  if (ViewRenderMode::VolumeRender == layout.renderMode() && layout.volumeRenderedImages().empty()) {
    layout.ControlFrame::setVolumeRenderedImages(layout.renderedImages());
  }
  layout.ControlFrame::setMetricImages(
    imageUidsForIndices(orderedImageUids, spec.m_imageSelection.m_metricImageIndices));
  return layout;
}

std::vector<Layout> instantiateLayoutSpecs(
  const std::vector<LayoutSpec>& specs,
  const uuid_range_t& orderedImageUids,
  const CrosshairsState& crosshairs,
  const ViewAlignmentMode& viewAlignment,
  const ViewConvention& viewConvention)
{
  std::vector<Layout> layouts;
  layouts.reserve(specs.size());

  for (const auto& spec : specs) {
    if (spec.m_views.empty() && !spec.m_grid) {
      spdlog::warn("Skipping serialized layout with no views");
      continue;
    }
    layouts.emplace_back(instantiateLayoutSpec(spec, orderedImageUids, crosshairs, viewAlignment, viewConvention));
  }

  return layouts;
}

} // namespace layout
