#pragma once

#include <cstddef>
#include <optional>
#include <set>
#include <vector>

namespace layout
{

struct ImageSelectionSpec
{
  std::vector<std::size_t> m_renderedImageIndices;
  std::vector<std::size_t> m_metricImageIndices;
};

struct ViewSpec
{
  float m_left = -1.0f;
  float m_bottom = -1.0f;
  float m_width = 2.0f;
  float m_height = 2.0f;

  int m_viewType = 0;
  int m_renderMode = 0;
  int m_intensityProjectionMode = 0;

  int m_offsetMode = 3;
  float m_absoluteOffset = 0.0f;
  int m_relativeOffsetSteps = 0;
  std::optional<std::size_t> m_offsetImageIndex = std::nullopt;

  std::optional<std::size_t> m_rotationSyncGroup = std::nullopt;
  std::optional<std::size_t> m_translationSyncGroup = std::nullopt;
  std::optional<std::size_t> m_zoomSyncGroup = std::nullopt;
  std::optional<std::size_t> m_rotationSyncMembershipGroup = std::nullopt;
  std::optional<std::size_t> m_translationSyncMembershipGroup = std::nullopt;
  std::optional<std::size_t> m_zoomSyncMembershipGroup = std::nullopt;

  std::set<std::size_t> m_preferredDefaultRenderedImages;
  bool m_defaultRenderAllImages = true;
  ImageSelectionSpec m_imageSelection;
};

struct LayoutSpec
{
  int m_kind = 0;
  bool m_isLightbox = false;
  int m_viewType = 0;
  int m_renderMode = 0;
  int m_intensityProjectionMode = 0;
  std::set<std::size_t> m_preferredDefaultRenderedImages;
  bool m_defaultRenderAllImages = false;
  ImageSelectionSpec m_imageSelection;
  std::vector<ViewSpec> m_views;
};

} // namespace layout
