#include "ui/ImGuiWrapper.h"

#include "logic/app/AppPaths.h"
#include "logic/app/Data.h"
#include "ui/AboutIcon.h"
#include "ui/GradientBackgroundRenderer.h"
#include "ui/Style.h"
#include "ui/imgui/Layout.h"

#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <implot/implot.h>

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <functional>
#include <utility>

using namespace ui::imgui_detail;

ImGuiWrapper::ImGuiWrapper(GLFWwindow* window, AppData& appData, CallbackHandler& callbackHandler)
  : m_appData(appData), m_callbackHandler(callbackHandler), m_window(window)
{
  IMGUI_CHECKVERSION();

  ImGui::CreateContext();
  spdlog::debug("Created ImGui context");

  ImPlot::CreateContext();
  spdlog::debug("Created ImPlot context");

  ImGuiIO& io = ImGui::GetIO();

  m_iniFilePath = app_paths::userDataDirectory() / "entropy_ui.ini";
  m_logFilePath = app_paths::logDirectory() / "entropy_ui.log";
  std::filesystem::create_directories(m_iniFilePath.parent_path());
  std::filesystem::create_directories(m_logFilePath.parent_path());

  m_iniFileName = m_iniFilePath.string();
  m_logFileName = m_logFilePath.string();
  m_applyDefaultPanelLayout = !savedDockspaceLayoutExists(m_iniFilePath);
  if (m_applyDefaultPanelLayout) appData.guiData().m_showSegmentationsWindow = true;
  io.IniFilename = m_iniFileName.c_str();
  io.LogFilename = m_logFileName.c_str();

  io.ConfigDragClickToInputText = true;
  //    io.MouseDrawCursor = true;
  /// @todo Add window option for unsaved document (a little dot) when project changes

  io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  // Setup ImGui platform/renderer bindings:
  const char* const glsl_version = "#version 150";
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  applyUiStylePreset(appData.settings().uiColorPreset());
  ::applyUiDensityPreset(appData.settings().uiDensityPreset());
  ::applyUiWindowBgOpacity(appData.settings().uiWindowBgOpacity());

  m_uiScaleManager.captureBaseStyle(ImGui::GetStyle());
  m_uiScaleManager.setFontReloadCallback([this](float scale) { initializeFonts(scale); });

  spdlog::debug("Done setup of ImGui platform and renderer bindings");

  m_uiScaleManager.setUserScaleOverride(appData.settings().uiScaleOverride());
  setContentScale(appData.windowData().getContentScaleRatio());
}

ImGuiWrapper::~ImGuiWrapper()
{
  about_entropy_icon::releaseTexture();
  ui::releaseGradientBackgroundResources();
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();

  ImPlot::DestroyContext();
  spdlog::debug("Destroyed ImPlot context");

  ImGui::DestroyContext();
  spdlog::debug("Destroyed ImGui context");
}

