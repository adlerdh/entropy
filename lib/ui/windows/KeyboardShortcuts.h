#pragma once

#include <string>
#include <vector>

namespace ui
{
struct KeyboardShortcutRow
{
  std::string section;
  std::string shortcut;
  std::string action;
  std::string details;
};

const std::vector<KeyboardShortcutRow>& keyboardShortcutRows();
const std::vector<KeyboardShortcutRow>& threeDViewControlRows();

void renderKeyboardShortcutsWindow(bool& open);
} // namespace ui
