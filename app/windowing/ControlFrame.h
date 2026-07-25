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

#include <cstddef>
#include <list>
#include <set>

class AppData;

/**
 * @brief Shared view and layout controls: viewport, image selection, render mode, and UI flags
 */
class ControlFrame
{
public:
  /**
   * @brief Construct a frame with viewport, view type, render mode, and UI-control state
   * @param winClipViewport Frame bounds in enclosing-window clip coordinates
   * @param viewType Initial view type
   * @param renderMode Initial render mode
   * @param ipMode Initial intensity projection mode
   * @param uiControls UI controls shown by the frame
   * @throw Propagates exceptions from viewport or selection construction
   */
  ControlFrame(
    glm::vec4 winClipViewport,
    ViewType viewType,
    ViewRenderMode renderMode,
    IntensityProjectionMode ipMode,
    UiControls uiControls);

  /**
   * @brief Copy a control frame
   * @param other Frame to copy
   * @throw Propagates exceptions from member copies
   */
  ControlFrame(const ControlFrame& other) = default;
  /**
   * @brief Copy-assign a control frame
   * @param other Frame to copy
   * @return Reference to this frame
   * @throw Propagates exceptions from member assignment
   */
  ControlFrame& operator=(const ControlFrame& other) = default;
  /**
   * @brief Move a control frame
   * @param other Frame to move from
   */
  ControlFrame(ControlFrame&&) noexcept = default;
  /**
   * @brief Move-assign a control frame
   * @param other Frame to move from
   * @return Reference to this frame
   */
  ControlFrame& operator=(ControlFrame&&) noexcept = default;
  /**
   * @brief Destroy the control frame
   */
  virtual ~ControlFrame() = default;

  /**
   * @brief Set viewport bounds in enclosing-window clip coordinates
   * @param winClipViewport Frame bounds in enclosing-window clip coordinates
   */
  void setWindowClipViewport(glm::vec4 winClipViewport);

  /**
   * @brief Get viewport bounds in enclosing-window clip coordinates
   * @return Frame bounds in enclosing-window clip coordinates
   */
  const glm::vec4& windowClipViewport() const;

  /**
   * @brief Get transform from this frame's clip coordinates to enclosing-window clip coordinates
   * @return Transform from view clip space to window clip space
   */
  const glm::mat4& windowClip_T_viewClip() const;

  /**
   * @brief Get transform from enclosing-window clip coordinates to this frame's clip coordinates
   * @return Transform from window clip space to view clip space
   */
  const glm::mat4& viewClip_T_windowClip() const;

  /**
   * @brief Get the frame view type
   * @return Current view type
   */
  ViewType viewType() const;
  /**
   * @brief Set the frame view type
   * @param viewType New view type
   */
  virtual void setViewType(const ViewType& viewType);

  /**
   * @brief Get the frame render mode
   * @return Current render mode
   */
  ViewRenderMode renderMode() const;
  /**
   * @brief Set the frame render mode
   * @param shaderType New render mode
   */
  virtual void setRenderMode(const ViewRenderMode& shaderType);

  /**
   * @brief Get the frame intensity projection mode
   * @return Current intensity projection mode
   */
  IntensityProjectionMode intensityProjectionMode() const;
  /**
   * @brief Set the frame intensity projection mode
   * @param ipMode New intensity projection mode
   */
  virtual void setIntensityProjectionMode(const IntensityProjectionMode& ipMode);

  /**
   * @brief Check whether an image at an application image index is selected for 2D rendering
   * @param appData Application data used to resolve the image index
   * @param index Image index in application order
   * @return True when the image is rendered by this frame
   */
  bool isImageRendered(const AppData& appData, std::size_t index);

  /**
   * @brief Check whether an image UID is selected for 2D rendering
   * @param imageUid Image UID to query
   * @return True when the image is rendered by this frame
   */
  bool isImageRendered(const uuids::uuid& imageUid);

  /**
   * @brief Set 2D rendering visibility for an image at an application image index
   * @param appData Application data used to resolve and order image UIDs
   * @param index Image index in application order
   * @param visible True to render the image
   */
  virtual void setImageRendered(const AppData& appData, std::size_t index, bool visible);

  /**
   * @brief Set 2D rendering visibility for an image UID
   * @param appData Application data used to preserve valid image ordering
   * @param imageUid Image UID to update
   * @param visible True to render the image
   */
  virtual void setImageRendered(const AppData& appData, const uuids::uuid& imageUid, bool visible);

  /**
   * @brief Get images selected for 2D rendering
   * @return Ordered rendered image UIDs
   */
  const std::list<uuids::uuid>& renderedImages() const;

  /**
   * @brief Replace the images selected for 2D rendering
   * @param imageUids Ordered image UIDs to render
   * @param filterByDefaults True to filter through the frame's default image selection policy
   * @throw Propagates exceptions from selection storage
   */
  virtual void setRenderedImages(const std::list<uuids::uuid>& imageUids, bool filterByDefaults);

