#pragma once

#include "common/Types.h"
#include "common/UuidRange.h"
#include "common/Viewport.h"

#include "logic/app/CrosshairsState.h"

#include "layout/LayoutSpec.h"
#include "viewer/LayoutTypes.h"
#include "viewer/ViewTypes.h"
#include "windowing/ControlFrame.h"
#include "windowing/Layout.h"
#include "windowing/View.h"

#include <glm/fwd.hpp>
#include <uuid.h>

#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class WindowData
{
  using uuid = uuids::uuid;

public:
  /**
   * @brief Construct window state and default layouts
   * @param crosshairs Crosshairs state referenced by all created views
   * @throw Propagates exceptions from layout construction
   */
  explicit WindowData(const CrosshairsState& crosshairs);
  /**
   * @brief Destroy window state and owned layouts
   */
  ~WindowData() = default;

  /**
   * @brief Resolve and set default rendered images for every layout
   * @param appData Application data containing image order and selection policy
   * @throw Propagates exceptions from layout image-selection updates
   */
  void setDefaultRenderedImagesForAllLayouts(const AppData& appData);
  /**
   * @brief Resolve and set default rendered images for one layout
   * @param layout Layout to update
   * @param appData Application data containing image order and selection policy
   * @throw Propagates exceptions from layout image-selection updates
   */
  void setDefaultRenderedImagesForLayout(Layout& layout, const AppData& appData) const;

  /**
   * @brief Reorder rendered and metric image selections after image order changes
   * @param orderedImageUids New application image UID order
   * @throw Propagates exceptions from layout selection storage
   */
  void updateImageOrdering(const uuid_range_t& orderedImageUids);

  /**
   * @brief Remove image-specific layouts and references to one image
   * @param imageUid Image UID to remove from layouts
   * @param orderedImageUids Remaining image UIDs in application order
   * @throw Propagates exceptions from layout storage or selection updates
   */
  void removeImageFromLayouts(const uuid& imageUid, const uuid_range_t& orderedImageUids);

  /**
   * @brief Add an image to views that render all images by default
   * @param appData Application data containing image order
   * @param imageUid Image UID to append
   * @throw Propagates exceptions from layout selection storage
   */
  void appendImageToDefaultRenderedImages(const AppData& appData, const uuid& imageUid);

  /**
   * @brief Recenter all views in World coordinates
   * @param worldCenter Target center in World space
   * @param worldFov Field of view size in World units
   * @param resetZoom True to reset camera zoom while recentering
   * @param resetObliqueOrientation True to reset oblique view orientation
   * @param excludedViews View UIDs that should not be recentered
   * @throw Propagates exceptions from camera transforms
   */
  void recenterAllViews(
    const glm::vec3& worldCenter,
    const glm::vec3& worldFov,
    bool resetZoom,
    bool resetObliqueOrientation,
    const std::set<uuid>& excludedViews = {});

  /**
   * @brief Recenter one current-layout view by UID
   * @param viewUid UID of the view to recenter
   * @param worldCenter Target center in World space
   * @param worldFov Field of view size in World units
   * @param resetZoom True to reset camera zoom while recentering
   * @param resetObliqueOrientation True to reset oblique view orientation
   * @throw Propagates exceptions from camera transforms
   */
  void recenterView(
    const uuid& viewUid,
    const glm::vec3& worldCenter,
    const glm::vec3& worldFov,
    bool resetZoom,
    bool resetObliqueOrientation);

  /**
   * @brief Recenter one view instance
   * @param view View to recenter
   * @param worldCenter Target center in World space
   * @param worldFov Field of view size in World units
   * @param resetZoom True to reset camera zoom while recentering
   * @param resetObliqueOrientation True to reset oblique view orientation
   * @throw Propagates exceptions from camera transforms
   */
  void recenterView(
    View& view,
    const glm::vec3& worldCenter,
    const glm::vec3& worldFov,
    bool resetZoom,
    bool resetObliqueOrientation);

  /**
   * @brief Get UIDs of views in the current layout
   * @return Current-layout view UIDs in display order
   * @throw Propagates exceptions from vector allocation
   */
  uuid_range_t currentViewUids() const;

  /**
   * @brief Find the current-layout view under a window-space cursor position
   * @param windowPos Cursor position in window pixel coordinates
   * @return View UID under the cursor, or `std::nullopt`
   */
  std::optional<uuid> currentViewUidAtCursor(const glm::vec2& windowPos) const;

  /**
   * @brief Find a const view by UID in the current layout
   * @param viewUid View UID to find
   * @return View pointer, or nullptr when not found
   */
  const View* getCurrentView(const uuid& viewUid) const;
  /**
   * @brief Find a mutable view by UID in the current layout
   * @param viewUid View UID to find
   * @return View pointer, or nullptr when not found
   */
  View* getCurrentView(const uuid& viewUid);

  /**
   * @brief Find a const view by UID across all layouts
   * @param viewUid View UID to find
   * @return View pointer, or nullptr when not found
   */
  const View* getView(const uuid& viewUid) const;
  /**
   * @brief Find a mutable view by UID across all layouts
   * @param viewUid View UID to find
   * @return View pointer, or nullptr when not found
   */
  View* getView(const uuid& viewUid);

  /**
   * @brief Get the active view UID
   * @return Active view UID, or `std::nullopt` when no view is active
   */
  std::optional<uuid> activeViewUid() const;

  /**
   * @brief Set the active view UID
   * @param viewUid Active view UID, or `std::nullopt` to clear active view state
   */
  void setActiveViewUid(const std::optional<uuid>& viewUid);

  /**
   * @brief Get the number of layouts
   * @return Layout count
   */
  std::size_t numLayouts() const;

  /**
   * @brief Get all layouts
   * @return Layouts in UI order
   */
  const std::vector<Layout>& layouts() const;
  /**
   * @brief Get a layout display name by index
   * @param index Layout index
   * @return User-facing display name
   * @throw Propagates exceptions from bounds checking or string allocation
   */
  std::string layoutDisplayName(std::size_t index) const;

  /**
   * @brief Get the current layout index
   * @return Current layout index
   */
  std::size_t currentLayoutIndex() const;

  /**
   * @brief Select the current layout by index
   * @param index Layout index to select
   */
  void setCurrentLayoutIndex(std::size_t index);
  /**
   * @brief Cycle the current layout index by a signed step
   * @param step Signed step applied to the current layout index
   */
  void cycleCurrentLayout(int step);
  /**
   * @brief Move a layout while keeping the same selected layout active
   * @param fromIndex Source layout index
   * @param toIndex Destination layout index
   * @throw Propagates exceptions from vector operations
   */
  void moveLayout(std::size_t fromIndex, std::size_t toIndex);

  /**
   * @brief Get a layout by index
   * @param index Layout index
   * @return Layout pointer, or nullptr when `index` is out of range
   */
  const Layout* layout(std::size_t index) const;

  /**
   * @brief Get the current layout
   * @return Const current layout
   * @throw Propagates exceptions if no current layout exists
   */
  const Layout& currentLayout() const;
  /**
   * @brief Get the mutable current layout
   * @return Mutable current layout
   * @throw Propagates exceptions if no current layout exists
   */
  Layout& currentLayout();

  /**
   * @brief Add a grid layout
   * @param viewType View type used for all grid cells
   * @param width Number of grid columns
   * @param height Number of grid rows
   * @param offsetViews True to offset slices across grid cells
   * @param isLightbox True when the layout is a lightbox for one image
   * @param imageIndexForLightbox Preferred image index for a lightbox
   * @param imageUidForLightbox Image UID for a lightbox
   * @param absoluteOffsetStep Optional absolute slice offset step
   * @throw Propagates exceptions from layout or view construction
   */
  void addGridLayout(
    const ViewType& viewType,
    std::size_t width,
    std::size_t height,
    bool offsetViews,
    bool isLightbox,
    std::size_t imageIndexForLightbox,
    const uuid& imageUidForLightbox,
    std::optional<float> absoluteOffsetStep = std::nullopt);

  /**
   * @brief Add a lightbox layout for one image
   * @param viewType View type used for lightbox cells
   * @param numSlices Number of lightbox slices
   * @param imageIndex Preferred image index
   * @param imageUid Image UID shown in the lightbox
   * @throw Propagates exceptions from layout or view construction
   */
  void addLightboxLayoutForImage(
    const ViewType& viewType,
    std::size_t numSlices,
    std::size_t imageIndex,
    const uuid& imageUid);

  /**
   * @brief Add axial/coronal/sagittal views grouped by image
   * @param numImages Number of image rows to create
   * @throw Propagates exceptions from layout or view construction
   */
  void addAxCorSagLayout(std::size_t numImages);

  /**
   * @brief Change the current layout's view type
   * @param appData Application data used when a managed lightbox must be rebuilt
   * @param viewType New view type
   *
   * Managed lightboxes are rebuilt so their tile count and relative offsets match the new orientation
   *
   * @throw Propagates exceptions from lightbox rebuilding
   */
  void setCurrentLayoutViewType(const AppData& appData, const ViewType& viewType);

  /**
   * @brief Rebuild layouts that depend on the loaded image set
   * @param appData Application image and selection state
   * @param dicomNativeViewTypesByImage Native DICOM slice orientation by image UID
   * @throw Propagates exceptions from layout rebuilding
   */
  void reconcileImageDependentLayouts(
    const AppData& appData,
    const std::unordered_map<uuid, ViewType>& dicomNativeViewTypesByImage = {});

  /**
   * @brief Serialize current layouts as project layout snapshots
   * @param orderedImageUids Image UIDs in application order
   * @return Layout snapshots
   * @throw Propagates exceptions from snapshot allocation
   */
  std::vector<layout::LayoutSpec> createProjectLayoutSnapshots(const uuid_range_t& orderedImageUids) const;

  /**
   * @brief Create default project layout snapshots for the current image set
   * @param appData Application image and selection state
   * @param dicomNativeViewTypesByImage Native DICOM slice orientation by image UID
   * @return Default layout snapshots
   * @throw Propagates exceptions from layout construction or snapshot allocation
   */
  std::vector<layout::LayoutSpec> createDefaultProjectLayoutSnapshots(
    const AppData& appData,
    const std::unordered_map<uuid, ViewType>& dicomNativeViewTypesByImage = {}) const;

  /**
   * @brief Choose the default layout index for the current image set
   * @param appData Application image and selection state
   * @param dicomNativeViewTypesByImage Native DICOM slice orientation by image UID
   * @return Default layout index
   */
  std::size_t defaultProjectLayoutIndex(
    const AppData& appData,
    const std::unordered_map<uuid, ViewType>& dicomNativeViewTypesByImage = {}) const;

  /**
   * @brief Replace current layouts from serialized project layout snapshots
   * @param layouts Layout snapshots to apply
   * @param orderedImageUids Image UIDs in application order
   * @param currentLayoutIndex Optional layout index to select after replacement
   * @return True when snapshots were applied
   * @throw Propagates exceptions from layout reconstruction
   */
  bool applyProjectLayoutSnapshots(
    const std::vector<layout::LayoutSpec>& layouts,
    const uuid_range_t& orderedImageUids,
    std::optional<std::size_t> currentLayoutIndex);

  /**
   * @brief Append project-specific layouts after regenerated default layouts
   * @param layouts Additional project layouts
   * @param orderedImageUids Image UIDs in application order
   * @param currentLayoutIndex Optional final layout index to select after appending
   * @return True when at least one layout was appended
   * @throw Propagates exceptions from layout reconstruction
   */
  bool appendProjectLayoutSnapshots(
    const std::vector<layout::LayoutSpec>& layouts,
    const uuid_range_t& orderedImageUids,
    std::optional<std::size_t> currentLayoutIndex);

  /**
   * @brief Replace one generated/default layout from a serialized snapshot
   * @param index Layout index to replace
   * @param layout Replacement layout snapshot
   * @param orderedImageUids Image UIDs in application order
   * @return True when the layout was replaced
   * @throw Propagates exceptions from layout reconstruction
   */
  bool replaceProjectLayoutSnapshot(
    std::size_t index,
    const layout::LayoutSpec& layout,
    const uuid_range_t& orderedImageUids);

  /**
   * @brief Remove one layout by index
   * @param index Layout index to remove
   * @throw Propagates exceptions from vector operations
   */
  void removeLayout(std::size_t index);
  /**
   * @brief Remove all layouts
   */
  void clearLayouts();
  /**
   * @brief Restore the built-in default layout set
   * @throw Propagates exceptions from layout construction
   */
  void resetDefaultLayouts();

  /**
   * @brief Select the default layout for the currently loaded image set
   * @param appData Application image and selection state
   */
  void setCurrentLayoutToDefaultForImages(const AppData& appData);

  /**
   * @brief Replace all layouts with one three-view overview layout
   * @throw Propagates exceptions from layout construction
   */
  void resetToThreeUpLayout();

  /**
   * @brief Get the window viewport
   * @return Window viewport in device-independent pixels
   */
  const Viewport& viewport() const;

  /**
   * @brief Set the window viewport in device-independent pixels
   * @param left Left coordinate
   * @param bottom Bottom coordinate
   * @param width Width
   * @param height Height
   */
  void setViewport(float left, float bottom, float width, float height);

  /**
   * @brief Set the GLFW content scale ratio
   * @param scale Content scale in x and y
   */
  void setContentScaleRatios(const glm::vec2& scale);

  /**
   * @brief Get the GLFW content scale ratio
   * @return Content scale in x and y
   */
  const glm::vec2& getContentScaleRatios() const;

  /**
   * @brief Get the primary content scale ratio
   * @return X content scale ratio
   */
  float getContentScaleRatio() const;

  /**
   * @brief Cache the window position in screen coordinates
   * @param posX Window x position
   * @param posY Window y position
   */
  void setWindowPos(int posX, int posY);

  /**
   * @brief Get cached window position in screen coordinates
   * @return Window position
   */
  const glm::ivec2& getWindowPos() const;

  /**
   * @brief Set the logical window size
   * @param width Window width in logical coordinates
   * @param height Window height in logical coordinates
   */
  void setWindowSize(int width, int height);

  /**
   * @brief Get the logical window size
   * @return Window size in logical coordinates
   */
  const glm::ivec2& getWindowSize() const;

  /**
   * @brief Set the framebuffer size in pixels
   * @param width Framebuffer width in pixels
   * @param height Framebuffer height in pixels
   */
  void setFramebufferSize(int width, int height);

  /**
   * @brief Get the framebuffer size in pixels
   * @return Framebuffer size in pixels
   */
  const glm::ivec2& getFramebufferSize() const;

  /**
   * @brief Compute framebuffer-to-window scale ratio
   * @return Ratio of framebuffer pixels to logical window units
   */
  glm::vec2 computeFramebufferToWindowRatio() const;

  /**
   * @brief Set the view orientation convention used by all views
   * @param convention New view orientation convention
   */
  void setViewOrientationConvention(const ViewConvention& convention);

  /**
   * @brief Get the view orientation convention
   * @return Current view orientation convention
   */
  ViewConvention getViewOrientationConvention() const;

  /**
   * @brief Get the view alignment mode
   * @return Current view alignment mode
   */
  ViewAlignmentMode viewAlignmentMode() const;

  /**
   * @brief Set the view alignment mode
   * @param mode New view alignment mode
   */
  void setViewAlignmentMode(ViewAlignmentMode mode);

  /**
   * @brief Get view UIDs in a camera synchronization group
   * @param mode Camera property synchronized by the group
   * @param syncGroupUid Synchronization group UID
   * @return View UIDs in the group, or an empty range when not found
   * @throw Propagates exceptions from vector allocation
   */
  uuid_range_t cameraSyncGroupViewUids(CameraSyncMode mode, const uuid& syncGroupUid) const;

  /**
   * @brief Apply one view's image selection to all views in the current layout
   * @param referenceViewUid UID of the view whose image selection should be copied
   * @throw Propagates exceptions from selection storage
   */
  void applyImageSelectionToAllCurrentViews(const uuid& referenceViewUid);

  /**
   * @brief Apply one view's render and projection modes to the current layout
   * @param referenceViewUid UID of the view whose modes should be copied
   */
  void applyViewRenderModeAndProjectionToAllCurrentViews(const uuid& referenceViewUid);

  /**
   * @brief Find current-layout views with a matching or opposite normal
   * @param worldNormal World-space normal to compare with view front directions
   * @return Matching view UIDs
   * @throw Propagates exceptions from vector allocation or camera transform access
   */
  std::vector<uuid> findCurrentViewsWithNormal(const glm::vec3& worldNormal) const;

  /**
   * @brief Find the largest view in the current layout
   * @return UID of the largest current-layout view
   * @throw Propagates exceptions if the current layout has no views
   */
  uuid findLargestCurrentView() const;

private:
  /**
   * @brief Create the default view layouts
   * @throw Propagates exceptions from layout construction
   */
  void setupViews();

  /**
   * @brief Recompute camera aspect ratios for all current-layout views
   */
  void recomputeCameraAspectRatios();

  /**
   * @brief Recompute view aspect ratios and frame corners
   */
  void updateAllViews();

  /** @brief Crosshairs state referenced by all views */
  const CrosshairsState& m_crosshairs;

  /** @brief Window viewport encompassing all views */
  Viewport m_viewport;

  /** @brief Window position in screen space with origin at the lower-left screen corner */
  glm::ivec2 m_windowPos;

  /** @brief Logical window size that should not be passed to `glViewport` */
  glm::ivec2 m_windowSize;

  /** @brief Window framebuffer size in pixels passed to `glViewport` */
  glm::ivec2 m_framebufferSize;

  /** @brief GLFW content scale ratio in x and y */
  glm::vec2 m_contentScaleRatio;

  /** @brief All view layouts in UI order */
  std::vector<Layout> m_layouts;
  /** @brief Index of the layout currently on display */
  std::size_t m_currentLayout;

  /** @brief UID of the view in which the user is currently interacting with the mouse */
  std::optional<uuid> m_activeViewUid = std::nullopt;

  /** @brief Default view orientation convention used for all views */
  ViewConvention m_viewConvention = ViewConvention::Radiological;

  /** @brief View alignment mode */
  ViewAlignmentMode m_viewAlignment = ViewAlignmentMode::Crosshairs;
};
