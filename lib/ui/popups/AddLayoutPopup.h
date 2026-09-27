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
 * @brief Render the modal popup used to create a new layout.
 */
void renderAddLayoutModalPopup(
  AppData& appData,
  bool openAddLayoutPopup,
  const std::function<void(void)>& recenterViews);
