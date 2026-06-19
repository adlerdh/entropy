#include "windowing/LayoutSerialization.h"

#include "layout/ImageIndexMapping.h"
#include "layout/SyncGroupIndexMap.h"
#include "logic/app/CrosshairsState.h"
#include "windowing/View.h"

#include <spdlog/spdlog.h>

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

layout::ImageSelectionSpec createImageSelectionSpec(uuid_range_t orderedImageUids, const ControlFrame& frame)
{
  layout::ImageSelectionSpec selection;
  selection.m_renderedImageIndices = layout::imageIndicesForUids(orderedImageUids, frame.renderedImages());
  selection.m_metricImageIndices = layout::imageIndicesForUids(orderedImageUids, frame.metricImages());
  return selection;
}

void applyImageSelectionSpec(
  ControlFrame& frame,
  uuid_range_t orderedImageUids,
  const layout::ImageSelectionSpec& selection)
{
  frame.setRenderedImages(layout::imageUidsForIndices(orderedImageUids, selection.m_renderedImageIndices), false);
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

} // namespace

namespace layout
{

LayoutSpec createLayoutSpec(const Layout& layout, uuid_range_t orderedImageUids)
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

    layoutSpec.m_views.emplace_back(std::move(viewSpec));
  }

  return layoutSpec;
}

std::vector<LayoutSpec> createLayoutSpecs(const std::vector<Layout>& layouts, uuid_range_t orderedImageUids)
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
  uuid_range_t orderedImageUids,
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

  for (const auto& viewSpec : spec.m_views) {
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

    const uuid viewUid = view->uid();
    layout.addView(std::move(view));
    layout.addViewToCameraSyncGroup(CameraSyncMode::Rotation, rotationMembershipGroupUid, viewUid);
    layout.addViewToCameraSyncGroup(CameraSyncMode::Translation, translationMembershipGroupUid, viewUid);
    layout.addViewToCameraSyncGroup(CameraSyncMode::Zoom, zoomMembershipGroupUid, viewUid);
  }

  layout.ControlFrame::setRenderedImages(
    imageUidsForIndices(orderedImageUids, spec.m_imageSelection.m_renderedImageIndices),
    false);
  layout.ControlFrame::setMetricImages(
    imageUidsForIndices(orderedImageUids, spec.m_imageSelection.m_metricImageIndices));
  return layout;
}

std::vector<Layout> instantiateLayoutSpecs(
  const std::vector<LayoutSpec>& specs,
  uuid_range_t orderedImageUids,
  const CrosshairsState& crosshairs,
  const ViewAlignmentMode& viewAlignment,
  const ViewConvention& viewConvention)
{
  std::vector<Layout> layouts;
  layouts.reserve(specs.size());

  for (const auto& spec : specs) {
    if (spec.m_views.empty()) {
      spdlog::warn("Skipping serialized layout with no views");
      continue;
    }
    layouts.emplace_back(instantiateLayoutSpec(spec, orderedImageUids, crosshairs, viewAlignment, viewConvention));
  }

  return layouts;
}

} // namespace layout
