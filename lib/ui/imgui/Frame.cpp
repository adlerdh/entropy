#include "ui/ImGuiWrapper.h"

#include "logic/app/CallbackHandler.h"
#include "logic/app/Data.h"
#include "logic/app/UserPreferences.h"
#include "ui/imgui/Layout.h"
#include "ui/popups/Popups.h"

#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <functional>
#include <utility>

using namespace ui::imgui_detail;

void ImGuiWrapper::render()
{
  const auto defaultProjectSaveDirectory = std::bind_front(&ImGuiWrapper::defaultProjectSaveDirectory, this);
  const auto defaultProjectSaveName = std::bind_front(&ImGuiWrapper::defaultProjectSaveName, this);
  const auto saveUserSettingsToDefault = std::bind_front(&ImGuiWrapper::saveUserSettingsToDefault, this);

  const bool loadingOrImporting = m_appData.state().animating();

  generateIsosurfaceMeshGpuRecords();
  if (!loadingOrImporting) {
    processComponentProjectionFutures();
  }
  processWarpInversionFutures();
  processRegistrationJobFutures();
  requestQueuedRegistrationJobs();
  if (m_appData.guiData().m_exportJobs) {
    m_appData.guiData().m_exportJobs->dispatchCompletion();
  }

  if (m_pendingUserScaleOverride) {
    m_uiScaleManager.setUserScaleOverride(*m_pendingUserScaleOverride);
    m_appData.guiData().m_effectiveUiScale = m_uiScaleManager.effectiveScale();
    m_pendingUserScaleOverride.reset();
  }
  if (m_pendingFontReload) {
    m_uiScaleManager.rebuildFontsForCurrentScale();
    m_pendingFontReload = false;
  }

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();

  if (updateLayoutTabBarHeight(m_appData.guiData()) && m_readjustViewport) {
    m_readjustViewport();
  }

  ImGui::NewFrame();

  requestAutomaticUpdateCheckIfNeeded();
  processUpdateCheckFuture();

  const ProjectLoadState projectLoadState = m_appData.state().projectLoadState();
  const bool backgroundTaskRunning = m_appData.state().animating();
  const bool hasLoadedProject =
    ProjectLoadState::Loaded == projectLoadState && 0 != m_appData.windowData().numLayouts();

  if (hasLoadedProject) {
    const ImGuiID dockspaceId = renderMainDockspace(m_appData.guiData());
    if (m_applyDefaultPanelLayout) {
      applyDefaultPanelDockLayout(dockspaceId, m_appData);
      ImGui::SaveIniSettingsToDisk(m_iniFileName.c_str());
      m_applyDefaultPanelLayout = false;
    }
    if (keepDockTabsVisible(dockspaceId)) {
      ImGui::SaveIniSettingsToDisk(m_iniFileName.c_str());
    }
    if (updateRenderViewportFromDockspace(m_appData, dockspaceId)) {
      if (m_readjustViewport) {
        m_readjustViewport();
      }
      if (m_postEmptyGlfwEvent) {
        m_postEmptyGlfwEvent();
      }
    }
  }
  else if (m_appData.guiData().m_renderViewport) {
    m_appData.guiData().m_renderViewport = std::nullopt;
    if (m_readjustViewport) {
      m_readjustViewport();
    }
  }

  user_preferences::updateAppSettingsDirtyState(
    m_appData.settings(),
    m_appData.applicationRenderPreferences(),
    m_appData.guiData());

  renderConfirmCloseAppPopup(m_appData, m_quitAppWithoutPrompt);
  renderUnsavedAppSettingsPopup(m_appData, saveUserSettingsToDefault, m_quitAppWithoutPrompt);
  renderUnsavedProjectPopup(
    m_appData,
    m_saveProject,
    m_saveProjectAs,
    m_closeProjectWithoutPrompt,
    m_quitAppWithoutPrompt,
    defaultProjectSaveDirectory,
    defaultProjectSaveName);

  if (!renderApplicationWindows(loadingOrImporting, hasLoadedProject, projectLoadState, backgroundTaskRunning)) {
    return;
  }

  if (ProjectLoadState::Loaded != m_appData.state().projectLoadState() || 0 == m_appData.windowData().numLayouts()) {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return;
  }

  if (!renderViewOverlays()) {
    return;
  }

  m_callbackHandler.refreshBrushPreviewIfNeeded();

  if (
    m_postEmptyGlfwEvent &&
    (ImGui::IsAnyItemActive() || ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
     ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
     ImGui::IsMouseReleased(ImGuiMouseButton_Right)))
  {
    m_postEmptyGlfwEvent();
  }

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
