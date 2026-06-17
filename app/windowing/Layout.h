#pragma once

#include "viewer_types/LayoutTypes.h"
#include "windowing/ControlFrame.h"
#include "windowing/View.h"

#include <uuid.h>

#include <list>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

/** @brief Set of views rendered together in the window. */
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
   * @brief Add a view and append its UID to the stable display order.
   * @param view View instance whose UID should be inserted into the layout.
   * @return true when the view was inserted; false when its UID already exists.
   */
  bool addView(std::unique_ptr<View> view);

  /**
   * @brief Views keyed by UID. Use `orderedViewUids()` or `orderedViews()` when display order matters.
   * @return Map of view UID to owned live view.
   */
  const std::unordered_map<uuid, std::unique_ptr<View>>& views() const;

  /**
   * @brief Stable display order used for rendering, UI hit testing, and serialization.
   * @return View UIDs in display order.
   */
  const std::vector<uuid>& orderedViewUids() const;

  /**
   * @brief Live views in display order.
   * @return Mutable view pointers in display order.
   */
  std::vector<View*> orderedViews();

  /**
   * @brief Live views in display order.
   * @return Const view pointers in display order.
   */
  std::vector<const View*> orderedViews() const;

  /**
   * @brief Generate a new UID and add an empty camera synchronization group for `mode`.
   * @param mode Camera property synchronized by the new group.
   * @return UID of the newly created synchronization group.
   */
  uuid addCameraSyncGroup(CameraSyncMode mode);

  /**
   * @brief Return a camera synchronization group, or nullptr when `groupUid` is not found for `mode`.
   * @param mode Camera property synchronized by the group.
   * @param groupUid Synchronization group UID to find.
   * @return Pointer to the group view-UID list, or nullptr when no matching group exists.
   */
  const std::list<uuid>* getCameraSyncGroup(CameraSyncMode mode, const uuid& groupUid) const;

  /**
   * @brief Return a mutable camera synchronization group, or nullptr when `groupUid` is not found.
   * @param mode Camera property synchronized by the group.
   * @param groupUid Synchronization group UID to find.
   * @return Mutable pointer to the group view-UID list, or nullptr when no matching group exists.
   */
  std::list<uuid>* getCameraSyncGroup(CameraSyncMode mode, const uuid& groupUid);

  /**
   * @brief Find the synchronization group for `mode` that contains `viewUid`.
   * @param mode Camera property synchronized by the group.
   * @param viewUid View UID whose group membership should be located.
   * @return Synchronization group UID when the view is a member; otherwise std::nullopt.
   */
  std::optional<uuid> cameraSyncGroupUidContainingView(CameraSyncMode mode, const uuid& viewUid) const;

  /**
   * @brief Add `viewUid` to an existing group, create no membership when `groupUid` is empty.
   * @param mode Camera property synchronized by the group.
   * @param groupUid Optional synchronization group UID to join.
   * @param viewUid View UID to add to the group.
   */
  void addViewToCameraSyncGroup(CameraSyncMode mode, const std::optional<uuid>& groupUid, const uuid& viewUid);

private:
  void updateAllViewsInLayout();

  uuid m_uid; //!< Layout uid

  bool m_isLightbox;                      //!< Layout-level controls affect all views.
  LayoutKind m_kind = LayoutKind::Custom; //!< Layout kind used for display and serialization.

  std::unordered_map<uuid, std::unique_ptr<View>> m_views; //!< Views keyed by UID.
  std::vector<uuid> m_orderedViewUids;                     //!< View UIDs in display order.

  using SyncGroupToViews = std::unordered_map<uuid, std::list<uuid>>;
  std::unordered_map<CameraSyncMode, SyncGroupToViews> m_cameraSyncGroups; //!< Camera sync groups by mode.
};
