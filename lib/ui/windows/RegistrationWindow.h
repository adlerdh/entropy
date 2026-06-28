#pragma once

class AppData;

/**
 * @brief Render the registration setup window.
 * @param appData Application data containing loaded images and GUI visibility state.
 */
void renderRegistrationSetupWindow(AppData& appData);

/**
 * @brief Render the registration jobs window.
 * @param appData Application data containing GUI visibility state.
 */
void renderRegistrationJobsWindow(AppData& appData);

/**
 * @brief Render the compact registration progress window when jobs are active.
 * @param appData Application data containing registration jobs.
 */
void renderRegistrationProgressWindow(AppData& appData);
