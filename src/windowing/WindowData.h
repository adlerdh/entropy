#ifndef WINDOW_DATA_H
#define WINDOW_DATA_H

#include "common/Types.h"
#include "common/UuidRange.h"
#include "common/Viewport.h"

#include "windowing/Layout.h"
#include "windowing/View.h"

#include <uuid.h>

#include <optional>
#include <vector>

/**
 * @brief Data for the window
 */
class WindowData
{
public:
  WindowData();
  ~WindowData() = default;

  void setDefaultRenderedImagesForAllLayouts(uuid_range_t orderedImageUids);
  void setDefaultRenderedImagesForLayout(Layout& layout, uuid_range_t orderedImageUids);

  /// Call this when image order changes in order to update rendered and metric images
  void updateImageOrdering(uuid_range_t orderedImageUids);

  /// Initialize all view to the given center and FOV, defined in World space
  void recenterAllViews(
    const glm::vec3& worldCenter,
    const glm::vec3& worldFov,
    bool resetZoom,
    bool resetObliqueOrientation
  );

  /// Recenter a view to the given center position, without changing its FOV.
  /// (FOV is passed in only to adjust camera pullback distance.)
  void recenterView(
    const uuids::uuid& viewUid,
    const glm::vec3& worldCenter,
    const glm::vec3& worldFov,
    bool resetZoom,
    bool resetObliqueOrientation
  );

  void recenterView(
    View& view,
    const glm::vec3& worldCenter,
    const glm::vec3& worldFov,
    bool resetZoom,
    bool resetObliqueOrientation
  );

  /// Get all current view UIDs
  uuid_range_t currentViewUids() const;

  /// In which view is the window position?
  std::optional<uuids::uuid> currentViewUidAtCursor(const glm::vec2& windowPos) const;

  /// Get const/non-const pointer to a current view
  const View* getCurrentView(const uuids::uuid&) const;
  View* getCurrentView(const uuids::uuid&);

  /// Get const/non-const pointer to a view
  const View* getView(const uuids::uuid& viewUid) const;
  View* getView(const uuids::uuid& viewUid);

  /// Get UID of active view
  std::optional<uuids::uuid> activeViewUid() const;

  /// Set UID of the active view
  void setActiveViewUid(const std::optional<uuids::uuid>& viewUid);

  /// Number of layouts
  std::size_t numLayouts() const;

  /// Current layout index
  std::size_t currentLayoutIndex() const;

  void setCurrentLayoutIndex(std::size_t index);
  void cycleCurrentLayout(int step);

  const Layout* layout(std::size_t index) const;

  const Layout& currentLayout() const;
  Layout& currentLayout();

  /// Add a grid layout
  void addGridLayout(
    const ViewType& viewType,
    std::size_t width,
    std::size_t height,
    bool offsetViews,
    bool isLightbox,
    std::size_t imageIndexForLightbox,
    const uuids::uuid& imageUidForLightbox
  );

  /// Add a lightbox grid layout with enough views to hold a given number of slices
  void addLightboxLayoutForImage(
    const ViewType& viewType,
    std::size_t numSlices,
    std::size_t imageIndex,
    const uuids::uuid& imageUid
  );

  /// Add a layout with one row per image and columns for axial, coronal, and sagittal views
  void addAxCorSagLayout(std::size_t numImages);

  /// Remove a layout
  void removeLayout(std::size_t index);

  /// Get the window viewport
  const Viewport& viewport() const;

  /// Set the window viewport (in device-independent pixel units)
  void setViewport(float left, float bottom, float width, float height);

  /**
     * @brief Set/get the window content scale ratio
     * 
     * @see GLFW
     * The content scale is the ratio between the current DPI and the platform's default DPI.
     * This is especially important for text and any UI elements. If the pixel dimensions of your UI
     * scaled by this look appropriate on your machine then it should appear at a reasonable size on
     * other machines regardless of their DPI and scaling settings. This relies on the system DPI and
     * scaling settings being somewhat correct.
     * 
     * On systems where each monitors can have its own content scale, the window content scale will depend
     * on which monitor the system considers the window to be on.
     */
  void setContentScaleRatios(const glm::vec2& ratio);
  const glm::vec2& getContentScaleRatios() const;
  float getContentScaleRatio() const;

  /// Set/get the window position in screen space. This does not move the window.
  void setWindowPos(int posX, int posY);
  const glm::ivec2& getWindowPos() const;

  /// Set/get the whole window size, which is specified in artificial units that do not
  /// necessarily correspond to real screen pixels, as is the case when DPI scaling is activated.
  void setWindowSize(int width, int height);
  const glm::ivec2& getWindowSize() const;

  /// Set/get the framebuffer size in pixel units.
  void setFramebufferSize(int width, int height);
  const glm::ivec2& getFramebufferSize() const;

  /// Compute the ratio of framebuffer pixels to window size
  glm::vec2 computeFramebufferToWindowRatio() const;

  /// Set/get the view orientation convention
  void setViewOrientationConvention(const ViewConvention& convention);
  ViewConvention getViewOrientationConvention() const;

  /// Get view UIDs in a camera rotation synchronization group
  uuid_range_t cameraRotationGroupViewUids(const uuids::uuid& syncGroupUid) const;

  /// Get view UIDs in a camera translation synchronization group
  uuid_range_t cameraTranslationGroupViewUids(const uuids::uuid& syncGroupUid) const;

  /// Get view UIDs in a camera zoom synchronization group
  uuid_range_t cameraZoomGroupViewUids(const uuids::uuid& syncGroupUid) const;

  /// Apply a given view's image selection to all views of the current layout
  void applyImageSelectionToAllCurrentViews(const uuids::uuid& referenceViewUid);

  /// Apply a given view's render and intensity projection modes to all views of the current layout
  void applyViewRenderModeAndProjectionToAllCurrentViews(const uuids::uuid& referenceViewUid);

  /// Find all views in the current layout with normal vector either parallel to or anti-parallel to
  /// the given normal direction
  std::vector<uuids::uuid> findCurrentViewsWithNormal(const glm::vec3& worldNormal) const;

  /// Find the largest view (in terms of area) in the current layout.
  uuids::uuid findLargestCurrentView() const;

private:
  // Create the default view layouts
  void setupViews();

  // Recompute view aspect ratios
  void recomputeCameraAspectRatios();

  // Recompute view aspect ratios and corners
  void updateAllViews();

  // Window viewport (encompassing all views)
  Viewport m_viewport;

  // Window position in screen space with (0, 0) at bottom left corner of the screen
  glm::ivec2 m_windowPos;

  // Window size, measured in "artificial" screen coordinates. This should not be passed to glViewport.
  glm::ivec2 m_windowSize;

  // Window framebuffer size, measured in pixels. This is the size that should be passed to glViewport.
  glm::ivec2 m_framebufferSize;

  glm::vec2 m_contentScaleRatio;

  std::vector<Layout> m_layouts; // All view layouts
  std::size_t m_currentLayout;   // Index of the layout currently on display

  // UID of the view in which the user is currently interacting with the mouse.
  // The mouse must be held down for the view to be active.
  std::optional<uuids::uuid> m_activeViewUid;

  // Default view orientation convention used for all views
  ViewConvention m_viewConvention;
};

#endif // WINDOW_DATA_H
