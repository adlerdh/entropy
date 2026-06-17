#pragma once

#include "common/Types.h"
#include "logic/app/CrosshairsState.h"
#include "logic/camera/Camera.h"
#include "rendering/utility/math/SliceIntersectorTypes.h"
#include "windowing/ControlFrame.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <uuid.h>

#include <optional>

class Image;

/** @brief Rendered viewport with a camera, slice state, and sync-group membership. */
class View : public ControlFrame
{
  using uuid = uuids::uuid;

public:
  /**
   * @brief Construct a view.
   * @param winClipViewport Viewport bounds in enclosing-window clip coordinates.
   * @param offsetSetting Slice offset behavior.
   * @param viewType View orientation/type.
   * @param renderMode Initial render mode.
   * @param ipMode Initial intensity projection mode.
   * @param uiControls UI controls enabled for this view.
   * @param viewConvention View orientation convention.
   * @param crosshairs Crosshairs state referenced by the view.
   * @param viewAlignment View alignment mode.
   * @param cameraRotationSyncGroupUid Rotation sync group, when any.
   * @param translationSyncGroup Translation sync group, when any.
   * @param zoomSyncGroup Zoom sync group, when any.
   */
  View(
    glm::vec4 winClipViewport,
    ViewOffsetSetting offsetSetting,
    ViewType viewType,
    ViewRenderMode renderMode,
    IntensityProjectionMode ipMode,
    UiControls uiControls,
    const ViewConvention& viewConvention,
    const CrosshairsState& crosshairs,
    const ViewAlignmentMode& viewAlignment,
    std::optional<uuid> cameraRotationSyncGroupUid,
    std::optional<uuid> translationSyncGroup,
    std::optional<uuid> zoomSyncGroup);

  const uuid& uid() const;

  void setViewType(const ViewType& newViewType) override;
  void setRenderMode(const ViewRenderMode& renderMode) override;

  const Camera& camera() const;
  Camera& camera();

  /**
   * @brief Update the view's camera based on the crosshairs World-space position.
   * @param appData Application data used for image geometry.
   * @param worldCrosshairs Crosshairs position in world coordinates.
   * @return Crosshairs position on the current slice.
   */
  glm::vec3 updateImageSlice(const AppData& appData, const glm::vec3& worldCrosshairs);

  /**
   * @brief Compute this view's intersection polygon with one image.
   * @param image Image to intersect.
   * @param crosshairs Crosshairs frame used to position the slice.
   * @return Intersection vertices, or std::nullopt when the slice misses the image.
   */
  std::optional<intersection::IntersectionVerticesVec4> computeImageSliceIntersection(
    const Image* image,
    const CoordinateFrame& crosshairs) const;

  /** @brief Clip-space depth of the current image plane. */
  float clipPlaneDepth() const;

  /** @brief Slice offset behavior for this view. */
  const ViewOffsetSetting& offsetSetting() const;

  /** @brief Camera rotation sync group, when any. */
  std::optional<uuid> cameraRotationSyncGroupUid() const;

  /** @brief Camera translation sync group, when any. */
  std::optional<uuid> cameraTranslationSyncGroupUid() const;

  /** @brief Camera zoom sync group, when any. */
  std::optional<uuid> cameraZoomSyncGroupUid() const;

private:
  CoordinateFrame get_anatomy_T_start(const ViewType& viewType) const;

  bool updateImageSliceIntersection(const AppData& appData, const glm::vec3& worldCrosshairs);

  const uuid m_uid; //!< This view's uid

  ViewOffsetSetting m_offset; //!< Slice offset behavior.

  ProjectionType m_projectionType;
  Camera m_camera;

  const ViewConvention& m_viewConvention;
  const CrosshairsState& m_crosshairs;
  const ViewAlignmentMode& m_viewAlignment;

  std::optional<uuid> m_cameraRotationSyncGroupUid;    //!< Rotation sync group.
  std::optional<uuid> m_cameraTranslationSyncGroupUid; //!< Translation sync group.
  std::optional<uuid> m_cameraZoomSyncGroupUid;        //!< Zoom sync group.

  float m_clipPlaneDepth; //!< Current image-plane depth in clip coordinates.

  CoordinateFrame m_anatomy_T_start;
};
