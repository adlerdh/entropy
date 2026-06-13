#include "ui/dialogs/NativeMessageDialogModel.h"

namespace native_dialog::model
{
std::vector<std::string> buttonLabels(const MessageDialog& dialog)
{
  std::vector<std::string> labels;
  labels.reserve(3);

  labels.push_back(dialog.firstButton);
  labels.push_back(dialog.secondButton);
  if (!dialog.thirdButton.empty()) {
    labels.push_back(dialog.thirdButton);
  }

  return labels;
}

std::string combinedInformativeText(const std::string& message, const std::string& informativeText)
{
  if (message.empty()) {
    return informativeText;
  }
  if (informativeText.empty()) {
    return message;
  }
  return message + "\n\n" + informativeText;
}

std::string fallbackMessageText(const MessageDialog& dialog)
{
  return combinedInformativeText(dialog.message, dialog.informativeText);
}
} // namespace native_dialog::model
