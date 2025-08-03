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

/**
 * @brief Represents a view in the window. Each view is a visual representation of a
 * scene from a single orientation. The view holds a camera and information about the
 * image plane being rendered in it.
 */
class View : public ControlFrame
{
  using uuid = uuids::uuid;;

public:
  /**
     * @brief Construct a view
     *
     * @param[in] winClipViewport Viewport (left, bottom, width, height) of the view,
     * defined in Clip space of its enclosing window's viewport
     * (e.g. (-1, -1, 2, 2) is a view that covers the full window viewport and
     * (0, 0, 1, 1) is a view that covers the top-right quadrant of the window viewport)
     *
     * @param[in] numOffsets Number of scroll offsets (relative to the reference image)
     * from the crosshairs at which to render this view's image planes
     *
     * @param[in] viewType Type of view
     * @param[in] shaderType Shader type of the view
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

  const Camera& camera() const;
  Camera& camera();

  /**
     * @brief Update the view's camera based on the crosshairs World-space position.
     * @param[in] appData
     * @param[in] worldCrosshairs
     * @return The crosshairs position on the slice
     */
  glm::vec3 updateImageSlice(const AppData& appData, const glm::vec3& worldCrosshairs);

  std::optional<intersection::IntersectionVerticesVec4>
  computeImageSliceIntersection(const Image* image, const CoordinateFrame& crosshairs) const;

  float clipPlaneDepth() const;

  const ViewOffsetSetting& offsetSetting() const;

  std::optional<uuid> cameraRotationSyncGroupUid() const;
  std::optional<uuid> cameraTranslationSyncGroupUid() const;
  std::optional<uuid> cameraZoomSyncGroupUid() const;

private:
  CoordinateFrame get_anatomy_T_start(const ViewType& viewType) const;

  bool updateImageSliceIntersection(const AppData& appData, const glm::vec3& worldCrosshairs);

  const uuid m_uid; //!< This view's uid

  /// View offset setting
  ViewOffsetSetting m_offset;

  ProjectionType m_projectionType;
  Camera m_camera;

  const ViewConvention& m_viewConvention;
  const CrosshairsState& m_crosshairs;
  const ViewAlignmentMode& m_viewAlignment;

  /// ID of the camera synchronization groups to which this view belongs
  std::optional<uuid> m_cameraRotationSyncGroupUid;
  std::optional<uuid> m_cameraTranslationSyncGroupUid;
  std::optional<uuid> m_cameraZoomSyncGroupUid;

  /// Depth (z component) of any point on the image plane to be rendered (defined in Clip space)
  float m_clipPlaneDepth;
};
