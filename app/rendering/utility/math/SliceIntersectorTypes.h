#ifndef SLICE_INTERSECTOR_TYPES_H
#define SLICE_INTERSECTOR_TYPES_H

#include "common/IntersectionTypes.h"

namespace intersection
{

/**
 * @brief Describes the method used for positioning slices.
 */
enum class PositioningMethod
{
  OffsetFromCamera,
  FrameOrigin,
  UserDefined
};

/**
 * @brief Describes the method used for aligning slices.
 */
enum class AlignmentMethod
{
  CameraZ,
  FrameX,
  FrameY,
  FrameZ,
  UserDefined
};

} // namespace intersection

#endif // SLICE_INTERSECTOR_TYPES_H
