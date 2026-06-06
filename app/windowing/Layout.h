#pragma once

#include "windowing/ControlFrame.h"
#include "windowing/View.h"

#include <uuid.h>

#include <list>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

enum class CameraSyncMode
{
  Rotation,
  Translation,
  Zoom
};

enum class LayoutKind
{
  Custom = 0,
  FourUp = 1,
  Tri = 2,
  SingleAxial = 3,
  MultiImageAxialGrid = 4,
  AxCorSagByImage = 5,
  AxialLightbox = 6,
  CoronalLightbox = 7,
  SagittalLightbox = 8,
  NumElements
};

/// @brief Represents a set of views rendered together in the window at one time
class Layout : public ControlFrame
{
public:
  using uuid = uuids::uuid;

  explicit Layout(bool isLightbox);
  Layout(const Layout&) = delete;
  Layout& operator=(const Layout&) = delete;
  Layout(Layout&&) noexcept;
  Layout& operator=(Layout&& other) noexcept;
  ~Layout() = default;

  void setImageRendered(const AppData& appData, std::size_t index, bool visible) override;
  void setRenderedImages(const std::list<uuid>& imageUids, bool filterByDefaults) override;

  void setMetricImages(const std::list<uuid>& imageUids) override;
  void setImageUsedForMetric(const AppData& appData, std::size_t index, bool visible) override;

  void updateImageOrdering(uuid_range_t orderedImageUids) override;

  void setViewType(const ViewType& viewType) override;
  void setRenderMode(const ViewRenderMode& renderMode) override;
  void setIntensityProjectionMode(const IntensityProjectionMode& renderMode) override;

  const uuid& uid() const;
  bool isLightbox() const;
  LayoutKind kind() const;
  void setKind(LayoutKind kind);

  /**
   * @brief Add view
   * @return UID of the view in the layout
   */
  bool addView(std::unique_ptr<View> view);
  const std::unordered_map<uuid, std::unique_ptr<View>>& views() const;
  const std::vector<uuid>& orderedViewUids() const;
  std::vector<View*> orderedViews();
  std::vector<const View*> orderedViews() const;

  // Generates a new UUID and adds an empty camera synchronization group.
  // Returns the UUID of the newly added group.
  uuid addCameraSyncGroup(CameraSyncMode mode);

  // Returns a pointer to the camera synchronization group list with given UID, or nullptr if not
  // found
  const std::list<uuid>* getCameraSyncGroup(CameraSyncMode mode, const uuid& groupUid) const;
  std::list<uuid>* getCameraSyncGroup(CameraSyncMode mode, const uuid& groupUid);
  std::optional<uuid> cameraSyncGroupUidContainingView(CameraSyncMode mode, const uuid& viewUid) const;
  void addViewToCameraSyncGroup(CameraSyncMode mode, const std::optional<uuid>& groupUid, const uuid& viewUid);

private:
  void updateAllViewsInLayout();

  uuid m_uid; //!< Layout uid

  /// If true, then this layout has UI controls that affect all of its views,
  /// rather than each view having its own UI controls
  bool m_isLightbox;
  LayoutKind m_kind = LayoutKind::Custom;

  /// Views of the layout, keyed by their UID
  std::unordered_map<uuid, std::unique_ptr<View>> m_views;

  /// Stable display/order list for rendering, UI, hit testing, and serialization.
  std::vector<uuid> m_orderedViewUids;

  /// For each synchronization mode type, a map of synchronization group UID to the list of view
  /// UIDs in the group
  using SyncGroupToViews = std::unordered_map<uuid, std::list<uuid>>;
  std::unordered_map<CameraSyncMode, SyncGroupToViews> m_cameraSyncGroups;
};
