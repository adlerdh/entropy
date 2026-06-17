#include "viewer/FrameImageSelection.h"

#include "viewer/ImageSelection.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>

namespace viewer
{
namespace
{

constexpr std::size_t sk_maxMetricImages = 2;

std::optional<std::size_t> imageIndex(const uuid_range_t& orderedImageUids, const uuids::uuid& imageUid)
{
  const auto it = std::find(orderedImageUids.begin(), orderedImageUids.end(), imageUid);
  if (it == orderedImageUids.end()) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(std::distance(orderedImageUids.begin(), it));
}

bool contains(const std::list<uuids::uuid>& imageUids, const uuids::uuid& imageUid)
{
  return std::find(imageUids.begin(), imageUids.end(), imageUid) != imageUids.end();
}

void insertByImageOrder(
  std::list<uuids::uuid>& imageUids,
  const uuids::uuid& imageUid,
  const uuid_range_t& orderedImageUids)
{
  const auto index = imageIndex(orderedImageUids, imageUid);
  if (!index) {
    return;
  }

  for (auto it = imageUids.begin(); it != imageUids.end(); ++it) {
    if (const auto existingIndex = imageIndex(orderedImageUids, *it)) {
      if (*index < *existingIndex) {
        imageUids.insert(it, imageUid);
        return;
      }
    }
  }

  imageUids.push_back(imageUid);
}

} // namespace

bool FrameImageSelection::isImageRendered(const uuids::uuid& imageUid) const
{
  return contains(m_renderedImageUids, imageUid);
}

void FrameImageSelection::setImageRendered(const uuids::uuid& imageUid, uuid_range_t orderedImageUids, bool visible)
{
  if (!visible) {
    m_renderedImageUids.remove(imageUid);
    return;
  }

  if (contains(m_renderedImageUids, imageUid)) {
    return;
  }

  insertByImageOrder(m_renderedImageUids, imageUid, orderedImageUids);
}

const std::list<uuids::uuid>& FrameImageSelection::renderedImages() const
{
  return m_renderedImageUids;
}

void FrameImageSelection::setRenderedImages(const std::list<uuids::uuid>& imageUids, bool filterByDefaults)
{
  if (filterByDefaults) {
    m_renderedImageUids = image_selection::filteredDefaultRenderedImages(
      imageUids,
      m_defaultRenderAllImages,
      m_preferredDefaultRenderedImages);
  }
  else {
    m_renderedImageUids = imageUids;
  }
}

bool FrameImageSelection::isImageUsedForMetric(const uuids::uuid& imageUid) const
{
  return contains(m_metricImageUids, imageUid);
}

void FrameImageSelection::setImageUsedForMetric(
  const uuids::uuid& imageUid,
  uuid_range_t orderedImageUids,
  bool visible)
{
  if (!visible) {
    m_metricImageUids.remove(imageUid);
    return;
  }

  if (contains(m_metricImageUids, imageUid)) {
    return;
  }

  if (!imageIndex(orderedImageUids, imageUid)) {
    return;
  }

  if (m_metricImageUids.size() >= sk_maxMetricImages) {
    m_metricImageUids.erase(std::prev(m_metricImageUids.end()));
  }

  insertByImageOrder(m_metricImageUids, imageUid, orderedImageUids);
}

const std::list<uuids::uuid>& FrameImageSelection::metricImages() const
{
  return m_metricImageUids;
}

void FrameImageSelection::setMetricImages(const std::list<uuids::uuid>& imageUids)
{
  m_metricImageUids = imageUids;
}

const std::list<uuids::uuid>& FrameImageSelection::visibleImages(ViewRenderMode renderMode) const
{
  static const std::list<uuids::uuid> sk_noImages;

  switch (renderMode) {
    case ViewRenderMode::Image: {
      return renderedImages();
    }
    case ViewRenderMode::Disabled: {
      return sk_noImages;
    }
    default: {
      return metricImages();
    }
  }
}

void FrameImageSelection::setPreferredDefaultRenderedImages(std::set<std::size_t> imageIndices)
{
  m_preferredDefaultRenderedImages = std::move(imageIndices);
}

const std::set<std::size_t>& FrameImageSelection::preferredDefaultRenderedImages() const
{
  return m_preferredDefaultRenderedImages;
}

void FrameImageSelection::setDefaultRenderAllImages(bool renderAll)
{
  m_defaultRenderAllImages = renderAll;
}

bool FrameImageSelection::defaultRenderAllImages() const
{
  return m_defaultRenderAllImages;
}

void FrameImageSelection::updateImageOrdering(uuid_range_t orderedImageUids)
{
  m_renderedImageUids = image_selection::reorderSelectedImages(m_renderedImageUids, orderedImageUids);
  m_metricImageUids = image_selection::reorderSelectedImages(m_metricImageUids, orderedImageUids, sk_maxMetricImages);
}

} // namespace viewer