  /**
   * @brief Check whether an image at an application image index is selected for 3D volume rendering
   * @param appData Application data used to resolve the image index
   * @param index Image index in application order
   * @return True when the image is volume rendered by this frame
   */
  bool isImageVolumeRendered(const AppData& appData, std::size_t index);

  /**
   * @brief Check whether an image UID is selected for 3D volume rendering
   * @param imageUid Image UID to query
   * @return True when the image is volume rendered by this frame
   */
  bool isImageVolumeRendered(const uuids::uuid& imageUid);

  /**
   * @brief Set 3D volume-rendering visibility for an image at an application image index
   * @param appData Application data used to resolve and order image UIDs
   * @param index Image index in application order
   * @param visible True to volume render the image
   */
  virtual void setImageVolumeRendered(const AppData& appData, std::size_t index, bool visible);

  /**
   * @brief Set 3D volume-rendering visibility for an image UID
   * @param appData Application data used to preserve valid image ordering
   * @param imageUid Image UID to update
   * @param visible True to volume render the image
   */
  virtual void setImageVolumeRendered(const AppData& appData, const uuids::uuid& imageUid, bool visible);

  /**
   * @brief Get images selected for 3D volume rendering
   * @return Ordered volume-rendered image UIDs
   */
  const std::list<uuids::uuid>& volumeRenderedImages() const;

  /**
   * @brief Replace the images selected for 3D volume rendering
   * @param imageUids Ordered image UIDs to volume render
   * @throw Propagates exceptions from selection storage
   */
  virtual void setVolumeRenderedImages(const std::list<uuids::uuid>& imageUids);

  /**
   * @brief Check whether an image at an application image index is selected for metric rendering
   * @param appData Application data used to resolve the image index
   * @param index Image index in application order
   * @return True when the image is used by this frame's metric
   */
  bool isImageUsedForMetric(const AppData& appData, std::size_t index);

  /**
   * @brief Check whether an image UID is selected for metric rendering
   * @param imageUid Image UID to query
   * @return True when the image is used by this frame's metric
   */
  bool isImageUsedForMetric(const uuids::uuid& imageUid);

  /**
   * @brief Set metric visibility for an image at an application image index
   * @param appData Application data used to resolve and order image UIDs
   * @param index Image index in application order
   * @param visible True to use the image in the metric
   */
  virtual void setImageUsedForMetric(const AppData& appData, std::size_t index, bool visible);

  /**
   * @brief Get images selected for metric rendering
   * @return Ordered metric image UIDs
   */
  const std::list<uuids::uuid>& metricImages() const;

  /**
   * @brief Replace the images selected for metric rendering
   * @param imageUids Ordered image UIDs used by the metric
   * @throw Propagates exceptions from selection storage
   */
  virtual void setMetricImages(const std::list<uuids::uuid>& imageUids);

  /**
   * @brief Get images visible through either render or metric selection
   * @return Ordered visible image UIDs for the current render mode
   */
  const std::list<uuids::uuid>& visibleImages() const;

  /**
   * @brief Set image indices preferred by the frame's default image selection policy
   * @param imageIndices Image indices preferred when resolving defaults
   * @throw Propagates exceptions from set allocation
   */
  void setPreferredDefaultRenderedImages(std::set<std::size_t> imageIndices);

  /**
   * @brief Get image indices preferred by the frame's default image selection policy
   * @return Set of preferred image indices
   */
  const std::set<std::size_t>& preferredDefaultRenderedImages() const;

  /**
   * @brief Set whether default selection renders all available images
   * @param renderAll True to render all images by default
   */
  void setDefaultRenderAllImages(bool renderAll);

  /**
   * @brief Check whether default selection renders all available images
   * @return True when all images are rendered by default
   */
  bool defaultRenderAllImages() const;

  /**
   * @brief Reorder rendered, volume, and metric image selections after the image order changes
   * @param orderedImageUids New application image UID order
   * @throw Propagates exceptions from selection storage
   */
  virtual void updateImageOrdering(const uuid_range_t& orderedImageUids);

  /**
   * @brief Get UI controls enabled for the frame
   * @return UI control flags
   */
  const UiControls& uiControls() const;

protected:
  /** @brief Viewport and clip-space transforms */
  viewer::FrameViewport m_viewport;
  /** @brief Rendered, volume-rendered, metric, and visible image selections */
  viewer::FrameImageSelection m_imageSelection;

  /** @brief View type */
  ViewType m_viewType;
  /** @brief Render mode */
  ViewRenderMode m_renderMode;
  /** @brief Intensity projection mode */
  IntensityProjectionMode m_intensityProjectionMode;
  /** @brief UI controls shown in the frame */
  UiControls m_uiControls;
};
