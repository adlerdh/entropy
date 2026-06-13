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
 * @brief Render the About Entropy modal dialog.
 */
void renderAboutDialogModalPopup(bool open);
