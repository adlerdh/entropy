#pragma once

#include <cstddef>
#include <cstdint>

struct ImVec2;

/**
 * @brief helpMarker Helper to display a little (?) mark which shows a tooltip when hovered.
 * @param[in] tooltip
 * @todo Set sameLine to true and remove other SameLine() calls
 */
void helpMarker(const char* tooltip, bool sameLine = false);

ImVec2
constrainedWindowMaxSize(float viewportFraction = 0.8f, float fallbackWidth = 960.0f, float fallbackHeight = 720.0f);

void setNextWindowSizeConstraintsToMainViewport(
  float minWidth = 0.0f,
  float minHeight = 0.0f,
  float viewportFraction = 0.8f,
  float fallbackMaxWidth = 960.0f,
  float fallbackMaxHeight = 720.0f);

/**
 * @brief Apply the shared ImGui window class used by dockable side panels.
 *
 * @details The class keeps docked panel title tabs visible and removes the
 * extra dock-node close/menu controls that would duplicate the panel controls.
 */
void setNextDockablePanelWindowClass();

bool mySliderS32(const char* label, int32_t* value, int32_t min = 0, int32_t max = 100, const char* format = "%d");

bool mySliderS64(const char* label, int64_t* value, int64_t min = 0, int64_t max = 100, const char* format = "%d");

bool mySliderF32(const char* label, float* value, float min = 0.0, float max = 1.0, const char* format = "%.2f");

bool mySliderF64(const char* label, double* value, double min = 0.0, double max = 1.0, const char* format = "%.2f");

int myImFormatString(char* buf, size_t buf_size, const char* fmt, ...);
