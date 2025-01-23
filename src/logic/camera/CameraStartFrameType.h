#ifndef CAMERA_START_FRAME_TYPE_H
#define CAMERA_START_FRAME_TYPE_H

/// Describes the types of alignment for the starting frame of a camera
enum class CameraStartFrameType
{
  /// Aligned to axial plane (Z axis) of crosshairs
  Crosshairs_Axial_LAI, // radiological/neurological convention
  Crosshairs_Axial_RAS, // surgical convention

  /// Aligned to coronal plane (Y axis) of crosshairs
  Crosshairs_Coronal_LSA, // radiological/neurological convention
  Crosshairs_Coronal_RSP, // surgical convention

  /// Aligned to sagittal plane (X axis) of crosshairs
  Crosshairs_Sagittal_PSL,
  Crosshairs_Sagittal_ASR,

  /// Aligned to X axis of slide stack
  SlideStack_FacingNegX,

  /// Aligned to Y axis of slide stack
  SlideStack_FacingNegY,

  /// Aligned to look down facing the -Z axis of slide stack (last slide downwards)
  SlideStack_FacingNegZ,

  /// Aligned to look down facing the +Z axis of slide stack (first slide upwards)
  SlideStack_FacingPosZ
};

#endif // CAMERA_START_FRAME_TYPE_H
