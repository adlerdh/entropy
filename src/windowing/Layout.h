#pragma once

#include "windowing/ControlFrame.h"
#include "windowing/View.h"

#include <uuid.h>

#include <list>
#include <memory>
#include <unordered_map>

enum class CameraSynchronizationMode
{
  Rotation,
  Translation,
  Zoom
};

/// @brief Represents a set of views rendered together in the window at one time
class Layout : public ControlFrame
{
public:
  Layout(bool isLightbox);
  Layout(const Layout&) = delete;
  Layout& operator=(const Layout&) = delete;
  Layout(Layout&&) noexcept;
  Layout& operator=(Layout&& other) noexcept;
  ~Layout() = default;

  void setImageRendered(const AppData& appData, std::size_t index, bool visible) override;
  void setRenderedImages(const std::list<uuids::uuid>& imageUids, bool filterByDefaults) override;

  void setMetricImages(const std::list<uuids::uuid>& imageUids) override;
  void setImageUsedForMetric(const AppData& appData, std::size_t index, bool visible) override;

  void updateImageOrdering(uuid_range_t orderedImageUids) override;

  void setViewType(const ViewType& viewType) override;
  void setRenderMode(const ViewRenderMode& renderMode) override;
  void setIntensityProjectionMode(const IntensityProjectionMode& renderMode) override;

  const uuids::uuid& uid() const;
  bool isLightbox() const;

  void recenter();

  /**
   * @brief Add view
   * @return UID of the view in the layout
   */
  uuids::uuid addView(std::unique_ptr<View> view);
  const std::unordered_map<uuids::uuid, std::unique_ptr<View>>& views() const;

  // Generates a new UUID and adds an empty camera synchronization group.
  // Returns the UUID of the newly added group.
  uuids::uuid addCameraSyncGroup(CameraSynchronizationMode mode);

  // Returns a pointer to the camera synchronization group list with given UID, or nullptr if not found
  const std::list<uuids::uuid>* getCameraSyncGroup(CameraSynchronizationMode mode, const uuids::uuid& groupUid) const;
  std::list<uuids::uuid>* getCameraSyncGroup(CameraSynchronizationMode mode, const uuids::uuid& groupUid);

private:
  void updateAllViewsInLayout();

  uuids::uuid m_uid;

  /// If true, then this layout has UI controls that affect all of its views,
  /// rather than each view having its own UI controls
  bool m_isLightbox;

  /// Views of the layout, keyed by their UID
  std::unordered_map<uuids::uuid, std::unique_ptr<View>> m_views;

  /// For each synchronization mode type, a map of synchronization group UID to the list of view UIDs in the group
  std::unordered_map<CameraSynchronizationMode, std::unordered_map<uuids::uuid, std::list<uuids::uuid>>> m_cameraSyncGroups;
};
