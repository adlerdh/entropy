#pragma once

#include <uuid.h>

#include <functional>

class AppData;

/**
 * @brief Render the image opacity mixer window.
 * @param appData Application data containing image order, image settings, render data, and window visibility.
 * @param updateImageUniforms Callback invoked after an image opacity changes.
 */
void renderOpacityBlenderWindow(
  AppData& appData,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageUniforms);
