#include "ui/MainMenuBar.h"

#include <imgui/imgui.h>

void renderMainMenuBar(GuiData& uiData)
{
  if (!uiData.m_showMainMenuBar)
  {
    return;
  }

  if (ImGui::BeginMainMenuBar())
  {
    const ImVec2 winSize = ImGui::GetWindowSize();
    uiData.m_mainMenuBarDims = glm::vec2{winSize.x, winSize.y};

    if (ImGui::BeginMenu("File"))
    {
      if (ImGui::MenuItem("New"))
      {
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit"))
    {
      if (ImGui::MenuItem("Item"))
      {
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Modes"))
    {
      if (ImGui::MenuItem("Item"))
      {
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Windows"))
    {
      if (ImGui::MenuItem("Item"))
      {
      }
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }
}
