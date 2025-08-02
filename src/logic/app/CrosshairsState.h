#pragma once

#include "common/CoordinateFrame.h"

#include <uuid.h>
#include <optional>

/// @brief Holds crosshairs coordinate frames, defined in World space
struct CrosshairsState
{
  /// Saved crosshairs, prior to rotation starting. The single view in which crosshairs,
  /// are being rotated, will align to these crosshairs.
  CoordinateFrame worldCrosshairsOld;

  /// Current crosshairs. This accounts for the current rotation being done.
  CoordinateFrame worldCrosshairs;

  /// UID of view aligned with old crosshairs. This is the view in which the crosshairs are
  /// currently being rotated. Will be std::nullopt when no view aligns to the old crosshairs
  /// (i.e. crosshairs are not being rotated).
  std::optional<uuids::uuid> viewUidWithOldCrosshairs;
};
