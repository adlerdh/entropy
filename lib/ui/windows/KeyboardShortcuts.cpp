#include "ui/windows/KeyboardShortcuts.h"

#include <imgui/imgui.h>

namespace entropy::ui
{
namespace
{
void sectionRow(const char* title, int columnCount)
{
  ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
  ImGui::TableNextColumn();
  ImGui::TextUnformatted(title);
  for (int i = 1; i < columnCount; ++i) {
    ImGui::TableNextColumn();
    ImGui::TextUnformatted("");
  }
}

std::string commandShortcut(const char* key)
{
#ifdef __APPLE__
  return std::string{"Command + "} + key;
#else
  return std::string{"Ctrl + "} + key;
#endif
}

std::string optionShortcut(const char* key)
{
#ifdef __APPLE__
  return std::string{"Option + "} + key;
#else
  return std::string{"Alt + "} + key;
#endif
}
} // namespace

const std::vector<KeyboardShortcutRow>& keyboardShortcutRows()
{
  static const std::vector<KeyboardShortcutRow> k_rows{
    {"File", commandShortcut("O"), "Open image(s)", "Open one or more image files"},
    {"File", commandShortcut("Shift + O"), "Open project", "Open an Entropy project file"},
    {"File", commandShortcut("S"), "Save project", "Save the current project"},
    {"File", commandShortcut("Shift + S"), "Save project as", "Save the current project to a new file"},
    {"File", commandShortcut("Q"), "Quit", "Close Entropy"},

    {"Modes", "V", "Pointer", "Select the pointer/crosshairs mode"},
    {"Modes", "L", "Window / level", "Select window, level, and opacity adjustment mode"},
    {"Modes", "Z", "Zoom", "Select view zoom mode"},
    {"Modes", "X", "Pan / dolly", "Select view pan and dolly mode"},
    {"Modes", "B", "Segmentation brush", "Select segmentation painting mode"},
    {"Modes", "T", "Translate image", "Select manual image translation mode"},
    {"Modes", "R", "Rotate image", "Select manual image rotation mode"},
    {"Modes", "Y", "Scale image", "Select manual image scaling mode"},

    {"View", "C", "Recenter views", "Recenter views on the current crosshairs position"},
    {"View", "Shift + C", "Reset views and crosshairs", "Recenter crosshairs, realign orientations, and reset zoom"},
    {"View", "F4", "Enter full screen", "Toggle full-screen display"},
    {"View", "Esc", "Exit full screen", "Leave full-screen display when active"},
    {"View", "O", "Cycle overlays", "Cycle visibility of view overlays and UI overlays"},

    {"Images", "W", "Show image", "Toggle active image visibility"},
    {"Images", "Shift + E", "Show image edges", "Toggle image edge rendering"},
    {"Images", "Q", "Decrease image opacity", "Decrease active image opacity"},
    {"Images", "E", "Increase image opacity", "Increase active image opacity"},
    {"Images", "Shift + [", "Previous active image", "Select the previous loaded image"},
    {"Images", "Shift + ]", "Next active image", "Select the next loaded image"},
    {"Images", "Shift + Page Down", "Previous component", "Select the previous image component"},
    {"Images", "Shift + Page Up", "Next component", "Select the next image component"},

    {"Segmentation", "S", "Show segmentation", "Toggle active segmentation visibility"},
    {"Segmentation", "Space", "Show segmentation outline", "Toggle global segmentation outline visibility"},
    {"Segmentation", "A", "Decrease segmentation opacity", "Decrease segmentation opacity"},
    {"Segmentation", "D", "Increase segmentation opacity", "Increase segmentation opacity"},
    {"Segmentation", "Shift + A", "Decrease segmentation fill opacity", "Decrease segmentation interior opacity"},
    {"Segmentation", "Shift + D", "Increase segmentation fill opacity", "Increase segmentation interior opacity"},
    {"Segmentation", ",", "Previous foreground label", "Select the previous foreground segmentation label"},
    {"Segmentation", ".", "Next foreground label", "Select the next foreground segmentation label"},
    {"Segmentation", "Shift + ,", "Previous background label", "Select the previous background segmentation label"},
    {"Segmentation", "Shift + .", "Next background label", "Select the next background segmentation label"},
    {"Segmentation", "-", "Decrease brush size", "Decrease segmentation brush size"},
    {"Segmentation", "+ / =", "Increase brush size", "Increase segmentation brush size"},

    {"Navigation", "Page Down", "Previous slice", "Scroll the hovered view backward by one slice"},
    {"Navigation", "Page Up", "Next slice", "Scroll the hovered view forward by one slice"},
    {"Navigation", "Left", "Move crosshairs left", "Move crosshairs left in the hovered view"},
    {"Navigation", "Right", "Move crosshairs right", "Move crosshairs right in the hovered view"},
    {"Navigation", "Up", "Move crosshairs up", "Move crosshairs up in the hovered view"},
    {"Navigation", "Down", "Move crosshairs down", "Move crosshairs down in the hovered view"},

    {"Layouts", "[", "Previous layout", "Switch to the previous layout"},
    {"Layouts", "]", "Next layout", "Switch to the next layout"},

    {"Time series", optionShortcut(","), "Previous frame", "Move the active time-series image to the previous frame"},
    {"Time series", optionShortcut("."), "Next frame", "Move the active time-series image to the next frame"},
  };
  return k_rows;
}

void renderKeyboardShortcutsWindow(bool& open)
{
  if (!open) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2{780.0f, 560.0f}, ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Keyboard Shortcuts", &open)) {
    ImGui::End();
    return;
  }

  constexpr ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersInnerV |
                                    ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                                    ImGuiTableFlags_Reorderable | ImGuiTableFlags_ScrollY |
                                    ImGuiTableFlags_SizingStretchProp;

  if (ImGui::BeginTable("KeyboardShortcutsTable", 3, flags, ImVec2{0.0f, 0.0f})) {
    ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 130.0f);
    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 220.0f);
    ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    std::string currentSection;
    for (const KeyboardShortcutRow& row : keyboardShortcutRows()) {
      if (row.section != currentSection) {
        currentSection = row.section;
        sectionRow(currentSection.c_str(), 3);
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(row.shortcut.c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(row.action.c_str());
      ImGui::TableNextColumn();
      ImGui::TextWrapped("%s", row.details.c_str());
    }

    ImGui::EndTable();
  }

  ImGui::End();
}
} // namespace entropy::ui
