#include "ui/dialogs/InputLoadErrorDialog.h"

#include <cctype>
#include <utility>

namespace
{
std::string displayType(std::string type)
{
  if (type.empty()) return "Input Data";
  type.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(type.front())));
  return type;
}

std::string userFacingCause(std::string cause)
{
  if (cause.starts_with("[in function")) {
    const std::size_t metadataEnd = cause.find("] ");
    if (metadataEnd != std::string::npos) cause.erase(0, metadataEnd + 2u);
  }
  if (cause.empty()) return "The input is invalid, unsupported, unreadable, or could not be parsed.";
  return cause;
}
} // namespace

namespace native_dialog
{
MessageDialog inputLoadErrorDialog(const InputLoadError& error)
{
  const std::string inputType = error.inputType.empty() ? "input data" : error.inputType;
  const std::string type = displayType(inputType);
  std::string message = "Entropy could not load the selected " + inputType + ".";
  std::string details = "Input type: " + type;
  if (error.path && !error.path->empty()) {
    const std::string name = error.path->filename().string();
    message = "Entropy could not load \"" + (name.empty() ? error.path->string() : name) + "\".";
    details += "\nPath: " + error.path->string();
  }
  details += "\n\nCause: " + userFacingCause(error.cause);
  return {
    .title = "Unable to Load " + type,
    .message = std::move(message),
    .informativeText = std::move(details),
    .firstButton = "OK",
    .secondButton = "",
    .thirdButton = "",
    .severity = MessageDialogSeverity::Error};
}

} // namespace native_dialog
