#pragma once

#include "ui/GuiData.h"

#include <functional>

class AppData;

/**
 * @brief Render the standard-raster image geometry prompt.
 */
void renderRasterImageHeaderPromptPopup(
  AppData& appData,
  const std::function<
    void(GuiData::RasterImageHeaderDecision decision, ImageSpatialMetadata metadata, bool applyToAll)>& handleDecision);
