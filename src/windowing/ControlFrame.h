#pragma once

#include "common/UuidRange.h"

#include "logic/camera/CameraTypes.h"
#include "ui/UiControls.h"
#include "windowing/ViewTypes.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <list>
#include <set>

class AppData;

class ControlFrame
{
public:
  ControlFrame(
    glm::vec4 winClipViewport,
    ViewType viewType,
    ViewRenderMode renderMode,
    IntensityProjectionMode ipMode,
    UiControls uiControls);

  virtual ~ControlFrame() = default;

  void setWindowClipViewport(glm::vec4 winClipViewport);
  const glm::vec4& windowClipViewport() const;

  const glm::mat4& windowClip_T_viewClip() const;
  const glm::mat4& viewClip_T_windowClip() const;

  ViewType viewType() const;
  virtual void setViewType(const ViewType& viewType);

  ViewRenderMode renderMode() const;
  virtual void setRenderMode(const ViewRenderMode& renderMode);

  IntensityProjectionMode intensityProjectionMode() const;
  virtual void setIntensityProjectionMode(const IntensityProjectionMode& ipMode);

  bool isImageRendered(const AppData& appData, std::size_t index);
  bool isImageRendered(const uuids::uuid& imageUid);

  virtual void setImageRendered(const AppData& appData, std::size_t index, bool visible);
  virtual void setImageRendered(const AppData& appData, const uuids::uuid& imageUid, bool visible);

  const std::list<uuids::uuid>& renderedImages() const;
  virtual void setRenderedImages(const std::list<uuids::uuid>& imageUids, bool filterByDefaults);

  bool isImageUsedForMetric(const AppData& appData, std::size_t index);
  bool isImageUsedForMetric(const uuids::uuid& imageUid);

  virtual void setImageUsedForMetric(const AppData& appData, std::size_t index, bool visible);

  const std::list<uuids::uuid>& metricImages() const;
  virtual void setMetricImages(const std::list<uuids::uuid>& imageUids);

  /// This one accounts for both rendered and metric images.
  const std::list<uuids::uuid>& visibleImages() const;

  void setPreferredDefaultRenderedImages(std::set<std::size_t> imageIndices);
  const std::set<std::size_t>& preferredDefaultRenderedImages() const;

  void setDefaultRenderAllImages(bool renderAll);
  bool defaultRenderAllImages() const;

  /// Call this when image order changes in order to update rendered and metric images:
  virtual void updateImageOrdering(uuid_range_t orderedImageUids);

  const UiControls& uiControls() const;

protected:
  /// Viewport of the view defined in Clip space of the enclosing window,
  /// which spans from bottom left [-1, -1] to top right [1, 1].
  /// A full-window view has viewport (left = -1, bottom = -1, width = 2, height = 2)
  glm::vec4 m_winClipViewport;

  /// Transformation from view Clip space to Clip space of its enclosing window
  glm::mat4 m_windowClip_T_viewClip;

  /// Transformation from the Clip space of the view's enclosing window to Clip space of the view
  glm::mat4 m_viewClip_T_windowClip;

  /// Uids of images rendered in this frame. They are listed in the order in which they are
  /// rendered, with image 0 at the bottom.
  std::list<uuids::uuid> m_renderedImageUids;

  /// Uids of images used for metric calculation in this frame. The first image is the
  /// fixed image; the second image is the moving image. As of now, all metrics use two
  /// images, but we could potentially include metrics that use more than two images.
  std::list<uuids::uuid> m_metricImageUids;

  /// What images does this view prefer to render by default?
  std::set<std::size_t> m_preferredDefaultRenderedImages;

  /// Flag to render all images in this vew by default.
  /// When true, the set \c m_preferredDefaultRenderedImages is ignored and all images
  /// are rendered; when false, \c m_preferredDefaultRenderedImages is used.
  bool m_defaultRenderAllImages;

  ViewType m_viewType; //!< View type
  ViewRenderMode m_renderMode; //!< Render mode
  IntensityProjectionMode m_intensityProjectionMode; //!< Intensity projection mode
  UiControls m_uiControls; //!< UI controls shown in the frame
};
