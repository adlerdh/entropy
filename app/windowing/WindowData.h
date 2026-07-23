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
  WindowData(const CrosshairsState& crosshairs);
  ~WindowData() = default;

  void setDefaultRenderedImagesForAllLayouts(const AppData& appData);
  void setDefaultRenderedImagesForLayout(Layout& layout, const AppData& appData) const;

  /** @brief Reorder rendered and metric image selections after image order changes. */
  void updateImageOrdering(const uuid_range_t& orderedImageUids);

  /** @brief Remove image-specific layouts and references to one image. */
  void removeImageFromLayouts(const uuid& imageUid, const uuid_range_t& orderedImageUids);

  /** @brief Add an image to views that render all images by default. */
  void appendImageToDefaultRenderedImages(const AppData& appData, const uuid& imageUid);

  /** @brief Recenter all views in world coordinates. */
  void recenterAllViews(
    const glm::vec3& worldCenter,
    const glm::vec3& worldFov,
    bool resetZoom,
    bool resetObliqueOrientation,
    const std::set<uuid>& excludedViews = {});

  /** @brief Recenter one view without changing its FOV. */
  void recenterView(
    const uuid& viewUid,
    const glm::vec3& worldCenter,
    const glm::vec3& worldFov,
    bool resetZoom,
    bool resetObliqueOrientation);

  void recenterView(
    View& view,
    const glm::vec3& worldCenter,
    const glm::vec3& worldFov,
    bool resetZoom,
    bool resetObliqueOrientation);

  uuid_range_t currentViewUids() const;

  std::optional<uuid> currentViewUidAtCursor(const glm::vec2& windowPos) const;

  const View* getCurrentView(const uuid&) const;
  View* getCurrentView(const uuid&);

  const View* getView(const uuid& viewUid) const;
  View* getView(const uuid& viewUid);

  std::optional<uuid> activeViewUid() const;

  void setActiveViewUid(const std::optional<uuid>& viewUid);

  std::size_t numLayouts() const;

  const std::vector<Layout>& layouts() const;
  std::string layoutDisplayName(std::size_t index) const;

  std::size_t currentLayoutIndex() const;

  void setCurrentLayoutIndex(std::size_t index);
  void cycleCurrentLayout(int step);
  /** @brief Move a layout while keeping the same selected layout active. */
  void moveLayout(std::size_t fromIndex, std::size_t toIndex);

  const Layout* layout(std::size_t index) const;

  const Layout& currentLayout() const;
  Layout& currentLayout();

  /** @brief Add a grid layout. */
  void addGridLayout(
    const ViewType& viewType,
    std::size_t width,
    std::size_t height,
    bool offsetViews,
    bool isLightbox,
    std::size_t imageIndexForLightbox,
    const uuid& imageUidForLightbox,
    std::optional<float> absoluteOffsetStep = std::nullopt);

  /** @brief Add a lightbox layout for one image. */
  void addLightboxLayoutForImage(
    const ViewType& viewType,
    std::size_t numSlices,
    std::size_t imageIndex,
    const uuid& imageUid);

  /** @brief Add axial/coronal/sagittal views grouped by image. */
  void addAxCorSagLayout(std::size_t numImages);

  /**
   * @brief Change the current layout's view type.
   * @param appData Application data used when a managed lightbox must be rebuilt.
   * @param viewType New view type.
   *
   * Managed lightboxes are rebuilt so their tile count and relative offsets match the new orientation.
   */
  void setCurrentLayoutViewType(const AppData& appData, const ViewType& viewType);

  /**
   * @brief Rebuild layouts that depend on the loaded image set.
   * @param appData Application image and selection state.
   * @param dicomNativeViewTypesByImage Native DICOM slice orientation by image UID.
   */
  void reconcileImageDependentLayouts(
    const AppData& appData,
    const std::unordered_map<uuid, ViewType>& dicomNativeViewTypesByImage = {});

  std::vector<layout::LayoutSpec> createProjectLayoutSnapshots(const uuid_range_t& orderedImageUids) const;
  std::vector<layout::LayoutSpec> createDefaultProjectLayoutSnapshots(
    const AppData& appData,
    const std::unordered_map<uuid, ViewType>& dicomNativeViewTypesByImage = {}) const;
  std::size_t defaultProjectLayoutIndex(
    const AppData& appData,
    const std::unordered_map<uuid, ViewType>& dicomNativeViewTypesByImage = {}) const;
  bool applyProjectLayoutSnapshots(
    const std::vector<layout::LayoutSpec>& layouts,
    const uuid_range_t& orderedImageUids,
    std::optional<std::size_t> currentLayoutIndex);

  /**
   * @brief Append project-specific layouts after regenerated default layouts.
   * @param layouts Additional project layouts.
   * @param orderedImageUids Image UIDs in application order.
   * @param currentLayoutIndex Optional final layout index to select after appending.
   * @return True when at least one layout was appended.
   */
  bool appendProjectLayoutSnapshots(
    const std::vector<layout::LayoutSpec>& layouts,
    const uuid_range_t& orderedImageUids,
    std::optional<std::size_t> currentLayoutIndex);

  /**
   * @brief Replace one generated/default layout from a serialized snapshot.
   * @param index Layout index to replace.
   * @param layout Replacement layout snapshot.
   * @param orderedImageUids Image UIDs in application order.
   * @return True when the layout was replaced.
   */
  bool replaceProjectLayoutSnapshot(
    std::size_t index,
    const layout::LayoutSpec& layout,
    const uuid_range_t& orderedImageUids);

  void removeLayout(std::size_t index);
  void clearLayouts();
  void resetDefaultLayouts();

  /** @brief Select the default layout for the currently loaded image set. */
  void setCurrentLayoutToDefaultForImages(const AppData& appData);

  /** @brief Replace all layouts with one three-view overview layout. */
  void resetToThreeUpLayout();

  const Viewport& viewport() const;

  /** @brief Set the window viewport in device-independent pixels. */
  void setViewport(float left, float bottom, float width, float height);

  /** @brief Set/get the GLFW content scale ratio. */
  void setContentScaleRatios(const glm::vec2& scale);
  const glm::vec2& getContentScaleRatios() const;
  float getContentScaleRatio() const;

  /** @brief Cache the window position in screen coordinates. */
  void setWindowPos(int posX, int posY);
  const glm::ivec2& getWindowPos() const;

  /** @brief Set/get the logical window size. */
  void setWindowSize(int width, int height);
  const glm::ivec2& getWindowSize() const;

  /** @brief Set/get the framebuffer size in pixels. */
  void setFramebufferSize(int width, int height);
  const glm::ivec2& getFramebufferSize() const;

  glm::vec2 computeFramebufferToWindowRatio() const;

  void setViewOrientationConvention(const ViewConvention& convention);
  ViewConvention getViewOrientationConvention() const;

  ViewAlignmentMode viewAlignmentMode() const;
  void setViewAlignmentMode(ViewAlignmentMode mode);

  /** @brief View UIDs in a camera synchronization group. */
  uuid_range_t cameraSyncGroupViewUids(CameraSyncMode mode, const uuid& syncGroupUid) const;

  /** @brief Apply one view's image selection to all views in the current layout. */
  void applyImageSelectionToAllCurrentViews(const uuid& referenceViewUid);

  /** @brief Apply one view's render/projection modes to the current layout. */
  void applyViewRenderModeAndProjectionToAllCurrentViews(const uuid& referenceViewUid);

  /** @brief Find current-layout views with a matching or opposite normal. */
  std::vector<uuid> findCurrentViewsWithNormal(const glm::vec3& worldNormal) const;

  /** @brief Find the largest view in the current layout. */
  uuid findLargestCurrentView() const;

