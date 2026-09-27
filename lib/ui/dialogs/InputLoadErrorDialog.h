#pragma once

#include "ui/dialogs/NativeMessageDialogs.h"

#include <filesystem>
#include <optional>
#include <string>

namespace native_dialog
{
/** @brief Information needed to explain a failed user-requested input load. */
struct InputLoadError
{
  std::string inputType;
  std::optional<std::filesystem::path> path;
  std::string cause;
};

/** @brief Build a concise, user-facing native error dialog for an input load failure. */
MessageDialog inputLoadErrorDialog(const InputLoadError& error);

/** @brief Show a failed input load in a native error dialog. */
void showInputLoadErrorDialog(const InputLoadError& error);
} // namespace native_dialog
