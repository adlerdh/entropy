#ifndef UI_HELPERS_H
#define UI_HELPERS_H

#include <cstddef>
#include <cstdint>

/**
 * @brief helpMarker Helper to display a little (?) mark which shows a tooltip when hovered.
 * @param[in] tooltip
 * @todo Set sameLine to true and remove other SameLine() calls
 */
void helpMarker(const char* tooltip, bool sameLine = false);

bool mySliderS32(
  const char* label, int32_t* value, int32_t min = 0, int32_t max = 100, const char* format = "%d"
);

bool mySliderS64(
  const char* label, int64_t* value, int64_t min = 0, int64_t max = 100, const char* format = "%d"
);

bool mySliderF32(
  const char* label, float* value, float min = 0.0, float max = 1.0, const char* format = "%.2f"
);

bool mySliderF64(
  const char* label, double* value, double min = 0.0, double max = 1.0, const char* format = "%.2f"
);

int myImFormatString(char* buf, size_t buf_size, const char* fmt, ...);

#endif // UI_HELPERS_H
