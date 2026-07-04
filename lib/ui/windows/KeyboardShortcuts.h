#pragma once

#include <string>
#include <vector>

namespace entropy::ui
{
struct KeyboardShortcutRow
{
  std::string section;
  std::string shortcut;
  std::string action;
  std::string details;
};

const std::vector<KeyboardShortcutRow>& keyboardShortcutRows();

void renderKeyboardShortcutsWindow(bool& open);
} // namespace entropy::ui
