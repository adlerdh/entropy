#pragma once

#include "ui/dialogs/NativeMessageDialogs.h"

#include <string>
#include <vector>

namespace native_dialog::model
{
/**
 * @brief Return the ordered button labels that should be shown for a message dialog.
 *
 * @param dialog Dialog definition with two required buttons and one optional third button.
 * @return Non-empty button labels in display order.
 */
std::vector<std::string> buttonLabels(const MessageDialog& dialog);

/**
 * @brief Combine the main and optional informative text using platform dialog spacing.
 *
 * @param message Primary message text.
 * @param informativeText Optional additional details.
 * @return Combined detail text with a blank line between non-empty sections.
 */
std::string combinedInformativeText(const std::string& message, const std::string& informativeText);

/**
 * @brief Combine the message fields that should be shown by simple fallback dialogs.
 *
 * @param dialog Dialog definition.
 * @return Text containing the main message and optional informative details.
 */
std::string fallbackMessageText(const MessageDialog& dialog);
} // namespace native_dialog::model
