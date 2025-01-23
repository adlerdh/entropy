#ifndef INTERACTION_MANAGER_H
#define INTERACTION_MANAGER_H

#include "common/AABB.h"
#include "common/UID.h"

#include "gui/layout/ViewType.h"
#include "gui/layout/ViewTypeRange.h"

#include "common/CoordinateFrame.h"
#include "common/CoordinateFrameLinkingType.h"
#include "common/PublicTypes.h"

#include "logic/CrosshairsType.h"
#include "logic/camera/CameraStartFrameType.h"
#include "logic/camera/CameraTypes.h"
#include "logic/interaction/InteractionHandlerType.h"
#include "logic/interaction/InteractionModes.h"

#include <glm/fwd.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

namespace camera
{
class Camera;
class Projection;
} // namespace camera

class IInteractionHandler;
class InteractionPack;

/// @todo Rename to ViewInteractionManager
class InteractionManager
{
public:
  /// Defines the direction in which the camera looks at the Active Slide view.
  enum class ActiveSlideViewDirection
  {
    TopToBottomSlide, //!< From top (+z) to bottom (-z) of the slide stack axis
    BottomToTopSlide  //!< From bottom (-z) to top (+z) of the slide stack axis
  };

  InteractionManager(
    GetterType<view_type_range_t> viewUidAndTypeRangeProvider,
    GetterType<CoordinateFrame> crosshairsFrameProvider,
    GetterType<CoordinateFrame> slideStackCrosshairsFrameProvider,
    GetterType<CoordinateFrame> slideStackFrameProvider
  );

  InteractionManager(const InteractionManager&) = default;
  InteractionManager& operator=(const InteractionManager&) = default;

  InteractionManager(InteractionManager&&) = default;
  InteractionManager& operator=(InteractionManager&&) = default;

  ~InteractionManager();

  /// Set functional that provides the reference space AABB
  void setRefSpaceAABBoxProvider(GetterType<AABB<float> >);

  /// Set functional that provides the slide stack AABB
  void setSlideStackAABBoxProvider(GetterType<std::optional<AABB<float> > >);

  /// Get non-owning pointer to the interaction pack for a given view
  InteractionPack* getInteractionPack(const UID& viewUid);

  /// Get non-owning pointer to the camera for a given view
  camera::Camera* getCamera(const UID& viewUid);

  /// Set the main interaction handler type
  void setInteractionModeType(const InteractionModeType&);

  /// Get non-owning pointer to the active interaction handler for a given view
  IInteractionHandler* getActiveInteractionHandler(const UID& viewUid);

  /// Set the view direction of the active slide view
  void setActiveSlideViewDirection(const ActiveSlideViewDirection&);

  /// Get the view direction of the actie slide view
  ActiveSlideViewDirection getActiveSlideViewDirection() const;

  /// Set the cameras to view the bounding box of the reference space.
  void setupCamerasForAABBox(const AABB<float>& worldRefAABB, float refVoxelSize);

  void setCameraNearDistance(float distance);

  void alignCamerasToFrames();
  void resetCameras();

  camera::CameraType getCameraType(const gui::ViewType&) const;
  CrosshairsType getCrosshairsType(const gui::ViewType&) const;

  void applyExtraToCameras(const LinkedFrameType& linkedFrameType, const glm::mat4& extra);

private:
  void initialize();

  void addInteractionPackForView(const UID& viewUid, const gui::ViewType& viewType);

  void setCameraInteractionMode(const CameraInteractionMode&);
  void setCrosshairsInteractionMode(const CrosshairsInteractionMode&);
  void setRefImageInteractionMode(const RefImageInteractionMode&);
  void setStackInteractionMode(const StackInteractionMode&);
  void setSlideInteractionMode(const SlideInteractionMode&);
  void setWindowLevelInteractionMode(const WindowLevelInteractionMode&);

  /**
     * @brief For a given view type, this computes the coordinate frame mapping the view camera
     * Start Frame space to World space
     *
     * @param[in] viewType View type
     *
     * @return Coordinate frame mapping camera Start to World space
     */
  CoordinateFrame computeStartFrame(const gui::ViewType& viewType) const;

  GetterType<view_type_range_t> m_viewTypeRangeProvider;

  /// Function returning the AABB of the reference space in World space coordinates.
  GetterType<AABB<float> > m_refSpaceAABBoxProvider;

  /// Function providing the slide stack AABB in World space
  GetterType<std::optional<AABB<float> > > m_slideStackAABBoxProvider;

  /// Function providing the crosshairs for Reference Image views
  GetterType<CoordinateFrame> m_crosshairsFrameProvider;

  /// Function providing the crosshairs for Slide Stack views
  GetterType<CoordinateFrame> m_slideStackCrosshairsFrameProvider;

  /// Function providing the slide stack frame
  GetterType<CoordinateFrame> m_slideStackFrameProvider;

  /// Hash map of interaction packs, keyed by view UID.
  /// There is exactly one interaction pack per view.
  std::unordered_map<UID, std::unique_ptr<InteractionPack> > m_interactionPacks;

  /// Current map from view type to camera type
  std::unordered_map<gui::ViewType, camera::CameraType> m_viewTypeToCameraTypeMap;

  /// Map from view type to default crosshairs type
  static const std::unordered_map<gui::ViewType, CrosshairsType>
    smk_viewTypeToDefaultCrosshairsTypeMap;
  static CrosshairsType getDefaultCrosshairsType(const gui::ViewType&);

  /// Map from view type to default camera type
  static const std::unordered_map<gui::ViewType, camera::CameraType>
    smk_viewTypeToDefaultCameraTypeMap;
  static camera::CameraType getDefaultCameraType(const gui::ViewType&);

  /// Map from camera type to projection type
  static const std::unordered_map<camera::CameraType, camera::ProjectionType>
    smk_cameraTypeToProjectionTypeMap;
  static camera::ProjectionType getProjectionType(const camera::CameraType&);

  /// Map from camera type to default start frame linking type. This defines the
  /// Coordinate frame to which a camera start frame is linked.
  static const std::unordered_map<camera::CameraType, LinkedFrameType>
    smk_cameraStartFrameTypeToDefaultLinkedStartFrameTypeMap;
  static LinkedFrameType getDefaultLinkedStartFrameType(const camera::CameraType&);

  /// Map from camera type to default camera start frame type
  static const std::unordered_map<camera::CameraType, CameraStartFrameType>
    smk_cameraTypeToDefaultStartFrameTypeMap;
  static CameraStartFrameType getDefaultCameraStartFrameType(const camera::CameraType&);

  /// Default map from start frame type to start frame
  /// Anatomical coordinate frame transformation applied atop the linked frame,
  /// This is used for defining the Axial, Coronal, Sagittal, etc. anatomical transformations.
  /// The frame's "world_O_frame" transformation really maps linkedFrame_O_anatomicalFrame.
  static const std::unordered_map<CameraStartFrameType, glm::quat>
    smk_cameraStartFrameTypeToDefaultAnatomicalRotationMap;
  static glm::quat getDefaultAnatomicalRotation(const CameraStartFrameType&);

  /// @note Taken together, the concatenation of the two transformations
  /// (world_O_linkedFrame * linkedFrame_O_anatomicalFrame),
  /// i.e. linkedFrame.world_O_frame * anatomicalFrame.world_O_frame
  /// defines the "start anatomical frame" for the view camera.
};

#endif // INTERACTION_MANAGER_H
