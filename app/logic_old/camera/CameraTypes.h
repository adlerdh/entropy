#ifndef CAMERA_TYPES_H
#define CAMERA_TYPES_H

namespace camera
{

/**
 * @brief Types of cameras. Each can have a different starting orientation, projection,
 * and alignment rules. The cameras themselves are defined in \c InteractionManager.
 */
enum class CameraType
{
  /// Aligned with reference image subject axial plane
  Axial,

  /// Aligned with reference image subject coronal plane
  Coronal,

  /// Aligned with reference image subject sagittal plane
  Sagittal,

  /// Camera for the "big" 3D view
  Big3D,

  /// Camera for the "main" 3D view on the 4-up
  Main3D,

  /// Aligned with X axis of stack (perpendicular to stacking direction)
  StackSide1,

  /// Aligned with Y axis of stack (perpendicular to stacking direction)
  StackSide2,

  /// Aligned with active slide, looking top to bottom, down the stacking direction
  /// (from last to first slide)
  SlideActive_TopToBottomSlide,

  /// Aligned with active slide, looking bottom to top, up the stacking direction
  /// (from first to last slide)
  SlideActive_BottomToTopSlide,

  /// For 3D views of the slide stack
  Stack3D,
};

/**
 * @brief Types of camera projections
 */
enum class ProjectionType
{
  /// Orthographic projection is used for the "2D" views, since there's no compelling reason
  /// to do otherwise. Orthographic projections made zooming and rotation about arbitrary points
  /// behave nicer.
  Orthographic,

  /// Perspective projection is used for the main and big 3D views. It lets us fly inside the scene!
  Perspective
};

} // namespace camera

#endif // CAMERA_TYPES_H
