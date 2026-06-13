#include "ui/dialogs/NativeMessageDialogs.h"

namespace native_dialog
{
/**
 * @brief Fallback implementation for platforms without native message dialog support.
 *
 * @return Always std::nullopt so callers can render their existing ImGui modal.
 */
std::optional<MessageDialogResult> showMessageDialog(const MessageDialog&)
{
  return std::nullopt;
}
} // namespace native_dialog
