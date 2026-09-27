#pragma once

#include <functional>
#include <string>

class AppData;

/**
 * @brief Render the registration setup window.
 * @param appData Application data containing loaded images and GUI visibility state.
 */
void renderRegistrationSetupWindow(AppData& appData);

/**
 * @brief Render the registration jobs window.
 * @param appData Application data containing GUI visibility state.
 * @param importJobOutputs Callback invoked when the user imports one completed job's outputs.
 */
void renderRegistrationJobsWindow(
  AppData& appData,
  const std::function<void(const std::string& jobId)>& importJobOutputs);

/**
 * @brief Render the compact registration progress window when jobs are active.
 * @param appData Application data containing registration jobs.
 */
void renderRegistrationProgressWindow(AppData& appData);