private:
  // Create the default view layouts
  void setupViews();

  // Recompute view aspect ratios
  void recomputeCameraAspectRatios();

  // Recompute view aspect ratios and corners
  void updateAllViews();

  const CrosshairsState& m_crosshairs;

  // Window viewport (encompassing all views)
  Viewport m_viewport;

  // Window position in screen space with (0, 0) at bottom left corner of the screen
  glm::ivec2 m_windowPos;

  // Window size, measured in "artificial" screen coordinates. This should not be passed to
  // glViewport.
  glm::ivec2 m_windowSize;

  // Window framebuffer size, measured in pixels. This is the size that should be passed to
  // glViewport.
  glm::ivec2 m_framebufferSize;

  glm::vec2 m_contentScaleRatio;

  /**
   * @todo Map from uuid to layout
   */
  std::vector<Layout> m_layouts; // All view layouts
  std::size_t m_currentLayout;   // Index of the layout currently on display

  // UID of the view in which the user is currently interacting with the mouse.
  // The mouse must be held down for the view to be active.
  std::optional<uuid> m_activeViewUid = std::nullopt;

  // Default view orientation convention used for all views
  ViewConvention m_viewConvention = ViewConvention::Radiological;

  ViewAlignmentMode m_viewAlignment = ViewAlignmentMode::Crosshairs; //!< View alignment mode
};
