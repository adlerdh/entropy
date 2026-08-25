#pragma once

#include "viewer/CameraSyncGroups.h"
#include "viewer/LayoutTypes.h"
#include "windowing/ControlFrame.h"
#include "windowing/View.h"

#include <uuid.h>

#include <cstddef>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @brief Set of views rendered together in the application window
 */
class Layout : public ControlFrame
{
public:
  using uuid = uuids::uuid;

  /**
   * @brief Construct an empty layout
   * @param isLightbox True when layout-level controls are shown and propagated as a lightbox
   * @throw Propagates exceptions from base-frame construction or UID generation
   */
  explicit Layout(bool isLightbox);
  /**
   * @brief Copying layouts is disabled because they own views by unique pointer
   * @param other Layout that would be copied
   * @throw Not applicable
   */
  Layout(const Layout& other) = delete;
  /**
   * @brief Copy-assignment is disabled because layouts own views by unique pointer
   * @param other Layout that would be copied
   * @return Not applicable
   * @throw Not applicable
   */
  Layout& operator=(const Layout& other) = delete;
  /**
   * @brief Move a layout and its owned views
   * @param other Layout to move from
   */
  Layout(Layout&& other) noexcept = default;
  /**
   * @brief Move-assign a layout and its owned views
   * @param other Layout to move from
   * @return Reference to this layout
   */
  Layout& operator=(Layout&& other) noexcept = default;
  /**
   * @brief Destroy the layout and owned views
   */
  ~Layout() = default;

  /**
   * @brief Replace this layout's contents without changing its UID
   * @param other Replacement layout data
   */
  void replaceContentsPreservingUid(Layout&& other) noexcept;

  /**
   * @brief Set child-view 2D rendering visibility by image index
   * @param appData Application data used to resolve and order image UIDs
   * @param index Image index in application order
   * @param visible True to render the image
   */
  void setImageRendered(const AppData& appData, std::size_t index, bool visible) override;

  /**
   * @brief Replace child-view images selected for 2D rendering
   * @param imageUids Ordered image UIDs to render
   * @param filterByDefaults True to filter through default image selection policy
   * @throw Propagates exceptions from selection storage
   */
  void setRenderedImages(const std::list<uuid>& imageUids, bool filterByDefaults) override;

  /**
   * @brief Set child-view 3D volume-rendering visibility by image index
   * @param appData Application data used to resolve and order image UIDs
   * @param index Image index in application order
   * @param visible True to volume render the image
   */
  void setImageVolumeRendered(const AppData& appData, std::size_t index, bool visible) override;

  /**
   * @brief Replace child-view images selected for 3D volume rendering
   * @param imageUids Ordered image UIDs to volume render
   * @throw Propagates exceptions from selection storage
   */
  void setVolumeRenderedImages(const std::list<uuid>& imageUids) override;

  /**
   * @brief Replace child-view images selected for metric rendering
   * @param imageUids Ordered image UIDs used by the metric
   * @throw Propagates exceptions from selection storage
   */
  void setMetricImages(const std::list<uuid>& imageUids) override;

  /**
   * @brief Set child-view metric visibility by image index
   * @param appData Application data used to resolve and order image UIDs
   * @param index Image index in application order
   * @param visible True to use the image in the metric
   */
  void setImageUsedForMetric(const AppData& appData, std::size_t index, bool visible) override;

  /**
   * @brief Reorder child-view image selections after application image order changes
   * @param orderedImageUids New application image UID order
   * @throw Propagates exceptions from selection storage
   */
  void updateImageOrdering(const uuid_range_t& orderedImageUids) override;

  /**
   * @brief Set the layout view type and propagate it to all child views
   * @param viewType New view type
   */
  void setViewType(const ViewType& viewType) override;

  /**
   * @brief Set the layout render mode and propagate it to all child views
   * @param renderMode New render mode
   */
  void setRenderMode(const ViewRenderMode& renderMode) override;

  /**
   * @brief Set the layout intensity projection mode and propagate it to all child views
   * @param ipMode New intensity projection mode
   */
  void setIntensityProjectionMode(const IntensityProjectionMode& ipMode) override;

  /**
   * @brief Get the stable layout UID
   * @return Layout UID
   */
  const uuid& uid() const;

  /**
   * @brief Check whether this layout is a lightbox layout
   * @return True for lightbox layouts
   */
  bool isLightbox() const;

  /**
   * @brief Get the layout kind
   * @return Layout kind
   */
  LayoutKind kind() const;

  /**
   * @brief Set the layout kind
   * @param kind New layout kind
   */
  void setKind(LayoutKind kind);