void ImGuiWrapper::setCallbacks(ImGuiWrapperCallbacks callbacks)
{
  m_postEmptyGlfwEvent = std::move(callbacks.platform.postEmptyGlfwEvent);
  m_readjustViewport = std::move(callbacks.platform.readjustViewport);

  m_openImageFiles = std::move(callbacks.project.openImageFiles);
  m_addImageFiles = std::move(callbacks.project.addImageFiles);
  m_openDicomFolders = std::move(callbacks.project.openDicomFolders);
  m_addSegmentationFile = std::move(callbacks.project.addSegmentationFile);
  m_addSegmentationFileToImage = std::move(callbacks.project.addSegmentationFileToImage);
  m_importSurfaceMeshes = std::move(callbacks.project.importSurfaceMeshes);
  m_exportIsosurfaceMesh = std::move(callbacks.project.exportIsosurfaceMesh);
  m_exportSegmentationLabelMesh = std::move(callbacks.project.exportSegmentationLabelMesh);
  m_exportAllSegmentationLabelMeshes = std::move(callbacks.project.exportAllSegmentationLabelMeshes);
  m_loadDeformationField = std::move(callbacks.project.loadDeformationField);
  m_loadAndAssignDeformationField = std::move(callbacks.project.loadAndAssignDeformationField);
  m_importRegistrationJobOutputs = std::move(callbacks.project.importRegistrationJobOutputs);
  m_openProjectFile = std::move(callbacks.project.openProjectFile);
  m_largeImageLoadDecision = std::move(callbacks.project.largeImageLoadDecision);
  m_rasterImageHeaderDecision = std::move(callbacks.project.rasterImageHeaderDecision);
  m_loadDicomSeries = std::move(callbacks.project.loadDicomSeries);
  m_saveProject = std::move(callbacks.project.saveProject);
  m_saveProjectAs = std::move(callbacks.project.saveProjectAs);
  m_closeProject = std::move(callbacks.project.closeProject);
  m_loadLayoutsFile = std::move(callbacks.project.loadLayoutsFile);
  m_saveLayoutsFile = std::move(callbacks.project.saveLayoutsFile);
  m_resetProjectSettings = std::move(callbacks.project.resetProjectSettings);
  m_closeProjectWithoutPrompt = std::move(callbacks.project.closeProjectWithoutPrompt);
  m_requestQuitApp = std::move(callbacks.project.requestQuitApp);
  m_quitAppWithoutPrompt = std::move(callbacks.project.quitAppWithoutPrompt);

  m_recenterView = std::move(callbacks.view.recenterView);
  m_recenterAllViews = std::move(callbacks.view.recenterCurrentViews);
  m_getOverlayVisibility = std::move(callbacks.view.getOverlayVisibility);
  m_setOverlayVisibility = std::move(callbacks.view.setOverlayVisibility);
  m_updateAllImageUniforms = std::move(callbacks.view.updateAllImageUniforms);
  m_updateImageUniforms = std::move(callbacks.view.updateImageUniforms);
  m_updateImageInterpolationMode = std::move(callbacks.view.updateImageInterpolationMode);
  m_updateImageColorMapInterpolationMode = std::move(callbacks.view.updateImageColorMapInterpolationMode);
  m_updateLabelColorTableTexture = std::move(callbacks.view.updateLabelColorTableTexture);
  m_moveCrosshairsToSegLabelCentroid = std::move(callbacks.view.moveCrosshairsToSegLabelCentroid);
  m_updateMetricUniforms = std::move(callbacks.view.updateMetricUniforms);
  m_exportAsciiClipboardPayloadForView = std::move(callbacks.view.exportAsciiClipboardPayloadForView);

  m_getWorldDeformedPos = std::move(callbacks.inspection.getWorldDeformedPos);
  m_getSubjectPos = std::move(callbacks.inspection.getSubjectPos);
  m_getVoxelPos = std::move(callbacks.inspection.getVoxelPos);
  m_setSubjectPos = std::move(callbacks.inspection.setSubjectPos);
  m_setVoxelPos = std::move(callbacks.inspection.setVoxelPos);
  m_getImageValuesNN = std::move(callbacks.inspection.getImageValuesNN);
  m_getImageValuesLinear = std::move(callbacks.inspection.getImageValuesLinear);
  m_getSegLabel = std::move(callbacks.inspection.getSegLabel);

  m_createBlankSeg = std::move(callbacks.editing.createBlankSeg);
  m_clearSeg = std::move(callbacks.editing.clearSeg);
  m_removeSeg = std::move(callbacks.editing.removeSeg);
  m_executePoissonSeg = std::move(callbacks.editing.executePoissonSeg);
  m_setLockManualImageTransformation = std::move(callbacks.editing.setLockManualImageTransformation);
  m_setReferenceImage = std::move(callbacks.editing.setReferenceImage);
  m_removeImage = std::move(callbacks.editing.removeImage);
  m_paintActiveSegmentationWithActivePolygon = std::move(callbacks.editing.paintActiveSegmentationWithActivePolygon);
}
