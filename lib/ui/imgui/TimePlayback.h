#pragma once

/// @file Time-series frame selection, synchronized playback, and timeline controls.

#include <uuid.h>
#include <cstdint>
#include <optional>

class AppData;
class Image;

namespace ui::imgui_detail
{
/**
 * @brief Select a frame and refresh textures, updating other time series when synchronization is enabled.
 * @param imageUid UID of the supplied image.
 * @param timePoint Zero-based frame index, clamped separately to each image's range.
 */
void setTimePointWithSynchronization(AppData& appData, const uuids::uuid& imageUid, Image& image, uint32_t timePoint);

/// @brief Stop playback on every time-series image except the given image.
void stopOtherTimeSeriesPlayback(AppData& appData, const uuids::uuid& playingImageUid);

/// @brief Return the active time-series image, otherwise the first available one, or std::nullopt.
std::optional<uuids::uuid> globalTimeControlImageUid(AppData& appData);

/// @brief Advance playback, update animation state, and draw the global time controls when enabled.
void renderGlobalTimeControl(AppData& appData);
} // namespace ui::imgui_detail