  /**
   * @brief Get the user-facing layout display name
   * @return Layout display name
   */
  const std::string& displayName() const;

  /**
   * @brief Set the user-facing layout display name
   * @param displayName New display name. Empty names are replaced with "Custom"
   * @throw Propagates exceptions from string allocation
   */
  void setDisplayName(std::string displayName);

  /**
   * @brief Add a view and append its UID to the stable display order
   * @param view View instance whose UID should be inserted into the layout
   * @return true when the view was inserted; false when its UID already exists
   * @throw Throws `std::invalid_argument` when `view` is null
   */
  bool addView(std::unique_ptr<View> view);

  /**
   * @brief Views keyed by UID. Use `orderedViewUids()` or `orderedViews()` when display order matters
   * @return Map of view UID to owned live view
   */
  const std::unordered_map<uuid, std::unique_ptr<View>>& views() const;

  /**
   * @brief Stable display order used for rendering, UI hit testing, and serialization
   * @return View UIDs in display order
   */
  const std::vector<uuid>& orderedViewUids() const;

  /**
   * @brief Live views in display order
   * @return Mutable view pointers in display order
   * @throw Propagates exceptions from vector allocation
   */
  std::vector<View*> orderedViews();

  /**
   * @brief Live views in display order
   * @return Const view pointers in display order
   * @throw Propagates exceptions from vector allocation
   */
  std::vector<const View*> orderedViews() const;

  /**
   * @brief Generate a new UID and add an empty camera synchronization group for `mode`
   * @param mode Camera property synchronized by the new group
   * @return UID of the newly created synchronization group
   * @throw Propagates exceptions from UID generation or group storage
   */
  uuid addCameraSyncGroup(CameraSyncMode mode);

  /**
   * @brief Return a camera synchronization group, or nullptr when `groupUid` is not found for `mode`
   * @param mode Camera property synchronized by the group
   * @param groupUid Synchronization group UID to find
   * @return Pointer to the group view-UID list, or nullptr when no matching group exists
   */
  const std::list<uuid>* getCameraSyncGroup(CameraSyncMode mode, const uuid& groupUid) const;

  /**
   * @brief Return a mutable camera synchronization group, or nullptr when `groupUid` is not found
   * @param mode Camera property synchronized by the group
   * @param groupUid Synchronization group UID to find
   * @return Mutable pointer to the group view-UID list, or nullptr when no matching group exists
   */
  std::list<uuid>* getCameraSyncGroup(CameraSyncMode mode, const uuid& groupUid);

  /**
   * @brief Find the synchronization group for `mode` that contains `viewUid`
   * @param mode Camera property synchronized by the group
   * @param viewUid View UID whose group membership should be located
   * @return Synchronization group UID when the view is a member; otherwise std::nullopt
   */
  std::optional<uuid> cameraSyncGroupUidContainingView(CameraSyncMode mode, const uuid& viewUid) const;

  /**
   * @brief Add `viewUid` to an existing group, create no membership when `groupUid` is empty
   * @param mode Camera property synchronized by the group
   * @param groupUid Optional synchronization group UID to join
   * @param viewUid View UID to add to the group
   * @throw Propagates exceptions from group storage
   */
  void addViewToCameraSyncGroup(CameraSyncMode mode, const std::optional<uuid>& groupUid, const uuid& viewUid);

private:
  /**
   * @brief Propagate layout-level controls and image selections to all child views
   * @throw Propagates exceptions from child view setters
   */
  void updateAllViewsInLayout();

  /** @brief Stable layout UID */
  uuid m_uid;

  /** @brief True when layout-level controls affect all child views */
  bool m_isLightbox;
  /** @brief Layout kind used for display and serialization */
  LayoutKind m_kind = LayoutKind::Custom;
  /** @brief User-facing name for custom non-lightbox layouts */
  std::string m_displayName = "Custom";

  /** @brief Most recently selected layout-level render mode for 2D views */
  ViewRenderMode m_last2dRenderMode = ViewRenderMode::Image;
  /** @brief Most recently selected layout-level render mode for 3D views */
  ViewRenderMode m_last3dRenderMode = ViewRenderMode::SegmentationAndIsosurfaces;

  /** @brief Owned views keyed by UID */
  std::unordered_map<uuid, std::unique_ptr<View>> m_views;
  /** @brief View UIDs in stable display order */
  std::vector<uuid> m_orderedViewUids;

  /** @brief Camera synchronization groups organized by synchronized property */
  viewer::CameraSyncGroups m_cameraSyncGroups;
};
