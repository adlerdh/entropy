#ifndef LANDMARK_GROUP_RECORD_H
#define LANDMARK_GROUP_RECORD_H

#include "logic_old/RenderableRecord.h"
#include "logic_old/annotation/LandmarkGroupCpuRecord.h"
#include "rendering_old/records/EmptyGpuRecord.h"

/// @note Reference image landmarks are 3D points defined in Subject space of a reference image.
/// @note Slide landmarks are 3D points defined in normalized [0, 1]^3 coordinates of a slide.
using LandmarkGroupRecord = RenderableRecord<LandmarkGroupCpuRecord, EmptyGpuRecord>;

#endif // LANDMARK_GROUP_RECORD_H
