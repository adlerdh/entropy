#pragma once

#include "common/IntersectionTypes.h"

namespace intersection
{

/**
 * @brief Source used to place the slice plane in model space.
 */
enum class PositioningMethod
{
  OffsetFromCamera, //!< Transform a stored camera-space offset into model space
  FrameOrigin,      //!< Use the origin of the supplied frame transform
  UserDefined       //!< Use an explicitly supplied model-space point
};

/**
 * @brief Source used to orient the slice plane normal.
 */
enum class AlignmentMethod
{
  CameraZ,    //!< Use the camera-space Z axis transformed into model space
  FrameX,     //!< Use the frame-space X axis transformed into model space
  FrameY,     //!< Use the frame-space Y axis transformed into model space
  FrameZ,     //!< Use the frame-space Z axis transformed into model space
  UserDefined //!< Use an explicitly supplied normal
};

} // namespace intersection
