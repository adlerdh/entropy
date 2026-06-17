#pragma once

#include "common/UuidRange.h"

#include "ui/UiControls.h"
#include "viewer/FrameImageSelection.h"
#include "viewer/FrameViewport.h"
#include "viewer/ViewModes.h"
#include "viewer/ViewTypes.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <list>
#include <set>

class AppData;

/** @brief Shared view/layout controls: viewport, image selection, render mode, and UI flags. */
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

  /** @brief Set viewport bounds in enclosing-window clip coordinates. */
  void setWindowClipViewport(glm::vec4 winClipViewport);

  /** @brief Viewport bounds in enclosing-window clip coordinates. */
  const glm::vec4& windowClipViewport() const;

  /** @brief Transform from this frame's clip coordinates to enclosing-window clip coordinates. */
  const glm::mat4& windowClip_T_viewClip() const;

  /** @brief Transform from enclosing-window clip coordinates to this frame's clip coordinates. */
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

  /** @brief Images visible through either render or metric selection. */
  const std::list<uuids::uuid>& visibleImages() const;

  void setPreferredDefaultRenderedImages(std::set<std::size_t> imageIndices);
  const std::set<std::size_t>& preferredDefaultRenderedImages() const;

  void setDefaultRenderAllImages(bool renderAll);
  bool defaultRenderAllImages() const;

  /** @brief Reorder rendered and metric image selections after the image order changes. */
  virtual void updateImageOrdering(uuid_range_t orderedImageUids);

  const UiControls& uiControls() const;

protected:
  viewer::FrameViewport m_viewport;             //!< Viewport and clip-space transforms.
  viewer::FrameImageSelection m_imageSelection; //!< Rendered and metric image selections.

  ViewType m_viewType;                               //!< View type
  ViewRenderMode m_renderMode;                       //!< Render mode
  IntensityProjectionMode m_intensityProjectionMode; //!< Intensity projection mode
  UiControls m_uiControls;                           //!< UI controls shown in the frame
};
