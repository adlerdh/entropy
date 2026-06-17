#pragma once

#include "common/UuidRange.h"
#include "viewer/ViewModes.h"

#include <uuid.h>

#include <cstddef>
#include <list>
#include <set>

namespace viewer
{

/**
 * @brief Rendered and metric image selections for one view-like frame.
 */
class FrameImageSelection
{
public:
  /** @brief Is the image selected for direct image rendering? */
  bool isImageRendered(const uuids::uuid& imageUid) const;

  /**
   * @brief Add or remove a rendered image.
   *
   * @param imageUid Image UID to update.
   * @param orderedImageUids Image UIDs in application order.
   * @param visible Whether the image should be rendered.
   */
  void setImageRendered(const uuids::uuid& imageUid, uuid_range_t orderedImageUids, bool visible);

  /** @brief Images selected for direct image rendering, bottom layer first. */
  const std::list<uuids::uuid>& renderedImages() const;

  /**
   * @brief Replace the rendered image list.
   *
   * @param imageUids Image UIDs to store.
   * @param filterByDefaults Whether to apply default rendered-image preferences.
   */
  void setRenderedImages(const std::list<uuids::uuid>& imageUids, bool filterByDefaults);

  /** @brief Is the image selected for metric/comparison rendering? */
  bool isImageUsedForMetric(const uuids::uuid& imageUid) const;

  /**
   * @brief Add or remove a metric image.
   *
   * @param imageUid Image UID to update.
   * @param orderedImageUids Image UIDs in application order.
   * @param visible Whether the image should be part of metric rendering.
   */
  void setImageUsedForMetric(const uuids::uuid& imageUid, uuid_range_t orderedImageUids, bool visible);

  /** @brief Images selected for metric/comparison rendering. */
  const std::list<uuids::uuid>& metricImages() const;

  /** @brief Replace the metric image list. */
  void setMetricImages(const std::list<uuids::uuid>& imageUids);

  /**
   * @brief Return the image list used by the given render mode.
   *
   * @param renderMode Render mode that chooses rendered, metric, or empty selections.
   * @return Selected image UIDs used by the render mode.
   */
  const std::list<uuids::uuid>& visibleImages(ViewRenderMode renderMode) const;

  /** @brief Set preferred default rendered image positions. */
  void setPreferredDefaultRenderedImages(std::set<std::size_t> imageIndices);

  /** @brief Preferred default rendered image positions. */
  const std::set<std::size_t>& preferredDefaultRenderedImages() const;

  /** @brief Set whether default rendering keeps all images. */
  void setDefaultRenderAllImages(bool renderAll);

  /** @brief Whether default rendering keeps all images. */
  bool defaultRenderAllImages() const;

  /**
   * @brief Drop missing images and reorder selections to match application image order.
   *
   * @param orderedImageUids Current image UIDs in application order.
   */
  void updateImageOrdering(uuid_range_t orderedImageUids);

private:
  std::list<uuids::uuid> m_renderedImageUids; //!< Rendered images, bottom layer first.
  std::list<uuids::uuid> m_metricImageUids;   //!< Images used by metric/comparison modes.

  std::set<std::size_t> m_preferredDefaultRenderedImages; //!< Default rendered image indices.
  bool m_defaultRenderAllImages = true;                   //!< Ignore preferred indices and render all images.
};

} // namespace viewer
