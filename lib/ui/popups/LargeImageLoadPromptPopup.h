#pragma once

#include "ui/GuiData.h"

#include <uuid.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class AppData;

/**
 * @brief Render the large-image load policy prompt.
 */
void renderLargeImageLoadPromptPopup(
  AppData& appData,
  const std::function<void(GuiData::LargeImageLoadDecision decision)>& handleDecision);
