#include "ui/ImGuiWrapper.h"

#include "common/MathFuncs.h"

#include "ui/Helpers.h"
#include "ui/ImageExport.h"
#include "ui/dialogs/NativeMessageDialogs.h"
#include "ui/menus/MainMenuBar.h"
#ifdef __APPLE__
#include "ui/menus/MacNativeMainMenu.h"
#endif
#include "ui/NativeFileDialogs.h"
#include "ui/popups/Popups.h"
#include "ui/Style.h"
#include "ui/toolbars/Toolbars.h"
#include "ui/windows/Windows.h"
#include "ui/windows/OpacityMixerWindow.h"
#ifdef _WIN32
#include "ui/menus/WinNativeMainMenu.h"
#endif

#include "logic/app/AppPaths.h"
#include "logic/app/CallbackHandler.h"
#include "logic/app/Data.h"
#include "logic/app/StackTrace.h"
#include "logic/app/UserPreferences.h"
#include "logic/annotation/PointRecord.h"
#include "logic/annotation/SerializeAnnot.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/serialization/ProjectSerialization.h"
#include "logic/states/annotation/AnnotationStateHelpers.h"
#include "logic/states/annotation/AnnotationStateMachine.h"

#include "common/UuidUtility.h"
#include "image/TimePlaybackController.h"
#include "registration/Artifacts.h"
#include "registration/AffineTransformIO.h"
#include "registration/Availability.h"
#include "registration/Config.h"
#include "registration/ImportPlan.h"
#include "registration/Process.h"
#include "rendering/TextureSetup.h"

#include <IconsForkAwesome.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

// GLFW and OpenGL 3 bindings for ImGui:
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include <implot/implot.h>

#include <cmrc/cmrc.hpp>

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <format>
#include <iomanip>
#include <iterator>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

CMRC_DECLARE(fonts);

namespace fs = std::filesystem;

namespace
{
static const glm::quat k_identityRotation{1.0f, 0.0f, 0.0f, 0.0f};
static const glm::vec3 k_zeroVec{0.0f, 0.0f, 0.0f};
constexpr float k_layoutTabWindowPaddingX = 6.0f;
constexpr float k_layoutTabWindowPaddingY = 0.0f;
constexpr float k_layoutTabFrameRounding = 3.0f;
constexpr double k_maxTimePlaybackFramesPerSecond = 120.0;

class ProcessExecutableProbe final : public registration::IExecutableProbe
{
public:
  explicit ProcessExecutableProbe(registration::IProcessRunner& runner) : m_runner(runner) {}

  registration::ExecutableProbeResult probe(
    const std::filesystem::path& executable,
    const std::vector<std::string>& arguments) override
  {
    registration::CommandSpec command;
    command.executable = executable.string();
    command.args = arguments;
    command.description = "Registration backend compatibility check";

    registration::ProcessOptions options;
    options.mergeStdErrIntoStdOut = true;
    const registration::ProcessResult processResult = m_runner.run(command, options, {});

    registration::ExecutableProbeResult result;
    result.found = !processResult.launchFailed && processResult.exitCode != 127;
    result.exitCode = processResult.exitCode;
    result.failureMessage = processResult.failureMessage;
    for (const registration::ProcessOutputLine& line : processResult.outputLines) {
      if (line.stream == registration::OutputStream::Stderr) {
        result.standardError += line.text;
        result.standardError += '\n';
      }
      else {
        result.standardOutput += line.text;
        result.standardOutput += '\n';
      }
    }
    return result;
  }

private:
  registration::IProcessRunner& m_runner;
};

float scaledPixel(float value)
{
  return value * (ImGui::GetFontSize() / 16.0f);
}

struct LayoutTabMetrics
{
  ImVec2 windowPadding;
  float frameRounding = 0.0f;
  float windowHeight = 0.0f;
  float innerGap = 0.0f;
};

LayoutTabMetrics layoutTabMetrics()
{
  const ImVec2 windowPadding{scaledPixel(k_layoutTabWindowPaddingX), scaledPixel(k_layoutTabWindowPaddingY)};
  const float windowHeight = ImGui::GetFrameHeight() + (2.0f * windowPadding.y);
  const float dockspaceClearance = std::max(1.0f, ImGui::GetStyle().TabBarBorderSize);
  return LayoutTabMetrics{
    .windowPadding = windowPadding,
    .frameRounding = scaledPixel(k_layoutTabFrameRounding),
    .windowHeight = windowHeight,
    .innerGap = dockspaceClearance};
}

bool pathIsWithinDirectory(const fs::path& path, const fs::path& directory)
{
  std::error_code error;
  const fs::path normalizedPath = fs::weakly_canonical(path, error);
  if (error) {
    return false;
  }

  const fs::path normalizedDirectory = fs::weakly_canonical(directory, error);
  if (error) {
    return false;
  }

  auto pathIt = normalizedPath.begin();
  for (auto dirIt = normalizedDirectory.begin(); dirIt != normalizedDirectory.end(); ++dirIt, ++pathIt) {
    if (pathIt == normalizedPath.end() || *pathIt != *dirIt) {
      return false;
    }
  }
  return true;
}

void removeTemporaryRegistrationInputs(const registration::JobSpec& job, registration::JobExecution& execution)
{
  if (job.outputDirectory.empty()) {
    return;
  }

  std::error_code error;
  const fs::path tempDirectory = fs::temp_directory_path(error);
  if (error || !pathIsWithinDirectory(job.outputDirectory, tempDirectory)) {
    return;
  }

  for (const registration::InputArtifact& artifact : registration::buildInputArtifactPlan(job)) {
    if (artifact.path.empty() || !pathIsWithinDirectory(artifact.path, job.outputDirectory)) {
      continue;
    }

    error.clear();
    if (fs::is_regular_file(artifact.path, error) && !error) {
      error.clear();
      (void)fs::remove(artifact.path, error);
      if (error) {
        execution.warnings.push_back(
          "Could not remove temporary registration input " + artifact.path.string() + ": " + error.message());
      }
    }
  }

  if (!job.initialAffineTransform.empty() && pathIsWithinDirectory(job.initialAffineTransform, job.outputDirectory)) {
    error.clear();
    if (fs::is_regular_file(job.initialAffineTransform, error) && !error) {
      error.clear();
      (void)fs::remove(job.initialAffineTransform, error);
      if (error) {
        execution.warnings.push_back(
          "Could not remove temporary registration input " + job.initialAffineTransform.string() + ": " +
          error.message());
      }
    }
  }
}

bool checkRegistrationBackendBeforeLaunch(
  const registration::JobSpec& job,
  const registration::BackendConfig& config,
  registration::IProcessRunner& runner,
  registration::JobExecution& execution)
{
  if (job.backend == registration::Backend::FireANTs) {
    return true;
  }

  ProcessExecutableProbe probe{runner};
  const registration::BackendAvailability availability =
    registration::checkBackendAvailability(job.backend, config, probe);
  if (availability.status != registration::BackendAvailabilityStatus::Available) {
    execution.status = registration::JobStatus::Failed;
    execution.errorMessage = availability.message;
    return false;
  }

  if (availability.compatibility != registration::BackendCompatibilityStatus::Compatible) {
    execution.warnings.push_back(availability.compatibilityMessage);
  }
  return true;
}

std::optional<glm::dmat4> readBackendSamplingAffine(const fs::path& affinePath, registration::Backend backend)
{
  return (backend == registration::Backend::Greedy) ? registration::readGreedyAffineTransform(affinePath)
                                                    : registration::readAffineTransform(affinePath);
}

bool applyImportedAffineToImage(
  AppData& appData,
  const uuids::uuid& imageUid,
  const fs::path& affinePath,
  registration::Backend backend)
{
  Image* image = appData.image(imageUid);
  if (!image) {
    return false;
  }

  const std::optional<glm::dmat4> samplingAffine = readBackendSamplingAffine(affinePath, backend);
  if (!samplingAffine) {
    return false;
  }
  const glm::dmat4 displayAffine = glm::inverse(*samplingAffine);

  ImageTransformations& imageTx = image->transformations();
  const bool wasLocked = imageTx.is_worldDef_T_affine_locked();
  imageTx.set_worldDef_T_affine_locked(false);
  imageTx.set_enable_affine_T_subject(true);
  imageTx.set_affine_T_subject(glm::mat4{displayAffine});
  imageTx.set_affine_T_subject_fileName(affinePath);
  imageTx.set_enable_worldDef_T_affine(true);
  imageTx.reset_worldDef_T_affine();
  imageTx.set_worldDef_T_affine_locked(wasLocked);

  for (const uuids::uuid& segUid : appData.imageToSegUids(imageUid)) {
    Image* seg = appData.seg(segUid);
    if (!seg) {
      continue;
    }

    ImageTransformations& segTx = seg->transformations();
    const bool wasSegLocked = segTx.is_worldDef_T_affine_locked();
    segTx.set_worldDef_T_affine_locked(false);
    segTx.set_enable_affine_T_subject(imageTx.get_enable_affine_T_subject());
    segTx.set_affine_T_subject(imageTx.get_affine_T_subject());
    segTx.set_affine_T_subject_fileName(affinePath);
    segTx.set_enable_worldDef_T_affine(imageTx.get_enable_worldDef_T_affine());
    segTx.reset_worldDef_T_affine();
    segTx.set_worldDef_T_affine_locked(wasSegLocked);
  }

  return true;
}

bool updateLayoutTabBarHeight(GuiData& guiData)
{
  const LayoutTabMetrics metrics = layoutTabMetrics();
  if (
    std::abs(guiData.m_layoutTabBarHeight - metrics.windowHeight) < 0.5f &&
    std::abs(guiData.m_layoutTabInnerGap - metrics.innerGap) < 0.5f)
  {
    return false;
  }

  guiData.m_layoutTabBarHeight = metrics.windowHeight;
  guiData.m_layoutTabInnerGap = metrics.innerGap;
  return true;
}

bool savedDockspaceLayoutExists(const std::filesystem::path& iniFilePath)
{
  std::ifstream stream{iniFilePath};
  if (!stream) {
    return false;
  }

  std::string line;
  while (std::getline(stream, line)) {
    if (
      line.find("EntropyMainDockspace") != std::string::npos || line.find("EntropyDockspaceHost") != std::string::npos)
    {
      return true;
    }
  }

  return false;
}

struct DockspaceGeometry
{
  ImVec2 pos;
  ImVec2 size;
};

DockspaceGeometry mainDockspaceGeometry(const GuiData& guiData)
{
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  DockspaceGeometry geometry{viewport->Pos, viewport->Size};

  const GuiData::Margins toolbarMargins = guiData.computeToolbarMargins();
  const GuiData::Margins chromeMargins = guiData.computeMargins();

  const float left = chromeMargins.left - toolbarMargins.left;
  const float right = chromeMargins.right - toolbarMargins.right;
  const float top = chromeMargins.top - toolbarMargins.top;
  const float bottom = chromeMargins.bottom - toolbarMargins.bottom;

  geometry.pos.x += left;
  geometry.pos.y += top;
  geometry.size.x = std::max(1.0f, geometry.size.x - (left + right));
  geometry.size.y = std::max(1.0f, geometry.size.y - (top + bottom));

  return geometry;
}

ImGuiID renderMainDockspace(const GuiData& guiData)
{
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (!viewport) {
    return 0;
  }

  const DockspaceGeometry geometry = mainDockspaceGeometry(guiData);

  ImGui::SetNextWindowPos(geometry.pos, ImGuiCond_Always);
  ImGui::SetNextWindowSize(geometry.size, ImGuiCond_Always);
  ImGui::SetNextWindowViewport(viewport->ID);

  constexpr ImGuiWindowFlags hostWindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                               ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                               ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
  ImGui::Begin("EntropyDockspaceHost", nullptr, hostWindowFlags);
  ImGui::PopStyleVar(3);

  const ImGuiID dockspaceId = ImGui::GetID("EntropyMainDockspace");
  ImGui::DockSpace(
    dockspaceId,
    ImVec2{0.0f, 0.0f},
    ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingInCentralNode);

  ImGui::End();
  return dockspaceId;
}

bool keepDockTabsVisible(ImGuiDockNode* node)
{
  if (!node) {
    return false;
  }

  bool changed = false;
  const ImGuiDockNodeFlags visibleTabFlags =
    node->LocalFlags &
    ~(ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_HiddenTabBar | ImGuiDockNodeFlags_NoTabBar);
  if (visibleTabFlags != node->LocalFlags) {
    node->SetLocalFlags(visibleTabFlags);
    changed = true;
  }

  changed = keepDockTabsVisible(node->ChildNodes[0]) || changed;
  changed = keepDockTabsVisible(node->ChildNodes[1]) || changed;
  return changed;
}

bool keepDockTabsVisible(ImGuiID dockspaceId)
{
  return keepDockTabsVisible(ImGui::DockBuilderGetNode(dockspaceId));
}

std::optional<glm::vec4> centralDockspaceRenderViewport(ImGuiID dockspaceId, int windowHeight)
{
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const ImGuiDockNode* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId);
  if (!viewport || !centralNode || centralNode->Size.x <= 1.0f || centralNode->Size.y <= 1.0f) {
    return std::nullopt;
  }

  const float left = centralNode->Pos.x - viewport->Pos.x;
  const float top = centralNode->Pos.y - viewport->Pos.y;
  const float width = centralNode->Size.x;
  const float height = centralNode->Size.y;
  const float bottom = static_cast<float>(windowHeight) - (top + height);

  return glm::vec4{std::max(0.0f, left), std::max(0.0f, bottom), std::max(1.0f, width), std::max(1.0f, height)};
}

bool nearlyEqualViewport(const glm::vec4& a, const glm::vec4& b)
{
  constexpr float k_viewportTolerance = 0.5f;
  return std::abs(a.x - b.x) < k_viewportTolerance && std::abs(a.y - b.y) < k_viewportTolerance &&
         std::abs(a.z - b.z) < k_viewportTolerance && std::abs(a.w - b.w) < k_viewportTolerance;
}

bool updateRenderViewportFromDockspace(AppData& appData, ImGuiID dockspaceId)
{
  std::optional<glm::vec4>& renderViewport = appData.guiData().m_renderViewport;
  const std::optional<glm::vec4> nextViewport =
    centralDockspaceRenderViewport(dockspaceId, appData.windowData().getWindowSize().y);

  if (!nextViewport) {
    const bool changed = renderViewport.has_value();
    renderViewport = std::nullopt;
    return changed;
  }

  if (renderViewport && nearlyEqualViewport(*renderViewport, *nextViewport)) {
    return false;
  }

  renderViewport = nextViewport;
  return true;
}

float clampedDockSplitFraction(float availableSize, float targetFraction, float minSize, float maxSize)
{
  if (availableSize <= 1.0f) {
    return targetFraction;
  }

  const float largestUsefulSize = std::min(maxSize, availableSize * 0.45f);
  const float smallestUsefulSize = std::min(minSize, largestUsefulSize);
  const float targetSize = std::clamp(availableSize * targetFraction, smallestUsefulSize, largestUsefulSize);
  return targetSize / availableSize;
}

struct DefaultDockLayoutFractions
{
  float leftPanel = 0.20f;
  float rightPanel = 0.20f;
  float inspector = 0.10f;
  float opacityMixer = 1.0f / 3.0f;
};

DefaultDockLayoutFractions defaultDockLayoutFractions(const ImVec2& dockspaceSize)
{
  const float aspectRatio = dockspaceSize.y > 1.0f ? dockspaceSize.x / dockspaceSize.y : 1.0f;
  const bool wideWorkspace = aspectRatio >= 2.10f;
  const bool narrowWorkspace = aspectRatio <= 1.35f;

  const float sideTargetFraction = wideWorkspace ? 0.23f : (narrowWorkspace ? 0.18f : 0.20f);
  const float sideMinSize = narrowWorkspace ? 220.0f : 280.0f;
  const float sideMaxSize = wideWorkspace ? 640.0f : 520.0f;

  return DefaultDockLayoutFractions{
    .leftPanel = clampedDockSplitFraction(dockspaceSize.x, sideTargetFraction, sideMinSize, sideMaxSize),
    .rightPanel = clampedDockSplitFraction(dockspaceSize.x, sideTargetFraction, sideMinSize, sideMaxSize),
    .inspector = clampedDockSplitFraction(dockspaceSize.y, 0.10f, 120.0f, 190.0f),
    .opacityMixer = 1.0f / 3.0f};
}

void applyDefaultPanelDockLayout(ImGuiID dockspaceId, const GuiData& guiData)
{
  if (0 == dockspaceId) {
    return;
  }

  const DockspaceGeometry geometry = mainDockspaceGeometry(guiData);

  ImGui::DockBuilderRemoveNode(dockspaceId);
  ImGui::DockBuilderAddNode(
    dockspaceId,
    ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingInCentralNode);
  ImGui::DockBuilderSetNodePos(dockspaceId, geometry.pos);
  ImGui::DockBuilderSetNodeSize(dockspaceId, geometry.size);

  ImGuiID centerNode = dockspaceId;
  ImGuiID leftNode = 0;
  ImGuiID rightNode = 0;
  ImGuiID rightMiddleNode = 0;
  ImGuiID rightBottomNode = 0;
  ImGuiID bottomNode = 0;
  ImGuiID bottomRightNode = 0;

  const DefaultDockLayoutFractions fractions = defaultDockLayoutFractions(geometry.size);

  ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Left, fractions.leftPanel, &leftNode, &centerNode);
  ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Right, fractions.rightPanel, &rightNode, &centerNode);
  ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Down, fractions.inspector, &bottomNode, &centerNode);
  ImGui::DockBuilderSplitNode(bottomNode, ImGuiDir_Right, fractions.opacityMixer, &bottomRightNode, &bottomNode);
  ImGui::DockBuilderSplitNode(rightNode, ImGuiDir_Down, 1.0f / 3.0f, &rightBottomNode, &rightNode);
  ImGui::DockBuilderSplitNode(rightNode, ImGuiDir_Down, 0.5f, &rightMiddleNode, &rightNode);

  ImGui::DockBuilderDockWindow("Images##Images", leftNode);
  ImGui::DockBuilderDockWindow("Segmentations##Segmentations", leftNode);

  ImGui::DockBuilderDockWindow("Annotations", rightNode);
  ImGui::DockBuilderDockWindow("Landmarks", rightMiddleNode);
  ImGui::DockBuilderDockWindow("Isosurfaces", rightBottomNode);

  ImGui::DockBuilderDockWindow("Voxel Inspector##InspectionWindow", bottomNode);
  ImGui::DockBuilderDockWindow("Image Opacity Mixer", bottomRightNode);

  ImGui::DockBuilderFinish(dockspaceId);
}

float buttonWidthForLabel(const char* label)
{
  return ImGui::CalcTextSize(label, nullptr, true).x + 2.0f * ImGui::GetStyle().FramePadding.x;
}

std::unordered_map<ImGuiID, std::size_t> layoutIndicesByTabId(const std::vector<std::string>& labels)
{
  std::unordered_map<ImGuiID, std::size_t> indices;
  indices.reserve(labels.size());
  for (std::size_t index = 0; index < labels.size(); ++index) {
    indices.emplace(ImGui::GetID(labels.at(index).c_str()), index);
  }
  return indices;
}

std::vector<std::size_t> layoutOrderFromTabBar(const std::vector<std::string>& labels, const ImGuiTabBar& tabBar)
{
  const auto indicesByTabId = layoutIndicesByTabId(labels);
  std::vector<std::size_t> order;
  order.reserve(labels.size());
  for (int tabIndex = 0; tabIndex < tabBar.Tabs.Size; ++tabIndex) {
    const auto indexIt = indicesByTabId.find(tabBar.Tabs[tabIndex].ID);
    if (indexIt == indicesByTabId.end()) {
      continue;
    }
    order.emplace_back(indexIt->second);
  }
  return order;
}

std::optional<std::size_t> selectedLayoutIndexFromTabBar(
  const std::vector<std::string>& labels,
  const ImGuiTabBar& tabBar)
{
  const auto indicesByTabId = layoutIndicesByTabId(labels);
  const auto selectedIt = indicesByTabId.find(tabBar.SelectedTabId);
  if (selectedIt == indicesByTabId.end()) {
    return std::nullopt;
  }
  return selectedIt->second;
}

bool applyLayoutOrder(WindowData& windowData, const std::vector<std::size_t>& sourceOrder)
{
  if (sourceOrder.size() != windowData.numLayouts()) {
    return false;
  }

  std::vector<std::size_t> currentOrder(windowData.numLayouts());
  std::iota(currentOrder.begin(), currentOrder.end(), std::size_t{0});
  if (sourceOrder == currentOrder) {
    return false;
  }

  for (std::size_t destinationIndex = 0; destinationIndex < sourceOrder.size(); ++destinationIndex) {
    if (currentOrder.at(destinationIndex) == sourceOrder.at(destinationIndex)) {
      continue;
    }

    auto sourceIt = std::find(
      currentOrder.begin() + static_cast<std::ptrdiff_t>(destinationIndex),
      currentOrder.end(),
      sourceOrder.at(destinationIndex));
    if (sourceIt == currentOrder.end()) {
      return false;
    }

    const std::size_t sourceIndex = static_cast<std::size_t>(std::distance(currentOrder.begin(), sourceIt));
    windowData.moveLayout(sourceIndex, destinationIndex);

    const std::size_t movedValue = *sourceIt;
    currentOrder.erase(sourceIt);
    currentOrder.insert(currentOrder.begin() + static_cast<std::ptrdiff_t>(destinationIndex), movedValue);
  }

  return true;
}

std::string truncateTabImageName(std::string name)
{
  constexpr std::size_t k_maxTabImageNameLength = 12;
  if (name.size() <= k_maxTabImageNameLength) {
    return name;
  }
  name.resize(k_maxTabImageNameLength);
  name += "...";
  return name;
}

std::string layoutImageDisplayName(const AppData& appData, const Layout& layout)
{
  if (layout.renderedImages().empty()) {
    return {};
  }

  const uuids::uuid& imageUid = layout.renderedImages().front();
  const Image* image = appData.image(imageUid);
  if (!image) {
    return {};
  }

  return image->settings().displayName();
}

std::string layoutImageTabName(const AppData& appData, const Layout& layout)
{
  if (layout.renderedImages().empty()) {
    return {};
  }

  const Image* image = appData.image(layout.renderedImages().front());
  return image ? truncateTabImageName(image->settings().displayName()) : std::string{};
}

GuiData::Margins marginsWithoutLayoutTabs(const GuiData& guiData)
{
  GuiData marginsData = guiData;
  marginsData.m_showLayoutTabs = false;
  return marginsData.computeMargins();
}

GuiData::LayoutTabPlacement guiLayoutTabPlacement(UiLayoutTabPlacement placement)
{
  return UiLayoutTabPlacement::Bottom == placement ? GuiData::LayoutTabPlacement::Bottom
                                                   : GuiData::LayoutTabPlacement::Top;
}

void syncLayoutTabGuiDataFromSettings(AppData& appData)
{
  appData.guiData().m_showLayoutTabs = appData.settings().showLayoutTabs();
  appData.guiData().m_layoutTabPlacement = guiLayoutTabPlacement(appData.settings().layoutTabPlacement());
}

std::string layoutTabBaseLabel(const Layout& layout, const std::string& displayName)
{
  switch (layout.kind()) {
    case LayoutKind::FourUp:
      return "4-Up";
    case LayoutKind::ThreeUp:
      return "3-Up";
    case LayoutKind::OneUp:
      return "1-Up";
    case LayoutKind::MultiImageGrid:
    case LayoutKind::AxCorSagByImage:
      return displayName;
    case LayoutKind::Lightbox:
      return "Lightbox";
    case LayoutKind::Custom:
      return displayName;
    case LayoutKind::NumElements:
      return displayName;
  }
  return displayName;
}

std::vector<std::string> layoutTabLabels(const AppData& appData)
{
  const WindowData& windowData = appData.windowData();
  std::vector<std::string> baseNames;
  baseNames.reserve(windowData.numLayouts());

  std::unordered_map<std::string, std::size_t> baseNameCounts;
  for (std::size_t index = 0; index < windowData.numLayouts(); ++index) {
    baseNames.emplace_back(layoutTabBaseLabel(windowData.layouts().at(index), windowData.layoutDisplayName(index)));
    ++baseNameCounts[baseNames.back()];
  }

  std::vector<std::string> labels;
  labels.reserve(windowData.numLayouts());
  std::unordered_map<std::string, std::size_t> seenBaseNameCounts;
  for (std::size_t index = 0; index < windowData.numLayouts(); ++index) {
    const Layout& layout = windowData.layouts().at(index);
    std::string label = baseNames.at(index);
    if (baseNameCounts.at(label) > 1) {
      if (LayoutKind::Lightbox == layout.kind()) {
        const std::string imageName = layoutImageTabName(appData, layout);
        if (!imageName.empty()) {
          label += " - " + imageName;
        }
      }
      if (label == baseNames.at(index)) {
        const std::size_t duplicateIndex = ++seenBaseNameCounts[label];
        label += " " + std::to_string(duplicateIndex);
      }
    }
    label += "##layout_tab_" + uuids::to_string(windowData.layouts().at(index).uid());
    labels.emplace_back(std::move(label));
  }
  return labels;
}

void requestLayoutRemoval(AppData& appData, std::size_t index)
{
  WindowData& windowData = appData.windowData();
  if (index >= windowData.numLayouts() || windowData.numLayouts() <= 1) {
    return;
  }

  appData.guiData().m_pendingRemoveLayoutIndex = index;
  appData.guiData().m_showConfirmRemoveLayoutPopup = true;
}

void removeLayout(AppData& appData, std::size_t index)
{
  WindowData& windowData = appData.windowData();
  if (index >= windowData.numLayouts() || windowData.numLayouts() <= 1) {
    return;
  }

  if (index == windowData.currentLayoutIndex()) {
    windowData.setCurrentLayoutIndex(index > 0 ? index - 1 : 1);
  }
  windowData.removeLayout(index);
}

void renderConfirmRemoveLayoutPopup(AppData& appData)
{
  constexpr const char* popupTitle = "Remove Layout?";
  GuiData& guiData = appData.guiData();

  const std::optional<std::size_t> pendingIndex = guiData.m_pendingRemoveLayoutIndex;
  const bool validIndex = pendingIndex && *pendingIndex < appData.windowData().numLayouts();
  const std::string layoutName =
    validIndex ? appData.windowData().layoutDisplayName(*pendingIndex) : std::string{"this layout"};
  const bool canRemove = validIndex && appData.windowData().numLayouts() > 1;

  if (guiData.m_showConfirmRemoveLayoutPopup && !ImGui::IsPopupOpen(popupTitle)) {
    const auto result = native_dialog::showMessageDialog(
      {popupTitle,
       "Remove '" + layoutName + "'?",
       canRemove ? "This removes the layout from the current project. Image data and files are not deleted."
                 : "At least one layout must remain.",
       "Remove",
       "Cancel",
       ""});
    if (result) {
      if (native_dialog::MessageDialogResult::FirstButton == *result && canRemove) {
        removeLayout(appData, *pendingIndex);
      }
      guiData.m_pendingRemoveLayoutIndex = std::nullopt;
      guiData.m_showConfirmRemoveLayoutPopup = false;
      return;
    }
  }

  if (guiData.m_showConfirmRemoveLayoutPopup && !ImGui::IsPopupOpen(popupTitle)) {
    ImGui::OpenPopup(popupTitle, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize);
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});

  if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Remove '%s'?", layoutName.c_str());
    ImGui::Spacing();
    ImGui::TextWrapped("This removes the layout from the current project. Image data and files are not deleted.");
    ImGui::Separator();

    if (!canRemove) {
      ImGui::TextUnformatted("At least one layout must remain.");
      ImGui::Spacing();
    }

    if (ImGui::Button("Remove") && canRemove) {
      removeLayout(appData, *pendingIndex);
      guiData.m_pendingRemoveLayoutIndex = std::nullopt;
      guiData.m_showConfirmRemoveLayoutPopup = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      guiData.m_pendingRemoveLayoutIndex = std::nullopt;
      guiData.m_showConfirmRemoveLayoutPopup = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  if (!ImGui::IsPopupOpen(popupTitle)) {
    guiData.m_showConfirmRemoveLayoutPopup = false;
    guiData.m_pendingRemoveLayoutIndex = std::nullopt;
  }
}

void renderLayoutTabs(AppData& appData)
{
  GuiData& guiData = appData.guiData();
  WindowData& windowData = appData.windowData();
  if (!guiData.m_showLayoutTabs || 0 == windowData.numLayouts()) {
    return;
  }

  static std::optional<uuids::uuid> lastSyncedSelectedLayoutUid;

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (!viewport) {
    return;
  }

  const bool placeAtTop = GuiData::LayoutTabPlacement::Top == guiData.m_layoutTabPlacement;
  const LayoutTabMetrics metrics = layoutTabMetrics();
  guiData.m_layoutTabBarHeight = metrics.windowHeight;
  guiData.m_layoutTabInnerGap = metrics.innerGap;
  const GuiData::Margins toolbarMargins = guiData.computeToolbarMargins();
  const GuiData::Margins chromeMargins = marginsWithoutLayoutTabs(guiData);
  const float top = chromeMargins.top - toolbarMargins.top;
  const float bottom = chromeMargins.bottom - toolbarMargins.bottom;
  const float y =
    placeAtTop ? viewport->Pos.y + top : viewport->Pos.y + viewport->Size.y - bottom - metrics.windowHeight;

  ImGui::SetNextWindowPos(ImVec2{viewport->Pos.x, y}, ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2{viewport->Size.x, metrics.windowHeight}, ImGuiCond_Always);
  ImGui::SetNextWindowViewport(viewport->ID);

  constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                                           ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav |
                                           ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoDocking;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, metrics.windowPadding);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, metrics.frameRounding);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2{0.0f, 0.0f});
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));

  if (ImGui::Begin("LayoutTabs", nullptr, windowFlags)) {
    const std::vector<std::string> labels = layoutTabLabels(appData);
    const std::size_t currentLayoutIndex = windowData.currentLayoutIndex();
    const uuids::uuid currentLayoutUid = windowData.layouts().at(currentLayoutIndex).uid();
    const bool forceCurrentTabSelected =
      !lastSyncedSelectedLayoutUid || currentLayoutUid != *lastSyncedSelectedLayoutUid;

    std::optional<std::size_t> requestedLayoutIndex;

    const ImGuiTabBarFlags tabBarFlags = ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_TabListPopupButton |
                                         ImGuiTabBarFlags_DrawSelectedOverline | ImGuiTabBarFlags_FittingPolicyScroll |
                                         ImGuiTabBarFlags_NoCloseWithMiddleMouseButton;
    if (ImGui::BeginTabBar("##LayoutTabStrip", tabBarFlags)) {
      std::optional<std::size_t> pendingRemoveLayoutIndex;
      for (std::size_t index = 0; index < labels.size(); ++index) {
        const bool selected = currentLayoutIndex == index;
        ImGuiTabItemFlags tabFlags = ImGuiTabItemFlags_NoAssumedClosure;
        if (selected && forceCurrentTabSelected) {
          tabFlags |= ImGuiTabItemFlags_SetSelected;
        }

        bool tabOpen = true;
        bool* tabOpenPtr = windowData.numLayouts() > 1 ? &tabOpen : nullptr;
        const bool tabSelected = ImGui::BeginTabItem(labels.at(index).c_str(), tabOpenPtr, tabFlags);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
          requestedLayoutIndex = index;
        }
        if (!tabOpen) {
          pendingRemoveLayoutIndex = index;
        }
        if (tabSelected) {
          ImGui::EndTabItem();
        }
      }

      if (ImGui::TabItemButton("+##AddLayoutTab", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
        guiData.m_showAddLayoutPopup = true;
      }

      const bool layoutOrderChanged =
        !pendingRemoveLayoutIndex && ImGui::GetCurrentTabBar() &&
        applyLayoutOrder(windowData, layoutOrderFromTabBar(labels, *ImGui::GetCurrentTabBar()));
      if (!layoutOrderChanged && !pendingRemoveLayoutIndex && !forceCurrentTabSelected) {
        if (const ImGuiTabBar* tabBar = ImGui::GetCurrentTabBar()) {
          const auto selectedLayoutIndex = selectedLayoutIndexFromTabBar(labels, *tabBar);
          if (
            selectedLayoutIndex && *selectedLayoutIndex < windowData.numLayouts() &&
            *selectedLayoutIndex != windowData.currentLayoutIndex())
          {
            requestedLayoutIndex = *selectedLayoutIndex;
          }
        }
      }

      ImGui::EndTabBar();

      if (pendingRemoveLayoutIndex) {
        requestLayoutRemoval(appData, *pendingRemoveLayoutIndex);
      }
      if (layoutOrderChanged) {
        requestedLayoutIndex = std::nullopt;
      }
    }

    if (requestedLayoutIndex && *requestedLayoutIndex < windowData.numLayouts()) {
      windowData.setCurrentLayoutIndex(*requestedLayoutIndex);
    }
    lastSyncedSelectedLayoutUid = windowData.layouts().at(windowData.currentLayoutIndex()).uid();
  }

  ImGui::End();
  ImGui::PopStyleColor(1);
  ImGui::PopStyleVar(5);
}

std::string loadingItemLabel(const GuiData::LoadingStatusItem& item)
{
  std::string label = item.fileName.filename().string();
  if (label.empty()) {
    label = item.fileName.string();
  }

  if (item.bytes) {
    const double mib = static_cast<double>(*item.bytes) / (1024.0 * 1024.0);
    const auto roundedMiB = static_cast<std::uintmax_t>(std::max(1.0, std::round(mib)));
    label += " (" + std::to_string(roundedMiB) + " MiB)";
  }

  return label;
}

void renderLoadingStatusWindow(const GuiData& guiData)
{
  if (!guiData.m_loadingStatus) {
    return;
  }

  std::string title;
  std::vector<GuiData::LoadingStatusItem> items;
  {
    std::scoped_lock lock(guiData.m_loadingStatus->mutex);
    if (!guiData.m_loadingStatus->visible) {
      return;
    }
    title = guiData.m_loadingStatus->title;
    items = guiData.m_loadingStatus->items;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float margin = scaledPixel(12.0f);
  const ImVec2 pos{viewport->WorkPos.x + margin, viewport->WorkPos.y + viewport->WorkSize.y - margin};

  ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2{0.0f, 1.0f});

  constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                                     ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{scaledPixel(12.0f), scaledPixel(10.0f)});

  if (ImGui::Begin("LoadingStatus", nullptr, flags)) {
    const int dotCount = static_cast<int>(ImGui::GetTime() * 3.0) % 4;
    std::string text = title.empty() ? "Loading images" : title;
    text.append(static_cast<std::size_t>(dotCount), '.');
    ImGui::TextUnformatted(text.c_str());

    if (!items.empty()) {
      ImGui::Separator();
      for (const auto& item : items) {
        if (item.loaded) {
          ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), ICON_FK_CHECK);
        }
        else {
          ImGui::TextDisabled("-");
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(loadingItemLabel(item).c_str());
      }
    }
  }

  ImGui::End();
  ImGui::PopStyleVar();
}

void refreshTimeSeriesTexture(AppData& appData, const uuids::uuid& imageUid)
{
  appData.renderData().m_imageTextures.erase(imageUid);
  createImageTextures(appData, std::vector<uuids::uuid>{imageUid});
}

void setTimePoint(AppData& appData, const uuids::uuid& imageUid, Image& image, uint32_t timePoint)
{
  const uint32_t clamped = image.timeAxis().clamp(timePoint);
  if (image.settings().activeTimePoint() == clamped) {
    return;
  }
  image.settings().setActiveTimePoint(clamped);
  refreshTimeSeriesTexture(appData, imageUid);
}

void setTimePointWithSynchronization(AppData& appData, const uuids::uuid& imageUid, Image& image, uint32_t timePoint)
{
  setTimePoint(appData, imageUid, image, timePoint);
  if (!appData.settings().synchronizeTimeSeries()) {
    return;
  }

  for (const uuids::uuid& otherUid : appData.imageUidsOrdered()) {
    if (otherUid == imageUid) {
      continue;
    }
    Image* other = appData.image(otherUid);
    if (other && other->isTimeSeries()) {
      setTimePoint(appData, otherUid, *other, timePoint);
    }
  }
}

void stopOtherTimeSeriesPlayback(AppData& appData, const uuids::uuid& playingImageUid)
{
  for (const uuids::uuid& otherUid : appData.imageUidsOrdered()) {
    if (otherUid == playingImageUid) {
      continue;
    }
    Image* other = appData.image(otherUid);
    if (other && other->isTimeSeries()) {
      other->settings().setTimePlaybackPlaying(false);
    }
  }
}

std::string timePointControlLabel(const Image& image, uint32_t timePoint)
{
  std::ostringstream os;
  os << timePoint << " of " << (image.timeAxis().numTimePoints() - 1u);
  if (const auto value = image.timeAxis().value(timePoint)) {
    os << " (" << std::fixed << std::setprecision(2) << *value << " " << image.timeAxis().units() << ")";
  }
  return os.str();
}

float timePointControlLabelWidth(const Image& image)
{
  const uint32_t lastTimePoint = image.timeAxis().numTimePoints() - 1u;
  return std::max(
    ImGui::CalcTextSize(timePointControlLabel(image, 0u).c_str()).x,
    ImGui::CalcTextSize(timePointControlLabel(image, lastTimePoint).c_str()).x);
}

void renderReservedWidthText(const char* text, float reservedWidth, ImU32 color)
{
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  const float height = ImGui::GetFrameHeight();
  const float offsetY = 0.5f * (height - ImGui::GetTextLineHeight());
  ImGui::Dummy(ImVec2{reservedWidth, height});
  ImGui::GetWindowDrawList()->AddText(ImVec2{pos.x, pos.y + offsetY}, color, text);
}

void renderReservedWidthText(const char* text, float reservedWidth)
{
  renderReservedWidthText(text, reservedWidth, ImGui::GetColorU32(ImGuiCol_Text));
}

std::string playbackRateLabel(const Image& image)
{
  const double speed = std::min(
    image.settings().timePlaybackSpeed(),
    image.timeAxis().maxPlaybackSpeedForFrameRate(k_maxTimePlaybackFramesPerSecond));
  const double framePeriod = image.timeAxis().playbackFramePeriodSeconds(speed);
  const double framesPerSecond = playbackFramesPerSecond(framePeriod);
  const char* frameWord = std::abs(framesPerSecond - 1.0) < 1.0e-6 ? "frame" : "frames";
  std::ostringstream os;
  os << std::fixed << std::setprecision(1) << speed << "x (" << framesPerSecond << ' ' << frameWord << "/sec)";
  return os.str();
}

std::optional<uuids::uuid> globalTimeControlImageUid(AppData& appData)
{
  if (const auto activeUid = appData.activeImageUid()) {
    if (const Image* activeImage = appData.image(*activeUid); activeImage && activeImage->isTimeSeries()) {
      return activeUid;
    }
  }

  for (const uuids::uuid& imageUid : appData.imageUidsOrdered()) {
    if (const Image* image = appData.image(imageUid); image && image->isTimeSeries()) {
      return imageUid;
    }
  }
  return std::nullopt;
}

void updateTimePlayback(AppData& appData, const uuids::uuid& imageUid, Image& image, uint32_t activeTimePoint)
{
  static std::unordered_map<uuids::uuid, TimePlaybackState> s_playbackStateByImage;

  if (!image.settings().timePlaybackPlaying()) {
    s_playbackStateByImage.erase(imageUid);
    return;
  }

  TimePlaybackState& state = s_playbackStateByImage[imageUid];
  const TimePlaybackUpdate update = updateTimePlaybackFrame(
    state,
    TimePlaybackInput{
      .playing = image.settings().timePlaybackPlaying(),
      .loop = image.settings().timePlaybackLoop(),
      .activeTimePoint = activeTimePoint,
      .numTimePoints = image.timeAxis().numTimePoints(),
      .framePeriodSeconds = image.timeAxis().playbackFramePeriodSeconds(image.settings().timePlaybackSpeed()),
      .nowSeconds = ImGui::GetTime()});
  if (update.playingChanged) {
    image.settings().setTimePlaybackPlaying(update.playing);
  }
  if (!update.playing) {
    s_playbackStateByImage.erase(imageUid);
  }
  if (!update.advanced) {
    return;
  }

  setTimePointWithSynchronization(appData, imageUid, image, update.timePoint);
}

bool anyTimeSeriesPlaybackRunning(const AppData& appData)
{
  for (const uuids::uuid& imageUid : appData.imageUidsOrdered()) {
    const Image* image = appData.image(imageUid);
    if (image && image->isTimeSeries() && image->settings().timePlaybackPlaying()) {
      return true;
    }
  }
  return false;
}

void updateAllTimeSeriesPlayback(AppData& appData)
{
  std::optional<uuids::uuid> playbackDriverUid;
  for (const uuids::uuid& imageUid : appData.imageUidsOrdered()) {
    Image* image = appData.image(imageUid);
    if (!image || !image->isTimeSeries()) {
      continue;
    }

    if (image->settings().timePlaybackPlaying()) {
      if (playbackDriverUid) {
        image->settings().setTimePlaybackPlaying(false);
        updateTimePlayback(appData, imageUid, *image, image->timeAxis().clamp(image->settings().activeTimePoint()));
        continue;
      }
      playbackDriverUid = imageUid;
    }

    updateTimePlayback(appData, imageUid, *image, image->timeAxis().clamp(image->settings().activeTimePoint()));
  }
}

void renderGlobalTimeControl(AppData& appData)
{
  static bool s_timePlaybackWasRunning = false;
  auto updatePlaybackAnimationState = [&appData]() {
    const bool playbackRunning = anyTimeSeriesPlaybackRunning(appData);
    if (playbackRunning) {
      appData.state().setAnimating(true);
    }
    else if (s_timePlaybackWasRunning) {
      appData.state().setAnimating(false);
    }
    s_timePlaybackWasRunning = playbackRunning;
  };

  updateAllTimeSeriesPlayback(appData);

  const auto imageUid = globalTimeControlImageUid(appData);
  if (!imageUid) {
    if (s_timePlaybackWasRunning) {
      appData.state().setAnimating(false);
      s_timePlaybackWasRunning = false;
    }
    return;
  }

  Image* image = appData.image(*imageUid);
  if (!image || !image->isTimeSeries()) {
    if (s_timePlaybackWasRunning) {
      appData.state().setAnimating(false);
      s_timePlaybackWasRunning = false;
    }
    return;
  }

  const uint32_t maxTimePoint = image->timeAxis().numTimePoints() - 1u;
  const uint32_t activeTimePoint = image->timeAxis().clamp(image->settings().activeTimePoint());
  uint32_t requestedTimePoint = activeTimePoint;

  if (!appData.settings().showGlobalTimeControls()) {
    updatePlaybackAnimationState();
    return;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  if (!viewport) {
    updatePlaybackAnimationState();
    return;
  }

  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoDocking;
  ImGui::SetNextWindowViewport(viewport->ID);
  ImGui::SetNextWindowPos(
    ImVec2{viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y - 12.0f},
    ImGuiCond_Always,
    ImVec2{0.5f, 1.0f});

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{8.0f, 6.0f});
  if (ImGui::Begin("GlobalTimeControl", nullptr, flags)) {
    const float timeLabelWidth = timePointControlLabelWidth(*image);
    const std::string timeLabel = timePointControlLabel(*image, activeTimePoint);
    const std::string rateLabel = playbackRateLabel(*image);
    const float rateLabelWidth = ImGui::CalcTextSize(rateLabel.c_str()).x;
    const ImVec2 buttonSize{ImGui::GetFrameHeight(), ImGui::GetFrameHeight()};

    bool playing = image->settings().timePlaybackPlaying();
    const std::string playbackButtonLabel =
      std::string{playing ? ICON_FK_PAUSE : ICON_FK_PLAY} + "##globalToggleTimePlayback";
    if (ImGui::Button(playbackButtonLabel.c_str(), buttonSize)) {
      playing = !playing;
      image->settings().setTimePlaybackPlaying(playing);
      if (playing) {
        stopOtherTimeSeriesPlayback(appData, *imageUid);
      }
      appData.state().setAnimating(playing || anyTimeSeriesPlaybackRunning(appData));
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(playing ? "Pause time-series playback" : "Play time series");
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_FAST_BACKWARD "##globalFirstTimePoint", buttonSize)) {
      requestedTimePoint = 0u;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Go to first time frame");
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_STEP_BACKWARD "##globalPreviousTimePoint", buttonSize)) {
      requestedTimePoint =
        activeTimePoint == 0u ? (image->settings().timePlaybackLoop() ? maxTimePoint : 0u) : activeTimePoint - 1u;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Go to previous time frame");
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_STEP_FORWARD "##globalNextTimePoint", buttonSize)) {
      requestedTimePoint = activeTimePoint == maxTimePoint ? (image->settings().timePlaybackLoop() ? 0u : maxTimePoint)
                                                           : activeTimePoint + 1u;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Go to next time frame");
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_FAST_FORWARD "##globalLastTimePoint", buttonSize)) {
      requestedTimePoint = maxTimePoint;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Go to last time frame");
    }
    ImGui::SameLine();
    renderReservedWidthText(timeLabel.c_str(), timeLabelWidth);
    ImGui::SameLine();
    renderReservedWidthText(rateLabel.c_str(), rateLabelWidth, ImGui::GetColorU32(ImGuiCol_TextDisabled));
    ImGui::SameLine();
    bool loop = image->settings().timePlaybackLoop();
    if (ImGui::Checkbox("Loop##globalTimePlaybackLoop", &loop)) {
      image->settings().setTimePlaybackLoop(loop);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Loop playback from the last frame back to the first frame");
    }

    if (requestedTimePoint != activeTimePoint) {
      setTimePointWithSynchronization(appData, *imageUid, *image, requestedTimePoint);
    }
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
  updatePlaybackAnimationState();
}

void renderEmptyWorkspace(
  ProjectLoadState projectLoadState,
  const std::function<void(const std::vector<fs::path>& fileNames)>& openImageFiles,
  const std::function<void(const std::vector<fs::path>& folderNames)>& openDicomFolders,
  const std::function<void()>& requestDicomFolderPathDialog,
  const std::function<void(const fs::path& fileName)>& openProjectFile)
{
  if (ProjectLoadState::Empty != projectLoadState && ProjectLoadState::Failed != projectLoadState) {
    return;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const ImGuiStyle& style = ImGui::GetStyle();
  const float panelMargin = scaledPixel(16.0f);
  const ImVec2 windowPadding{panelMargin, scaledPixel(20.0f)};
  const char* text = ProjectLoadState::Failed == projectLoadState ? "Project failed to load" : "No project loaded.";
  constexpr std::array<const char*, 3> buttonLabels{"Open Image(s)...", "Open DICOM Series...", "Open Project..."};

  float buttonWidth = 0.0f;
  for (const char* label : buttonLabels) {
    buttonWidth = std::max(buttonWidth, buttonWidthForLabel(label));
  }

  const float buttonsWidth = 3.0f * buttonWidth + 2.0f * style.ItemSpacing.x;
  const float contentWidth = std::max(ImGui::CalcTextSize(text).x, buttonsWidth);
  const float contentHeight = ImGui::GetTextLineHeight() + style.ItemSpacing.y + ImGui::GetFrameHeight();
  const ImVec2 panelSize{contentWidth + 2.0f * windowPadding.x, contentHeight + 2.0f * windowPadding.y};

  ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2{0.5f, 0.5f});
  ImGui::SetNextWindowSize(panelSize, ImGuiCond_Always);

  constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, windowPadding);
  if (ImGui::Begin("EmptyWorkspace", nullptr, flags)) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (contentWidth - ImGui::CalcTextSize(text).x) * 0.5f));
    ImGui::TextUnformatted(text);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + style.ItemSpacing.y);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (contentWidth - buttonsWidth) * 0.5f));

    if (ImGui::Button("Open Image(s)...", ImVec2{buttonWidth, 0.0f})) {
      const auto selectedFiles = native_dialog::openFiles(native_dialog::imageFilters());
      if (!selectedFiles.empty() && openImageFiles) {
        openImageFiles(selectedFiles);
      }
    }

    ImGui::SameLine();

    if (ImGui::Button("Open DICOM Series...", ImVec2{buttonWidth, 0.0f})) {
      const auto selectedFolders = native_dialog::pickFolders();
      if (!selectedFolders.empty() && openDicomFolders) {
        openDicomFolders(selectedFolders);
      }
      else if (requestDicomFolderPathDialog) {
        requestDicomFolderPathDialog();
      }
    }

    ImGui::SameLine();

    if (ImGui::Button("Open Project...", ImVec2{buttonWidth, 0.0f})) {
      if (const auto selectedFile = native_dialog::openFile(native_dialog::projectFilters())) {
        if (openProjectFile) {
          openProjectFile(*selectedFile);
        }
      }
    }
  }
  ImGui::End();
  ImGui::PopStyleVar();
}

ImFont* loadFont(
  const std::string& fontPath,
  const ImFontConfig& fontConfig,
  float fontSize,
  const ImWchar* glyphRange = nullptr)
{
  auto filesystem = cmrc::fonts::get_filesystem();
  cmrc::file fontFile = filesystem.open(fontPath);

  auto fontData = std::make_unique<char[]>(fontFile.size());
  std::copy(fontFile.cbegin(), fontFile.cend(), fontData.get());

  ImFontConfig config = fontConfig;
  config.FontDataOwnedByAtlas = true;

  ImFont* font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
    static_cast<void*>(fontData.get()),
    static_cast<int32_t>(fontFile.size()),
    fontSize,
    &config,
    glyphRange);
  if (font) {
    fontData.release();
  }

  return font;
}

struct UiFontSpec
{
  const char* name;
  const char* path;
  float size;
};

/**
 * @brief Return the embedded font resource and base size for an ImGui UI font family.
 */
UiFontSpec uiFontSpec(UiFontFamily family)
{
  switch (family) {
    case UiFontFamily::SpaceGrotesk:
      return {"Space Grotesk Light", "res/fonts/SpaceGrotesk/SpaceGrotesk-Light.ttf", 16.0f};
    case UiFontFamily::Inter:
      return {"Inter Regular", "res/fonts/Inter/Inter-Regular.ttf", 16.0f};
    case UiFontFamily::NotoSans:
      return {"Noto Sans Regular", "res/fonts/NotoSans/NotoSans-Regular.ttf", 16.0f};
    case UiFontFamily::Roboto:
      return {"Roboto Regular", "res/fonts/Roboto/Roboto-Regular.ttf", 16.0f};
    case UiFontFamily::SourceSans3:
      return {"Source Sans 3 Regular", "res/fonts/SourceSans3/SourceSans3-Regular.ttf", 16.0f};
    case UiFontFamily::IBMPlexSans:
      return {"IBM Plex Sans Regular", "res/fonts/IBMPlexSans/IBMPlexSans-Regular.ttf", 16.0f};
    case UiFontFamily::Cousine:
      return {"Cousine Regular", "res/fonts/Cousine/Cousine-Regular.ttf", 14.0f};
  }

  return {"Space Grotesk Light", "res/fonts/SpaceGrotesk/SpaceGrotesk-Light.ttf", 16.0f};
}

std::string componentProjectionTaskKey(const uuids::uuid& imageUid, ComponentProjectionMode mode, uint32_t timePoint)
{
  return uuids::to_string(imageUid) + ":" + std::to_string(static_cast<int>(mode)) + ":" + std::to_string(timePoint);
}

} // namespace

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
  io.IniFilename = m_iniFileName.c_str();
  io.LogFilename = m_logFileName.c_str();

  io.ConfigDragClickToInputText = true;
  //    io.MouseDrawCursor = true;
  /// @todo Add window option for unsaved document (a little dot) when project changes

  io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  // Setup ImGui platform/renderer bindings:
  static const char* glsl_version = "#version 150";
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
  m_loadDeformationField = std::move(callbacks.project.loadDeformationField);
  m_openProjectFile = std::move(callbacks.project.openProjectFile);
  m_largeImageLoadDecision = std::move(callbacks.project.largeImageLoadDecision);
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

bool ImGuiWrapper::materializeRegistrationInputs(registration::JobSpec& job)
{
  auto parseUid = [](const registration::DataRef& ref) -> std::optional<uuids::uuid> {
    if (ref.uid.empty()) {
      return std::nullopt;
    }
    return uuids::uuid::from_string(ref.uid);
  };

  auto exportRef = [&](registration::DataRef& ref, const std::filesystem::path& path) -> bool {
    if (!ref.fileName.empty()) {
      return true;
    }

    const std::optional<uuids::uuid> uid = parseUid(ref);
    if (!uid) {
      spdlog::error("Cannot export registration input '{}'; it has no valid UID", ref.displayName);
      return false;
    }

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
      spdlog::error("Cannot create registration input directory {}: {}", path.parent_path(), error.message());
      return false;
    }

    Image* image = nullptr;
    switch (ref.source) {
      case registration::DataSource::LoadedImage:
        image = m_appData.image(*uid);
        break;
      case registration::DataSource::Segmentation:
        image = m_appData.seg(*uid);
        break;
      case registration::DataSource::None:
      case registration::DataSource::LandmarkGroup:
      case registration::DataSource::AnnotationSet:
      case registration::DataSource::Surface:
      case registration::DataSource::ExternalFile:
        break;
    }

    if (!image) {
      spdlog::error(
        "Cannot export registration input '{}' with UID {}; unsupported source type or object is missing",
        ref.displayName,
        ref.uid);
      return false;
    }

    if (!image->saveComponentToDisk(0, path)) {
      spdlog::error("Cannot export registration input '{}' to {}", ref.displayName, path);
      return false;
    }

    ref.fileName = path;
    spdlog::info("Exported registration input '{}' to {}", ref.displayName, path);
    return true;
  };

  auto exportArtifact = [&](const registration::InputArtifact& artifact) -> bool {
    switch (artifact.role) {
      case registration::ArtifactRole::FixedMask:
        return exportRef(job.fixedMask, artifact.path);
      case registration::ArtifactRole::MovingMask:
        return exportRef(job.movingMask, artifact.path);
      case registration::ArtifactRole::AuxiliaryFixedImage:
      case registration::ArtifactRole::AuxiliaryMovingImage: {
        for (registration::AuxiliaryImagePair& pair : job.auxiliaryImagePairs) {
          if (pair.fixed.uid == artifact.source.uid && artifact.role == registration::ArtifactRole::AuxiliaryFixedImage)
          {
            return exportRef(pair.fixed, artifact.path);
          }
          if (
            pair.moving.uid == artifact.source.uid && artifact.role == registration::ArtifactRole::AuxiliaryMovingImage)
          {
            return exportRef(pair.moving, artifact.path);
          }
        }
        break;
      }
      case registration::ArtifactRole::FixedLandmarks:
      case registration::ArtifactRole::MovingLandmarks:
      case registration::ArtifactRole::Surface:
        spdlog::error(
          "Registration input export for {} is not implemented yet; provide a file-backed input for now",
          registration::label(artifact.role));
        return false;
      case registration::ArtifactRole::JobSpec:
      case registration::ArtifactRole::ResultManifest:
      case registration::ArtifactRole::AffineTransform:
      case registration::ArtifactRole::InverseWarp:
      case registration::ArtifactRole::ForwardWarp:
      case registration::ArtifactRole::WarpedImage:
      case registration::ArtifactRole::WarpedSegmentation:
      case registration::ArtifactRole::TransformedLandmarks:
      case registration::ArtifactRole::TransformedSurface:
        break;
    }

    spdlog::error("Unable to match registration input artifact {}", registration::label(artifact.role));
    return false;
  };

  if (job.useCurrentAffineTransformsForInitialization) {
    const std::optional<uuids::uuid> movingUid = parseUid(job.movingImage);
    const Image* movingImage = movingUid ? m_appData.image(*movingUid) : nullptr;
    if (!movingImage) {
      spdlog::error("Cannot export current affine initialization; moving image is missing");
      return false;
    }

    std::error_code error;
    std::filesystem::create_directories(job.outputDirectory, error);
    if (error) {
      spdlog::error("Cannot create registration output directory {}: {}", job.outputDirectory, error.message());
      return false;
    }

    const glm::dmat4 currentDisplayAffine{movingImage->transformations().worldDef_T_subject()};
    const glm::dmat4 currentSamplingAffine = glm::inverse(currentDisplayAffine);
    job.initialAffineTransform = job.outputDirectory / (registration::outputPrefix(job) + "_initial_affine.mat");
    const bool saved =
      job.backend == registration::Backend::Greedy
        ? registration::writeGreedyAffineTransform(job.initialAffineTransform, currentSamplingAffine)
        : registration::writeItkAffineTransform(job.initialAffineTransform, currentSamplingAffine, job.dimension);
    if (!saved) {
      spdlog::error("Cannot save current affine initialization to {}", job.initialAffineTransform);
      job.initialAffineTransform.clear();
      return false;
    }
    job.useImageCentersForInitialization = false;
    spdlog::info("Exported registration initial affine transform to {}", job.initialAffineTransform);
  }

  for (const registration::InputArtifact& artifact : registration::buildInputArtifactPlan(job)) {
    if (artifact.exportRequired && !exportArtifact(artifact)) {
      return false;
    }
  }

  return true;
}

void ImGuiWrapper::importRegistrationJobOutputs(const std::string& jobId)
{
  registration::JobStore& jobs = m_appData.registrationJobs();
  const registration::JobRecord* job = jobs.find(jobId);
  if (!job || !job->manifest) {
    return;
  }

  const registration::JobSpec spec = job->spec;
  const registration::ResultManifest manifest = *job->manifest;
  const registration::ImportPlan plan = registration::buildImportPlan(spec, manifest);

  auto appendEvent = [&jobs, &jobId](registration::ProgressEventKind kind, std::string message) {
    jobs.appendProgress(jobId, registration::ProgressEvent{.kind = kind, .message = std::move(message)});
  };

  auto parseTargetImageUid = [&appendEvent](const registration::ImportStep& step) -> std::optional<uuids::uuid> {
    if (step.targetImageUid.empty()) {
      appendEvent(registration::ProgressEventKind::Warning, "Registration import step has no target image UID.");
      return std::nullopt;
    }
    std::optional<uuids::uuid> uid = uuids::uuid::from_string(step.targetImageUid);
    if (!uid) {
      appendEvent(
        registration::ProgressEventKind::Warning,
        "Registration import step has an invalid target image UID: " + step.targetImageUid);
    }
    return uid;
  };

  jobs.setStatus(jobId, registration::JobStatus::ImportingOutputs);
  appendEvent(registration::ProgressEventKind::Started, "Importing registration outputs.");

  for (const std::string& warning : plan.warnings) {
    appendEvent(registration::ProgressEventKind::Warning, warning);
  }

  bool hadError = false;
  for (const registration::ImportStep& step : plan.steps) {
    try {
      if (!step.path.empty() && !fs::exists(step.path)) {
        appendEvent(
          registration::ProgressEventKind::Warning,
          "Registration output does not exist and was not imported: " + step.path.string());
        continue;
      }

      switch (step.action) {
        case registration::ImportAction::ApplyAffineTransform: {
          const std::optional<uuids::uuid> imageUid = parseTargetImageUid(step);
          if (!imageUid) {
            break;
          }
          if (applyImportedAffineToImage(m_appData, *imageUid, step.path, spec.backend)) {
            appendEvent(registration::ProgressEventKind::Artifact, "Applied affine transform: " + step.path.string());
            if (m_updateImageUniforms) {
              m_updateImageUniforms(*imageUid);
            }
          }
          else {
            appendEvent(
              registration::ProgressEventKind::Warning,
              "Unable to parse or apply affine transform: " + step.path.string());
          }
          break;
        }
        case registration::ImportAction::LoadInverseWarp: {
          const std::optional<uuids::uuid> imageUid = parseTargetImageUid(step);
          if (!imageUid || !m_loadDeformationField) {
            break;
          }
          const std::optional<uuids::uuid> warpUid = m_loadDeformationField(step.path);
          if (warpUid && m_appData.assignInverseWarpUidToImage(*imageUid, *warpUid)) {
            appendEvent(registration::ProgressEventKind::Artifact, "Imported inverse warp: " + step.path.string());
            if (m_updateImageUniforms) {
              m_updateImageUniforms(*imageUid);
            }
          }
          else {
            appendEvent(
              registration::ProgressEventKind::Warning,
              "Unable to import inverse warp: " + step.path.string());
          }
          break;
        }
        case registration::ImportAction::LoadForwardWarp: {
          const std::optional<uuids::uuid> imageUid = parseTargetImageUid(step);
          if (!imageUid || !m_loadDeformationField) {
            break;
          }
          const std::optional<uuids::uuid> warpUid = m_loadDeformationField(step.path);
          if (warpUid && m_appData.assignForwardWarpUidToImage(*imageUid, *warpUid)) {
            appendEvent(registration::ProgressEventKind::Artifact, "Imported forward warp: " + step.path.string());
          }
          else {
            appendEvent(
              registration::ProgressEventKind::Warning,
              "Unable to import forward warp: " + step.path.string());
          }
          break;
        }
        case registration::ImportAction::AssignWarpsToMovingImage:
          appendEvent(registration::ProgressEventKind::Progress, "Assigned imported warps to the moving image.");
          break;
        case registration::ImportAction::LoadWarpedImage:
          if (m_addImageFiles) {
            m_addImageFiles({step.path});
            appendEvent(registration::ProgressEventKind::Artifact, "Queued warped image import: " + step.path.string());
          }
          else {
            appendEvent(registration::ProgressEventKind::Warning, "No image import callback is available.");
          }
          break;
        case registration::ImportAction::LoadWarpedSegmentation: {
          const std::optional<uuids::uuid> imageUid = parseTargetImageUid(step);
          if (imageUid && m_addSegmentationFileToImage) {
            m_addSegmentationFileToImage(*imageUid, step.path);
            appendEvent(
              registration::ProgressEventKind::Artifact,
              "Queued warped segmentation import: " + step.path.string());
          }
          else {
            appendEvent(registration::ProgressEventKind::Warning, "No segmentation import callback is available.");
          }
          break;
        }
        case registration::ImportAction::TransformLandmarksAndAnnotations:
          appendEvent(
            registration::ProgressEventKind::Warning,
            "Transformed landmark/annotation import is not wired to the app yet: " + step.path.string());
          break;
        case registration::ImportAction::LoadTransformedSurface:
          appendEvent(
            registration::ProgressEventKind::Warning,
            "Transformed surface import is not wired to the app yet: " + step.path.string());
          break;
        case registration::ImportAction::MakeWarpedImageActive:
          appendEvent(
            registration::ProgressEventKind::Warning,
            "The warped image was queued for loading, but cannot be made active until image loading reports its UID.");
          break;
      }
    }
    catch (const std::exception& e) {
      hadError = true;
      appendEvent(registration::ProgressEventKind::Failed, e.what());
      spdlog::error(
        "Exception while importing registration output for job {}: {}\n{}",
        jobId,
        e.what(),
        stack_trace::current(1));
      break;
    }
  }

  if (!hadError) {
    appendEvent(registration::ProgressEventKind::Completed, "Registration outputs imported.");
    jobs.setStatus(jobId, registration::JobStatus::Completed);
  }

  m_appData.setRainbowColorsForAllImages();
  m_appData.setRainbowColorsForAllLandmarkGroups();
  if (m_updateAllImageUniforms) {
    m_updateAllImageUniforms();
  }

  if (m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}

void ImGuiWrapper::storeFuture(const uuids::uuid& taskUid, std::future<AsyncTaskDetails> future)
{
  std::lock_guard<std::mutex> lock(m_futuresMutex);

  if (!future.valid()) {
    spdlog::warn("Future for task {} is not valid", taskUid);
    return;
  }

  m_futures.emplace(taskUid, std::move(future));

  spdlog::debug("Storing future for UI task {}. Total number of UI task futures: {}", taskUid, m_futures.size());
}

void ImGuiWrapper::requestComponentProjectionImage(const uuids::uuid& imageUid, ComponentProjectionMode mode)
{
  const Image* image = m_appData.image(imageUid);
  if (!image) {
    spdlog::warn("Cannot compute component projection for invalid image {}", imageUid);
    return;
  }

  const uint32_t timePoint = image->timeAxis().clamp(image->settings().activeTimePoint());
  if (m_appData.componentProjectionImageUid(imageUid, mode, timePoint)) {
    return;
  }

  const std::string taskKey = componentProjectionTaskKey(imageUid, mode, timePoint);
  {
    std::lock_guard<std::mutex> lock(m_componentProjectionFuturesMutex);
    if (m_pendingComponentProjectionKeys.contains(taskKey)) {
      return;
    }
    m_pendingComponentProjectionKeys.insert(taskKey);
  }

  const uuids::uuid taskUid = generateRandomUuid();
  Image imageCopy = *image;
  auto future = std::async(std::launch::async, [imageUid, mode, timePoint, imageCopy = std::move(imageCopy)]() mutable {
    return ComponentProjectionTaskResult{
      imageUid,
      mode,
      timePoint,
      createComponentProjectionImage(imageCopy, mode, timePoint)};
  });

  {
    std::lock_guard<std::mutex> lock(m_componentProjectionFuturesMutex);
    m_componentProjectionFutures.emplace(taskUid, std::move(future));
  }

  spdlog::debug(
    "Started {} component projection task {} for image {} frame {}",
    componentProjectionModeName(mode),
    taskUid,
    imageUid,
    timePoint);

  if (m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}

void ImGuiWrapper::requestMissingComponentProjectionImages()
{
  for (const auto& imageUid : m_appData.imageUidsOrdered()) {
    const Image* image = m_appData.image(imageUid);
    if (!image) {
      continue;
    }

    if (image->header().numComponentsPerPixel() < 2) {
      continue;
    }

    const auto mode = componentProjectionForImage(*image);
    const uint32_t timePoint = image->timeAxis().clamp(image->settings().activeTimePoint());
    if (mode && !m_appData.componentProjectionImageUid(imageUid, *mode, timePoint)) {
      requestComponentProjectionImage(imageUid, *mode);
    }
  }
}

void ImGuiWrapper::processComponentProjectionFutures()
{
  using namespace std::chrono_literals;

  std::vector<uuids::uuid> readyTasks;
  {
    std::lock_guard<std::mutex> lock(m_componentProjectionFuturesMutex);
    for (auto& [taskUid, future] : m_componentProjectionFutures) {
      if (future.valid() && std::future_status::ready == future.wait_for(0ms)) {
        readyTasks.push_back(taskUid);
      }
    }
  }

  for (const auto& taskUid : readyTasks) {
    std::future<ComponentProjectionTaskResult> future;
    {
      std::lock_guard<std::mutex> lock(m_componentProjectionFuturesMutex);
      auto it = m_componentProjectionFutures.find(taskUid);
      if (m_componentProjectionFutures.end() == it) {
        continue;
      }
      future = std::move(it->second);
      m_componentProjectionFutures.erase(it);
    }

    ComponentProjectionTaskResult result = future.get();
    {
      std::lock_guard<std::mutex> lock(m_componentProjectionFuturesMutex);
      m_pendingComponentProjectionKeys.erase(
        componentProjectionTaskKey(result.sourceImageUid, result.mode, result.timePoint));
    }

    if (!result.image) {
      spdlog::warn(
        "Unable to compute {} component projection for image {} frame {}: {}",
        componentProjectionModeName(result.mode),
        result.sourceImageUid,
        result.timePoint,
        result.image.error());
      continue;
    }

    const auto projectionUid = m_appData.setComponentProjectionImage(
      result.sourceImageUid,
      result.mode,
      result.timePoint,
      std::move(*result.image));
    if (!projectionUid) {
      spdlog::warn("Source image {} no longer exists for component projection", result.sourceImageUid);
      continue;
    }

    m_appData.renderData().m_imageTextures.erase(*projectionUid);
    createImageTextures(m_appData, std::vector<uuids::uuid>{*projectionUid});

    if (m_updateImageUniforms) {
      m_updateImageUniforms(*projectionUid);
    }

    spdlog::debug(
      "Finished {} component projection for image {}",
      componentProjectionModeName(result.mode),
      result.sourceImageUid);
  }

  if (!readyTasks.empty() && m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}

namespace
{
std::string
warpInversionTaskKey(const uuids::uuid& imageUid, const uuids::uuid& sourceWarpUid, ComputedWarpDirection direction)
{
  return uuids::to_string(imageUid) + ":" + uuids::to_string(sourceWarpUid) + ":" +
         std::to_string(static_cast<int>(direction));
}

std::string computedWarpDirectionLabel(ComputedWarpDirection direction)
{
  return ComputedWarpDirection::Inverse == direction ? "inverse" : "forward";
}

void logWarpInversionReport(
  const Image& image,
  const Image& sourceWarp,
  const WarpInversionResult& result,
  const uuids::uuid& resultUid)
{
  const WarpInversionReport& report = result.report;
  const double outsidePercent = report.samples + report.outsideOppositeField > 0
                                  ? 100.0 * static_cast<double>(report.outsideOppositeField) /
                                      static_cast<double>(report.samples + report.outsideOppositeField)
                                  : 0.0;

  spdlog::info(
    "Computed {} warp '{}' ({}) for image '{}' from '{}'. "
    "elapsed={:.3f}s, itkMeanError={:.6g}, itkMaxError={:.6g}, "
    "meanResidual={:.6g} mm ({:.6g} vox), maxResidual={:.6g} mm ({:.6g} vox), "
    "outsideOppositeField={}/{} ({:.2f}%)",
    computedWarpDirectionLabel(result.direction),
    result.image.settings().displayName(),
    resultUid,
    image.settings().displayName(),
    sourceWarp.settings().displayName(),
    report.elapsedSeconds,
    report.itkMeanErrorNorm,
    report.itkMaxErrorNorm,
    report.meanResidualMm,
    report.meanResidualVoxels,
    report.maxResidualMm,
    report.maxResidualVoxels,
    report.outsideOppositeField,
    report.samples + report.outsideOppositeField,
    outsidePercent);
}
} // namespace

void ImGuiWrapper::requestWarpInversion(
  const uuids::uuid& imageUid,
  const uuids::uuid& sourceWarpUid,
  ComputedWarpDirection direction,
  const WarpInversionOptions& options)
{
  const Image* image = m_appData.image(imageUid);
  const Image* sourceWarp = m_appData.warpField(sourceWarpUid);
  const auto refImageUid = m_appData.refImageUid();
  const Image* referenceImage = refImageUid ? m_appData.image(*refImageUid) : nullptr;
  const Image* outputDomain = ComputedWarpDirection::Inverse == direction ? referenceImage : image;

  if (!image || !sourceWarp || !outputDomain) {
    spdlog::warn(
      "Cannot compute {} warp: missing image, source warp, or output domain",
      computedWarpDirectionLabel(direction));
    return;
  }

  const std::string key = warpInversionTaskKey(imageUid, sourceWarpUid, direction);
  {
    std::lock_guard<std::mutex> lock(m_warpInversionFuturesMutex);
    if (m_pendingWarpInversionKeys.contains(key)) {
      return;
    }
    m_pendingWarpInversionKeys.insert(key);
  }

  const uuids::uuid taskUid = generateRandomUuid();
  auto progress = std::make_shared<std::atomic<double>>(0.0);
  auto cancel = std::make_shared<std::atomic_bool>(false);
  auto lastPostedProgress = std::make_shared<std::atomic<double>>(0.0);
  auto postEmptyEvent = m_postEmptyGlfwEvent;

  WarpInversionTaskState state{
    .imageUid = imageUid,
    .sourceWarpUid = sourceWarpUid,
    .direction = direction,
    .description = std::format("Computing {} warp", computedWarpDirectionLabel(direction)),
    .progress = progress,
    .cancel = cancel};

  Image sourceWarpCopy = *sourceWarp;
  Image outputDomainCopy = *outputDomain;

  auto future = std::async(
    std::launch::async,
    [imageUid,
     sourceWarpUid,
     direction,
     options,
     progress,
     cancel,
     lastPostedProgress,
     postEmptyEvent,
     sourceWarpCopy = std::move(sourceWarpCopy),
     outputDomainCopy = std::move(outputDomainCopy)]() mutable {
      auto progressCallback = [progress, lastPostedProgress, postEmptyEvent](double value) {
        const double clamped = std::clamp(value, 0.0, 1.0);
        progress->store(clamped);
        const double previous = lastPostedProgress->load();
        if (postEmptyEvent && (clamped >= 1.0 || clamped - previous >= 0.01)) {
          lastPostedProgress->store(clamped);
          postEmptyEvent();
        }
      };

      return WarpInversionTaskResult{
        imageUid,
        sourceWarpUid,
        direction,
        computeMatchingWarp(sourceWarpCopy, outputDomainCopy, direction, options, progressCallback, cancel.get())};
    });

  {
    std::lock_guard<std::mutex> lock(m_warpInversionFuturesMutex);
    m_warpInversionTaskStates.emplace(taskUid, std::move(state));
    m_warpInversionFutures.emplace(taskUid, std::move(future));
  }

  spdlog::info(
    "Started {} warp computation for image {} from source warp {}",
    computedWarpDirectionLabel(direction),
    imageUid,
    sourceWarpUid);

  if (m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}

void ImGuiWrapper::processWarpInversionFutures()
{
  using namespace std::chrono_literals;

  std::vector<uuids::uuid> readyTasks;
  {
    std::lock_guard<std::mutex> lock(m_warpInversionFuturesMutex);
    for (auto& [taskUid, future] : m_warpInversionFutures) {
      if (future.valid() && std::future_status::ready == future.wait_for(0ms)) {
        readyTasks.push_back(taskUid);
      }
    }
  }

  for (const auto& taskUid : readyTasks) {
    std::future<WarpInversionTaskResult> future;
    {
      std::lock_guard<std::mutex> lock(m_warpInversionFuturesMutex);
      auto it = m_warpInversionFutures.find(taskUid);
      if (m_warpInversionFutures.end() == it) {
        continue;
      }
      future = std::move(it->second);
      m_warpInversionFutures.erase(it);
    }

    std::optional<WarpInversionTaskResult> taskResult;
    try {
      taskResult = future.get();
    }
    catch (const std::exception& e) {
      spdlog::error("Warp inversion task {} failed: {}", taskUid, e.what());
      std::lock_guard<std::mutex> lock(m_warpInversionFuturesMutex);
      if (const auto stateIt = m_warpInversionTaskStates.find(taskUid); m_warpInversionTaskStates.end() != stateIt) {
        m_pendingWarpInversionKeys.erase(
          warpInversionTaskKey(stateIt->second.imageUid, stateIt->second.sourceWarpUid, stateIt->second.direction));
      }
      m_warpInversionTaskStates.erase(taskUid);
      continue;
    }

    {
      std::lock_guard<std::mutex> lock(m_warpInversionFuturesMutex);
      m_pendingWarpInversionKeys.erase(
        warpInversionTaskKey(taskResult->imageUid, taskResult->sourceWarpUid, taskResult->direction));
      m_warpInversionTaskStates.erase(taskUid);
    }

    if (!taskResult->result) {
      spdlog::warn(
        "Unable to compute {} warp for image {} from source warp {}: {}",
        computedWarpDirectionLabel(taskResult->direction),
        taskResult->imageUid,
        taskResult->sourceWarpUid,
        taskResult->result.error());
      continue;
    }

    const Image* sourceWarp = m_appData.warpField(taskResult->sourceWarpUid);
    const Image* image = m_appData.image(taskResult->imageUid);
    if (!sourceWarp || !image) {
      spdlog::warn("Image or source warp disappeared before computed warp could be assigned");
      continue;
    }

    WarpInversionResult result = std::move(*taskResult->result);
    const std::optional<uuids::uuid> resultUid = m_appData.addDef(std::move(result.image));
    if (!resultUid) {
      spdlog::warn("Unable to add computed {} warp to the project", computedWarpDirectionLabel(taskResult->direction));
      continue;
    }

    createImageTextures(m_appData, std::vector<uuids::uuid>{*resultUid});

    if (ComputedWarpDirection::Inverse == taskResult->direction) {
      (void)m_appData.assignInverseWarpUidToImage(taskResult->imageUid, *resultUid);
    }
    else {
      (void)m_appData.assignForwardWarpUidToImage(taskResult->imageUid, *resultUid);
    }

    const Image* resultImage = m_appData.warpField(*resultUid);
    if (resultImage) {
      result.image = *resultImage;
    }
    logWarpInversionReport(*image, *sourceWarp, result, *resultUid);

    if (m_updateImageUniforms) {
      m_updateImageUniforms(taskResult->imageUid);
    }
  }

  if (!readyTasks.empty() && m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}

void ImGuiWrapper::requestQueuedRegistrationJobs()
{
  const int maxConcurrentJobs = std::max(1, m_appData.settings().registrationBackendConfig().maxConcurrentJobs);
  std::size_t runningJobs = 0;
  {
    std::lock_guard<std::mutex> lock(m_registrationJobFuturesMutex);
    runningJobs = m_runningRegistrationJobIds.size();
  }

  if (runningJobs >= static_cast<std::size_t>(maxConcurrentJobs)) {
    return;
  }

  const registration::BackendConfig config = m_appData.settings().registrationBackendConfig();
  const registration::CommandGenerationOptions commandOptions = registration::commandOptions(config);
  const auto postEmptyEvent = m_postEmptyGlfwEvent;

  {
    std::lock_guard<std::mutex> lock(m_registrationJobFuturesMutex);
    for (const registration::JobRecord& job : m_appData.registrationJobs().jobs()) {
      if (job.status != registration::JobStatus::Cancelled) {
        continue;
      }
      if (const auto it = m_registrationJobCancelFlags.find(job.id); it != m_registrationJobCancelFlags.end()) {
        it->second->store(true);
      }
    }
  }

  std::vector<std::pair<std::string, registration::JobSpec>> jobsToLaunch;
  for (const registration::JobRecord& job : m_appData.registrationJobs().jobs()) {
    if (job.status != registration::JobStatus::Queued) {
      continue;
    }

    {
      std::lock_guard<std::mutex> lock(m_registrationJobFuturesMutex);
      if (m_runningRegistrationJobIds.contains(job.id)) {
        continue;
      }
    }

    registration::JobSpec jobSpec = job.spec;
    if (!materializeRegistrationInputs(jobSpec)) {
      m_appData.registrationJobs().appendProgress(
        job.id,
        registration::ProgressEvent{
          .kind = registration::ProgressEventKind::Failed,
          .message = "Unable to export registration inputs for backend execution."});
      continue;
    }

    jobsToLaunch.emplace_back(job.id, std::move(jobSpec));
    ++runningJobs;
    if (runningJobs >= static_cast<std::size_t>(maxConcurrentJobs)) {
      break;
    }
  }

  for (const auto& [jobId, jobSpec] : jobsToLaunch) {
    m_appData.registrationJobs().setStatus(jobId, registration::JobStatus::Running);
    const uuids::uuid taskUid = generateRandomUuid();
    auto cancelFlag = std::make_shared<std::atomic_bool>(false);
    const bool keepTemporaryFiles = config.keepTemporaryFiles;
    auto future = std::async(
      std::launch::async,
      [jobId, jobSpec, config, commandOptions, postEmptyEvent, cancelFlag, keepTemporaryFiles]() {
        registration::JobExecution execution;
        try {
          registration::ShellProcessRunner runner;
          registration::JobExecutionCallbacks callbacks;
          callbacks.shouldCancel = [cancelFlag]() {
            return cancelFlag->load();
          };
          if (checkRegistrationBackendBeforeLaunch(jobSpec, config, runner, execution)) {
            std::vector<std::string> preflightWarnings = std::move(execution.warnings);
            execution = registration::executeJob(jobSpec, commandOptions, runner, callbacks);
            execution.warnings.insert(
              execution.warnings.begin(),
              std::make_move_iterator(preflightWarnings.begin()),
              std::make_move_iterator(preflightWarnings.end()));
          }
          if (!keepTemporaryFiles && execution.status != registration::JobStatus::Failed) {
            removeTemporaryRegistrationInputs(jobSpec, execution);
          }
        }
        catch (const std::exception& e) {
          execution.status = registration::JobStatus::Failed;
          execution.errorMessage = e.what();
        }

        if (postEmptyEvent) {
          postEmptyEvent();
        }
        return RegistrationJobTaskResult{jobId, std::move(execution)};
      });

    {
      std::lock_guard<std::mutex> lock(m_registrationJobFuturesMutex);
      m_runningRegistrationJobIds.insert(jobId);
      m_registrationJobCancelFlags.emplace(jobId, std::move(cancelFlag));
      m_registrationJobIdsByTask.emplace(taskUid, jobId);
      m_registrationJobFutures.emplace(taskUid, std::move(future));
    }

    spdlog::info("Started registration job {}", jobId);
  }
}

void ImGuiWrapper::processRegistrationJobFutures()
{
  using namespace std::chrono_literals;

  std::vector<uuids::uuid> readyTasks;
  {
    std::lock_guard<std::mutex> lock(m_registrationJobFuturesMutex);
    for (auto& [taskUid, future] : m_registrationJobFutures) {
      if (future.valid() && std::future_status::ready == future.wait_for(0ms)) {
        readyTasks.push_back(taskUid);
      }
    }
  }

  for (const uuids::uuid& taskUid : readyTasks) {
    std::future<RegistrationJobTaskResult> future;
    std::string jobId;
    {
      std::lock_guard<std::mutex> lock(m_registrationJobFuturesMutex);
      auto futureIt = m_registrationJobFutures.find(taskUid);
      if (m_registrationJobFutures.end() == futureIt) {
        continue;
      }
      future = std::move(futureIt->second);
      m_registrationJobFutures.erase(futureIt);

      if (const auto jobIt = m_registrationJobIdsByTask.find(taskUid); m_registrationJobIdsByTask.end() != jobIt) {
        jobId = jobIt->second;
        m_registrationJobIdsByTask.erase(jobIt);
      }
    }

    RegistrationJobTaskResult result;
    try {
      result = future.get();
      if (jobId.empty()) {
        jobId = result.jobId;
      }
    }
    catch (const std::exception& e) {
      spdlog::error("Registration job task {} failed: {}", taskUid, e.what());
      if (!jobId.empty()) {
        registration::JobExecution failed;
        failed.status = registration::JobStatus::Failed;
        failed.errorMessage = e.what();
        m_appData.registrationJobs().applyExecution(jobId, failed);
      }
    }

    {
      std::lock_guard<std::mutex> lock(m_registrationJobFuturesMutex);
      if (!jobId.empty()) {
        m_runningRegistrationJobIds.erase(jobId);
        m_registrationJobCancelFlags.erase(jobId);
      }
    }

    if (!result.jobId.empty()) {
      if (const registration::JobRecord* job = m_appData.registrationJobs().find(result.jobId);
          job && job->status == registration::JobStatus::Cancelled)
      {
        spdlog::info("Registration job {} completed after cancellation; leaving job cancelled", result.jobId);
      }
      else {
        m_appData.registrationJobs().applyExecution(result.jobId, result.execution);
      }
      spdlog::info(
        "Registration job {} finished with status {}",
        result.jobId,
        registration::label(result.execution.status));
    }
  }

  if (!readyTasks.empty() && m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}

void ImGuiWrapper::renderWarpInversionProgressPopup()
{
  std::vector<WarpInversionTaskState> states;
  {
    std::lock_guard<std::mutex> lock(m_warpInversionFuturesMutex);
    for (const auto& [taskUid, state] : m_warpInversionTaskStates) {
      states.push_back(state);
    }
  }

  if (states.empty()) {
    if (ImGui::BeginPopupModal("Warp inversion progress", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
    return;
  }

  ImGui::OpenPopup("Warp inversion progress");
  if (ImGui::BeginPopupModal("Warp inversion progress", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    for (auto& state : states) {
      ImGui::TextUnformatted(state.description.c_str());
      const float progress = static_cast<float>(state.progress ? state.progress->load() : 0.0);
      ImGui::ProgressBar(progress, ImVec2{scaledPixel(320.0f), 0.0f});
      if (state.cancel && ImGui::Button("Cancel")) {
        state.cancel->store(true);
      }
    }
    ImGui::EndPopup();
  }
}

void ImGuiWrapper::addTaskToIsosurfaceGpuMeshGenerationQueue(const uuids::uuid& taskUid)
{
  std::lock_guard<std::mutex> lock(m_isosurfaceTaskQueueMutex);

  m_isosurfaceTaskQueueForGpuMeshGeneration.push(taskUid);

  // Post an empty event to notify render thread
  if (m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}

void ImGuiWrapper::generateIsosurfaceMeshGpuRecords()
{
  std::lock_guard<std::mutex> lock(m_isosurfaceTaskQueueMutex);

  while (!m_isosurfaceTaskQueueForGpuMeshGeneration.empty()) {
    const uuids::uuid taskUid = m_isosurfaceTaskQueueForGpuMeshGeneration.front();
    m_isosurfaceTaskQueueForGpuMeshGeneration.pop();

    auto it = m_futures.find(taskUid);
    if (std::end(m_futures) == it) {
      spdlog::error("Invalid task {}", taskUid);
      continue;
    }

    auto& future = it->second;

    // In case the CPU mesh generation task is not done, then wait for it to finish
    // and get the result. (Note: it should be done, since tasks only get on this queue when
    // CPU mesh generation is done.)
    const AsyncTaskDetails value = future.get();

    // Remove the future
    m_futures.erase(it);

    if (
      AsyncTasks::IsosurfaceMeshGeneration != value.task || false == value.success || !value.imageUid ||
      !value.imageComponent || !value.objectUid)
    {
      spdlog::error("Failed task {}", taskUid);
      continue;
    }

    spdlog::info("Task {}: Start generating GPU mesh for isosurface {} ", taskUid, *value.objectUid);

    // Get the isosurface associated with this task
    const Isosurface* surface = m_appData.isosurface(*value.imageUid, *value.imageComponent, *value.objectUid);
    if (!surface) {
      spdlog::error("Null isosurface for isosurface {} of image {}", *value.objectUid, *value.imageUid);
      continue;
    }

#if 0
    const MeshCpuRecord* cpuMeshRecord = surface->mesh.cpuData();

    if (!cpuMeshRecord)
    {
      spdlog::error(
        "Null CPU mesh record for isosurface {} of image {}", *value.objectUid, *value.imageUid
      );
      continue;
    }

    std::unique_ptr<MeshGpuRecord> gpuMeshRecord = gpuhelper::createMeshGpuRecordFromVtkPolyData(
      cpuMeshRecord->polyData(),
      cpuMeshRecord->meshInfo().primitiveType(),
      BufferUsagePattern::StreamDraw
    );

    if (!gpuMeshRecord)
    {
      spdlog::error(
        "Error generating GPU mesh record for isosurface {} of image {}",
        *value.objectUid,
        *value.imageUid
      );
      continue;
    }

    spdlog::info("Task {}: Done generating GPU mesh for isosurface {} ", taskUid, *value.objectUid);

    const bool updated = m_appData.updateIsosurfaceMeshGpuRecord(
      *value.imageUid, *value.imageComponent, *value.objectUid, std::move(gpuMeshRecord)
    );

    if (updated)
    {
      spdlog::info(
        "Updated GPU record for isosurface mesh {} of image {}", *value.objectUid, *value.imageUid
      );
    }
    else
    {
      spdlog::error(
        "Could not update GPU record for isosurface mesh {} of image {}",
        *value.objectUid,
        *value.imageUid
      );
    }
#endif
  }
}

/*
Q: How should I handle DPI in my application?
For ImGui 1.92, Entropy keeps fonts at their base size and applies the chosen DPI scale through
ImGuiStyle::FontScaleDpi. Style dimensions are reset to the captured base style and scaled with
style.ScaleAllSizes().

Your application may want to detect DPI change and reload the fonts and reset style between frames.

Your ui code should avoid using hardcoded constants for size and positioning. Prefer to express
values as multiple of reference values such as ImGui::GetFontSize() or ImGui::GetFrameHeight(). So
e.g. instead of seeing a hardcoded height of 500 for a given item/window, you may want to use
30*ImGui::GetFontSize() instead.
*/
void ImGuiWrapper::setContentScale(float scale)
{
  m_uiScaleManager.applyContentScale(scale);
}

void ImGuiWrapper::setUserScaleOverride(std::optional<float> scale)
{
  m_pendingUserScaleOverride = scale;
  if (m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}

void ImGuiWrapper::requestFontReload()
{
  m_pendingFontReload = true;
  if (m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}

void ImGuiWrapper::applyUiColorPreset(UiColorPreset preset)
{
  m_appData.settings().setUiColorPreset(preset);
  const UiDensityPreset densityPreset = m_appData.settings().uiDensityPreset();
  const float windowBgOpacity = m_appData.settings().uiWindowBgOpacity();
  m_uiScaleManager.updateBaseStyle([preset, densityPreset, windowBgOpacity](ImGuiStyle& style) {
    applyUiStylePreset(preset, &style);
    ::applyUiDensityPreset(densityPreset, &style);
    ::applyUiWindowBgOpacity(windowBgOpacity, &style);
  });
}

void ImGuiWrapper::applyUiDensityPreset(UiDensityPreset preset)
{
  m_appData.settings().setUiDensityPreset(preset);
  m_uiScaleManager.updateBaseStyle([preset](ImGuiStyle& style) { ::applyUiDensityPreset(preset, &style); });
}

void ImGuiWrapper::applyUiWindowBgOpacity(float opacity)
{
  m_appData.settings().setUiWindowBgOpacity(opacity);
  const float clampedOpacity = m_appData.settings().uiWindowBgOpacity();
  m_uiScaleManager.updateBaseStyle(
    [clampedOpacity](ImGuiStyle& style) { ::applyUiWindowBgOpacity(clampedOpacity, &style); });
}

void ImGuiWrapper::initializeFonts(float scale)
{
  static const std::string forkAwesomeFontPath = std::string("res/fonts/ForkAwesome/") + FONT_ICON_FILE_NAME_FK;
  const UiFontSpec uiFont = uiFontSpec(m_appData.settings().uiFontFamily());

  spdlog::debug("Begin loading fonts for UI scale {}", scale);

  ImFontConfig uiFontConfig;

  myImFormatString(uiFontConfig.Name, IM_ARRAYSIZE(uiFontConfig.Name), "%s, %.0fpx", uiFont.name, uiFont.size);

  // Merge in icons from Fork Awesome:
  ImFontConfig forkAwesomeFontConfig;
  forkAwesomeFontConfig.MergeMode = true;
  forkAwesomeFontConfig.PixelSnapH = true;

  const float forkAwesomeFontSize = 14.0f;

  myImFormatString(
    forkAwesomeFontConfig.Name,
    IM_ARRAYSIZE(forkAwesomeFontConfig.Name),
    "%s, %.0fpx",
    "Fork Awesome",
    forkAwesomeFontSize);

  /// @see For details about Fork Awesome fonts: https://forkaweso.me/Fork-Awesome/icons/
  static const ImWchar forkAwesomeIconGlyphRange[] = {ICON_MIN_FK, ICON_MAX_FK, 0};

  // Load fonts: If no fonts are loaded, dear imgui will use the default font.
  // You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
  // AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the
  // font among multiple. If the file cannot be loaded, the function will return NULL.
  // Please handle those errors in your application (e.g. use an assertion, or display an error and
  // quit). With ImGui 1.92 dynamic textures, glyphs are baked and uploaded by the backend
  // as needed during the frame/render path.
  /// @todo use Freetype Rasterizer and Small Font Sizes

  m_appData.guiData().m_fonts.clear();

  ImFont* uiFontPtr = loadFont(uiFont.path, uiFontConfig, uiFont.size, nullptr);
  ImFont* fork1Ptr =
    loadFont(forkAwesomeFontPath, forkAwesomeFontConfig, forkAwesomeFontSize, forkAwesomeIconGlyphRange);

  if (uiFontPtr && fork1Ptr) {
    m_appData.guiData().m_fonts[uiFont.path] = uiFontPtr;
    m_appData.guiData().m_fonts[std::string(uiFont.path) + forkAwesomeFontPath] = fork1Ptr;
    ImGui::GetIO().FontDefault = uiFontPtr;
    spdlog::debug("Loaded font {}", uiFont.path);
  }
  else {
    spdlog::error("Unable to load font {} or {}", uiFont.path, forkAwesomeFontPath);
  }

  spdlog::debug("Done loading fonts");
}

std::pair<std::string, std::string> ImGuiWrapper::getImageDisplayAndFileNames(std::size_t imageIndex) const
{
  static const std::string s_empty("<unknown>");

  if (const auto imageUid = m_appData.imageUid(imageIndex)) {
    if (const Image* image = m_appData.image(*imageUid)) {
      return {image->settings().displayName(), image->header().fileName().string()};
    }
  }

  return {s_empty.c_str(), s_empty.c_str()};
}

void ImGuiWrapper::render()
{
  using namespace std::placeholders;

  generateIsosurfaceMeshGpuRecords();
  processComponentProjectionFutures();
  processWarpInversionFutures();
  processRegistrationJobFutures();
  requestQueuedRegistrationJobs();

  if (m_pendingUserScaleOverride) {
    m_uiScaleManager.setUserScaleOverride(*m_pendingUserScaleOverride);
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

  /// @todo Move these functions elsewhere

  /// @todo remove this
  auto getActiveImageIndex = [this]() -> std::size_t {
    if (0 == m_appData.numImages()) {
      return 0;
    }

    if (const auto imageUid = m_appData.activeImageUid()) {
      if (const auto index = m_appData.imageIndex(*imageUid)) {
        return *index;
      }
    }

    spdlog::warn("No valid active image for {} loaded images", m_appData.numImages());
    return 0;
  };

  auto setActiveImageIndex = [this](std::size_t index) {
    if (const auto imageUid = m_appData.imageUid(index)) {
      if (!m_appData.setActiveImageUid(*imageUid)) {
        spdlog::warn("Cannot set active image to {}", *imageUid);
      }
    }
    else {
      spdlog::warn("Cannot set active image to invalid index {}", index);
    }
  };

  auto getImageHasActiveSeg = [this](std::size_t index) -> bool {
    if (const auto imageUid = m_appData.imageUid(index)) {
      return m_appData.isImageBeingSegmented(*imageUid);
    }
    else {
      spdlog::warn("Cannot get whether seg is active for invalid image index {}", index);
      return false;
    }
  };

  auto setImageHasActiveSeg = [this](std::size_t index, bool set) {
    if (const auto imageUid = m_appData.imageUid(index)) {
      m_appData.setImageBeingSegmented(*imageUid, set);
    }
    else {
      spdlog::warn("Cannot set whether seg is active for invalid image index {}", index);
    }
  };

  /// @todo remove this
  auto getMouseMode = [this]() {
    return m_appData.state().mouseMode();
  };

  auto setMouseMode = [this](MouseMode mouseMode) {
    m_callbackHandler.setMouseMode(mouseMode);
  };

  auto cycleViewLayout = [this](int step) {
    m_appData.windowData().cycleCurrentLayout(step);
  };

  /// @todo remove this
  auto getNumImageColorMaps = [this]() -> std::size_t {
    return m_appData.numImageColorMaps();
  };

  auto getImageColorMap = [this](std::size_t cmapIndex) -> ImageColorMap* {
    if (const auto cmapUid = m_appData.imageColorMapUid(cmapIndex)) {
      return m_appData.imageColorMap(*cmapUid);
    }
    return nullptr;
  };

  auto getLabelTable = [this](std::size_t tableIndex) -> ParcellationLabelTable* {
    if (const auto tableUid = m_appData.labelTableUid(tableIndex)) {
      return m_appData.labelTable(*tableUid);
    }
    return nullptr;
  };

  auto getImageIsVisibleSetting = [this](std::size_t imageIndex) -> bool {
    if (const auto imageUid = m_appData.imageUid(imageIndex)) {
      if (const Image* image = m_appData.image(*imageUid)) {
        return image->settings().visibility();
      }
    }
    return false;
  };

  auto getImageIsActive = [this](std::size_t imageIndex) -> bool {
    if (const auto imageUid = m_appData.imageUid(imageIndex)) {
      if (const auto activeImageUid = m_appData.activeImageUid()) {
        return (*imageUid == *activeImageUid);
      }
    }
    return false;
  };

  auto moveImageBackward = [this](const uuids::uuid& imageUid) -> bool {
    if (m_appData.moveImageBackwards(imageUid)) {
      m_appData.windowData().updateImageOrdering(m_appData.imageUidsOrdered());
      return true;
    }
    return false;
  };

  auto moveImageForward = [this](const uuids::uuid& imageUid) -> bool {
    if (m_appData.moveImageForwards(imageUid)) {
      m_appData.windowData().updateImageOrdering(m_appData.imageUidsOrdered());
      return true;
    }
    return false;
  };

  auto moveImageToBack = [this](const uuids::uuid& imageUid) -> bool {
    if (m_appData.moveImageToBack(imageUid)) {
      m_appData.windowData().updateImageOrdering(m_appData.imageUidsOrdered());
      return true;
    }
    return false;
  };

  auto moveImageToFront = [this](const uuids::uuid& imageUid) -> bool {
    if (m_appData.moveImageToFront(imageUid)) {
      m_appData.windowData().updateImageOrdering(m_appData.imageUidsOrdered());
      return true;
    }
    return false;
  };

  auto activeImageUid = [this]() -> std::optional<uuids::uuid> {
    return m_appData.activeImageUid();
  };

  auto activeSegUid = [activeImageUid, this]() -> std::optional<uuids::uuid> {
    const auto imageUid = activeImageUid();
    return imageUid ? m_appData.imageToActiveSegUid(*imageUid) : std::nullopt;
  };

  auto activeAnnotation = [activeImageUid,
                           this]() -> std::pair<std::optional<uuids::uuid>, std::optional<uuids::uuid>> {
    const auto imageUid = activeImageUid();
    if (!imageUid) {
      return {std::nullopt, std::nullopt};
    }
    return {*imageUid, m_appData.imageToActiveAnnotationUid(*imageUid)};
  };

  auto activeLandmarkGroupUid = [activeImageUid, this]() -> std::optional<uuids::uuid> {
    const auto imageUid = activeImageUid();
    return imageUid ? m_appData.imageToActiveLandmarkGroupUid(*imageUid) : std::nullopt;
  };

  auto createActiveSegmentation = [activeImageUid, getActiveImageIndex, this]() {
    const auto imageUid = activeImageUid();
    const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
    if (!imageUid || !image || !m_createBlankSeg) {
      return;
    }

    const std::size_t numSegsForImage = m_appData.imageToSegUids(*imageUid).size();
    const std::string displayName = "Untitled segmentation " + std::to_string(numSegsForImage + 1) + " for image '" +
                                    image->settings().displayName() + "'";
    if (m_createBlankSeg(*imageUid, displayName) && m_updateImageUniforms) {
      m_updateImageUniforms(*imageUid);
    }
    static_cast<void>(getActiveImageIndex);
  };

  auto saveActiveSegmentation = [activeSegUid, this]() {
    const auto segUid = activeSegUid();
    Image* seg = segUid ? m_appData.seg(*segUid) : nullptr;
    if (!seg) {
      return;
    }

    if (const auto selectedFile = native_dialog::saveFile(native_dialog::segmentationFilters())) {
      static constexpr uint32_t compToSave = 0;
      if (seg->saveComponentToDisk(compToSave, *selectedFile)) {
        spdlog::info("Saved segmentation image to file {}", *selectedFile);
        seg->header().setFileName(*selectedFile);
      }
      else {
        spdlog::error("Error saving segmentation image to file {}", *selectedFile);
      }
    }
  };

  auto projectHasAnnotations = [this]() {
    for (const auto& imageUid : m_appData.imageUidsOrdered()) {
      if (!m_appData.annotationsForImage(imageUid).empty()) {
        return true;
      }
    }
    return false;
  };

  auto saveAllAnnotations = [projectHasAnnotations, this]() {
    if (!projectHasAnnotations()) {
      return;
    }

    if (const auto selectedFile = native_dialog::saveFile(native_dialog::annotationFilters())) {
      nlohmann::json annotationsJson;
      std::size_t numSavedAnnotations = 0;
      for (const auto& imageUid : m_appData.imageUidsOrdered()) {
        for (const auto& annotUid : m_appData.annotationsForImage(imageUid)) {
          if (const Annotation* annot = m_appData.annotation(annotUid)) {
            serialize::appendAnnotationToJson(*annot, annotationsJson);
            ++numSavedAnnotations;
          }
        }
      }

      if (serialize::saveToJsonFile(annotationsJson, *selectedFile)) {
        spdlog::info("Saved {} annotations to JSON file {}", numSavedAnnotations, *selectedFile);
        for (const auto& imageUid : m_appData.imageUidsOrdered()) {
          for (const auto& annotUid : m_appData.annotationsForImage(imageUid)) {
            if (Annotation* annot = m_appData.annotation(annotUid)) {
              annot->setFileName(*selectedFile);
              annot->markClean();
            }
          }
        }
      }
      else {
        spdlog::error("Error saving annotations to JSON file {}", *selectedFile);
      }
    }
  };

  auto createActiveLandmarkGroup = [activeImageUid, this]() {
    const auto imageUid = activeImageUid();
    const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
    if (!imageUid || !image) {
      return;
    }

    LandmarkGroup newGroup;
    newGroup.setName("Landmarks for " + image->settings().displayName());
    const auto landmarkGroupUid = m_appData.addLandmarkGroup(std::move(newGroup));
    m_appData.assignLandmarkGroupUidToImage(*imageUid, landmarkGroupUid);
    m_appData.setRainbowColorsForAllLandmarkGroups();
    m_appData.assignActiveLandmarkGroupUidToImage(*imageUid, landmarkGroupUid);
  };

  auto saveActiveLandmarkGroup = [activeLandmarkGroupUid, this]() {
    const auto landmarkGroupUid = activeLandmarkGroupUid();
    LandmarkGroup* landmarkGroup = landmarkGroupUid ? m_appData.landmarkGroup(*landmarkGroupUid) : nullptr;
    if (!landmarkGroup) {
      return;
    }

    if (const auto selectedFile = native_dialog::saveFile(native_dialog::landmarkFilters())) {
      if (serialize::saveLandmarkGroupCsvFile(landmarkGroup->getPoints(), *selectedFile)) {
        spdlog::info("Saved landmarks to CSV file {}", *selectedFile);
        landmarkGroup->setFileName(*selectedFile);
      }
      else {
        spdlog::error("Error saving landmarks to CSV file {}", *selectedFile);
      }
    }
  };

  auto addLandmarkAtCrosshairs = [activeImageUid, activeLandmarkGroupUid, this]() {
    const auto imageUid = activeImageUid();
    const auto landmarkGroupUid = activeLandmarkGroupUid();
    const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
    LandmarkGroup* landmarkGroup = landmarkGroupUid ? m_appData.landmarkGroup(*landmarkGroupUid) : nullptr;
    if (!image || !landmarkGroup) {
      return;
    }

    const glm::mat4 landmark_T_world = landmarkGroup->getInVoxelSpace() ? image->transformations().pixel_T_worldDef()
                                                                        : image->transformations().subject_T_worldDef();
    const glm::vec4 landmarkPosition =
      landmark_T_world * glm::vec4{m_appData.state().worldCrosshairs().worldOrigin(), 1.0f};

    PointRecord<glm::vec3> point{glm::vec3{landmarkPosition / landmarkPosition.w}};
    const size_t newIndex = landmarkGroup->getPoints().empty() ? 0u : landmarkGroup->maxIndex() + 1;
    const auto colors = math::generateRandomHsvSamples(
      1,
      std::make_pair(0.0f, 360.0f),
      std::make_pair(0.3f, 1.0f),
      std::make_pair(0.3f, 1.0f),
      static_cast<uint32_t>(newIndex));
    if (!colors.empty()) {
      point.setColor(glm::rgbColor(colors.front()));
    }

    landmarkGroup->addPoint(newIndex, point);
  };

  auto requestResetProjectSettings = [this]() {
    if (!m_resetProjectSettings) {
      return;
    }

    const auto result = native_dialog::showMessageDialog(
      {"Reset project settings?",
       "Reset project display and review settings to defaults?",
       "This resets project-wide view, comparison, raycasting, projection, segmentation display, isosurface display, "
       "annotation display, and time-series synchronization settings. Loaded images, segmentations, landmarks, "
       "annotations, layouts, affine transformations, per-image settings, and deformation warp assignments are not "
       "removed or reset.",
       "Reset Project Settings",
       "Cancel",
       ""});

    if (result && native_dialog::MessageDialogResult::FirstButton == *result) {
      m_resetProjectSettings();
    }
  };

  auto performMenuAction = [activeImageUid,
                            activeSegUid,
                            activeAnnotation,
                            createActiveSegmentation,
                            saveActiveSegmentation,
                            saveAllAnnotations,
                            createActiveLandmarkGroup,
                            saveActiveLandmarkGroup,
                            addLandmarkAtCrosshairs,
                            moveImageBackward,
                            moveImageForward,
                            moveImageToBack,
                            moveImageToFront,
                            requestResetProjectSettings,
                            this](MainMenuAction action) {
    const auto mirrorInitialAffineToSegmentations = [this](const uuids::uuid& imageUid, const Image& image) {
      for (const uuids::uuid& segUid : m_appData.imageToSegUids(imageUid)) {
        if (Image* seg = m_appData.seg(segUid)) {
          auto& segTx = seg->transformations();
          segTx.set_enable_affine_T_subject(image.transformations().get_enable_affine_T_subject());
          segTx.set_affine_T_subject(image.transformations().get_affine_T_subject());
          segTx.set_affine_T_subject_fileName(image.transformations().get_affine_T_subject_fileName());
        }
      }
    };

    const auto updateAffineUniforms = [this]() {
      if (m_updateAllImageUniforms) {
        m_updateAllImageUniforms();
      }
    };

    const auto confirmResetAffineTransformation =
      [](const char* title, const char* message, const char* informativeText, const char* resetButton) {
        const auto result =
          native_dialog::showMessageDialog({title, message, informativeText, resetButton, "Cancel", ""});
        return result && native_dialog::MessageDialogResult::FirstButton == *result;
      };

    switch (action) {
      case MainMenuAction::SetModePointer:
        m_callbackHandler.setMouseMode(MouseMode::Pointer);
        break;
      case MainMenuAction::SetModeWindowLevel:
        m_callbackHandler.setMouseMode(MouseMode::WindowLevel);
        break;
      case MainMenuAction::SetModeZoom:
        m_callbackHandler.setMouseMode(MouseMode::CameraZoom);
        break;
      case MainMenuAction::SetModePan:
        m_callbackHandler.setMouseMode(MouseMode::CameraTranslate);
        break;
      case MainMenuAction::SetModeRotateView:
        m_callbackHandler.setMouseMode(MouseMode::CameraRotate);
        break;
      case MainMenuAction::SetModeRotateCrosshairs:
        m_callbackHandler.setMouseMode(MouseMode::CrosshairsRotate);
        break;
      case MainMenuAction::SetModeSegment:
        m_callbackHandler.setMouseMode(MouseMode::Segment);
        break;
      case MainMenuAction::SetModeAnnotate:
        m_callbackHandler.setMouseMode(MouseMode::Annotate);
        break;
      case MainMenuAction::SetModeTranslateImage:
        m_callbackHandler.setMouseMode(MouseMode::ImageTranslate);
        break;
      case MainMenuAction::SetModeRotateImage:
        m_callbackHandler.setMouseMode(MouseMode::ImageRotate);
        break;
      case MainMenuAction::SetModeScaleImage:
        m_callbackHandler.setMouseMode(MouseMode::ImageScale);
        break;
      case MainMenuAction::Recenter:
        if (m_recenterAllViews) m_recenterAllViews(false, false, true, false, true);
        break;
      case MainMenuAction::ResetView:
        if (m_recenterAllViews) m_recenterAllViews(true, true, true, true, true);
        break;
      case MainMenuAction::ToggleImageVisibility:
        m_callbackHandler.toggleImageVisibility();
        break;
      case MainMenuAction::ToggleSegmentationVisibility:
        m_callbackHandler.toggleSegVisibility();
        break;
      case MainMenuAction::ToggleImageEdges:
        m_callbackHandler.toggleImageEdges();
        break;
      case MainMenuAction::ToggleSegmentationOutline:
        m_callbackHandler.toggleSegGlobalOutline();
        break;
      case MainMenuAction::DecreaseActiveImageOpacity:
        m_callbackHandler.changeImageOpacity(-0.05);
        break;
      case MainMenuAction::IncreaseActiveImageOpacity:
        m_callbackHandler.changeImageOpacity(0.05);
        break;
      case MainMenuAction::DecreaseSegmentationOpacity:
        m_callbackHandler.changeSegOpacity(-0.05, false);
        break;
      case MainMenuAction::IncreaseSegmentationOpacity:
        m_callbackHandler.changeSegOpacity(0.05, false);
        break;
      case MainMenuAction::ToggleCrosshairsVoxelSnapping:
        m_appData.renderData().m_snapCrosshairs =
          CrosshairsSnapping::Disabled == m_appData.renderData().m_snapCrosshairs ? CrosshairsSnapping::ReferenceImage
                                                                                  : CrosshairsSnapping::Disabled;
        break;
      case MainMenuAction::ToggleScaleBars:
        m_appData.renderData().m_showScaleBars = !m_appData.renderData().m_showScaleBars;
        break;
      case MainMenuAction::ToggleAsciiRendering:
        m_appData.renderData().m_asciiEnabled = !m_appData.renderData().m_asciiEnabled;
        break;
      case MainMenuAction::ToggleLightboxOffsets:
        m_appData.renderData().m_showLightboxOffsetLabels = !m_appData.renderData().m_showLightboxOffsetLabels;
        break;
      case MainMenuAction::ToggleOverlays:
        m_callbackHandler.cycleOverlayAndUiVisibility();
        break;
      case MainMenuAction::ToggleFullScreen:
        m_callbackHandler.toggleFullScreenMode();
        break;
      case MainMenuAction::ToggleEntropyInstanceSync:
        m_appData.settings().setEntropyInstanceSyncEnabled(!m_appData.settings().entropyInstanceSyncEnabled());
        break;
      case MainMenuAction::ToggleSync:
        m_appData.settings().setCursorSyncEnabled(!m_appData.settings().cursorSyncEnabled());
        break;
      case MainMenuAction::ToggleSyncSendCursor:
        m_appData.settings().setSendCursorSync(!m_appData.settings().sendCursorSync());
        break;
      case MainMenuAction::ToggleSyncReceiveCursor:
        m_appData.settings().setReceiveCursorSync(!m_appData.settings().receiveCursorSync());
        break;
      case MainMenuAction::ToggleSyncSendZoom:
        m_appData.settings().setSendZoomSync(!m_appData.settings().sendZoomSync());
        break;
      case MainMenuAction::ToggleSyncReceiveZoom:
        m_appData.settings().setReceiveZoomSync(!m_appData.settings().receiveZoomSync());
        break;
      case MainMenuAction::ToggleSyncSendPan:
        m_appData.settings().setSendPanSync(!m_appData.settings().sendPanSync());
        break;
      case MainMenuAction::ToggleSyncReceivePan:
        m_appData.settings().setReceivePanSync(!m_appData.settings().receivePanSync());
        break;
      case MainMenuAction::SetActiveImageAsReference:
        if (const auto imageUid = activeImageUid()) {
          m_appData.guiData().m_pendingReferenceImageUid = *imageUid;
          m_appData.guiData().m_showConfirmSetReferenceImagePopup = true;
        }
        break;
      case MainMenuAction::ExportActiveImage:
        if (const auto imageUid = activeImageUid()) {
          image_export::exportDicomImage(m_appData, *imageUid);
        }
        break;
      case MainMenuAction::RemoveActiveImage:
        if (const auto imageUid = activeImageUid()) {
          m_appData.guiData().m_pendingRemoveImageUid = *imageUid;
          m_appData.guiData().m_showConfirmRemoveImagePopup = true;
        }
        break;
      case MainMenuAction::MoveActiveImageBackward:
        if (const auto imageUid = activeImageUid()) moveImageBackward(*imageUid);
        break;
      case MainMenuAction::MoveActiveImageForward:
        if (const auto imageUid = activeImageUid()) moveImageForward(*imageUid);
        break;
      case MainMenuAction::MoveActiveImageToBack:
        if (const auto imageUid = activeImageUid()) moveImageToBack(*imageUid);
        break;
      case MainMenuAction::MoveActiveImageToFront:
        if (const auto imageUid = activeImageUid()) moveImageToFront(*imageUid);
        break;
      case MainMenuAction::ToggleActiveImageTransformationLock:
        if (const auto imageUid = activeImageUid()) {
          const Image* image = m_appData.image(*imageUid);
          if (image && m_setLockManualImageTransformation) {
            m_setLockManualImageTransformation(*imageUid, !image->transformations().is_worldDef_T_affine_locked());
          }
        }
        break;
      case MainMenuAction::LoadActiveImageInitialTransformation:
        if (const auto imageUid = activeImageUid()) {
          Image* image = m_appData.image(*imageUid);
          const auto selectedFile = image ? native_dialog::openFile(native_dialog::transformFilters()) : std::nullopt;
          if (image && selectedFile) {
            glm::dmat4 affine_T_subject{1.0};
            if (serialize::openAffineTxFile(affine_T_subject, *selectedFile)) {
              image->transformations().set_enable_affine_T_subject(true);
              image->transformations().set_affine_T_subject(glm::mat4{affine_T_subject});
              image->transformations().set_affine_T_subject_fileName(*selectedFile);
              mirrorInitialAffineToSegmentations(*imageUid, *image);
              updateAffineUniforms();
              spdlog::info("Loaded initial affine transformation matrix from file {}", *selectedFile);
            }
            else {
              spdlog::error("Error loading initial affine transformation matrix from file {}", *selectedFile);
            }
          }
        }
        break;
      case MainMenuAction::SaveActiveImageInitialTransformation:
        if (const auto imageUid = activeImageUid()) {
          const Image* image = m_appData.image(*imageUid);
          const auto selectedFile = image ? native_dialog::saveFile(native_dialog::transformFilters()) : std::nullopt;
          if (image && selectedFile) {
            const glm::dmat4 affine_T_subject{image->transformations().get_affine_T_subject()};
            if (serialize::saveAffineTxFile(affine_T_subject, *selectedFile)) {
              spdlog::info("Saved initial affine transformation matrix to file {}", *selectedFile);
            }
            else {
              spdlog::error("Error saving initial affine transformation matrix to file {}", *selectedFile);
            }
          }
        }
        break;
      case MainMenuAction::ResetActiveImageInitialTransformation:
        if (const auto imageUid = activeImageUid()) {
          if (Image* image = m_appData.image(*imageUid)) {
            if (confirmResetAffineTransformation(
                  "Reset initial affine?",
                  "Reset the initial/imported affine transformation to identity?",
                  "This clears the source transform file and cannot be undone.",
                  "Reset Initial Affine"))
            {
              image->transformations().set_enable_affine_T_subject(true);
              image->transformations().set_affine_T_subject(glm::mat4{1.0f});
              image->transformations().set_affine_T_subject_fileName(std::nullopt);
              mirrorInitialAffineToSegmentations(*imageUid, *image);
              updateAffineUniforms();
            }
          }
        }
        break;
      case MainMenuAction::ResetActiveImageManualTransformation:
        if (const auto imageUid = activeImageUid()) {
          Image* image = m_appData.image(*imageUid);
          if (image) {
            if (confirmResetAffineTransformation(
                  "Reset manual affine?",
                  "Reset the manual affine transformation to identity?",
                  "This clears manual translation, rotation, and scale adjustments and cannot be undone.",
                  "Reset Manual Affine"))
            {
              image->transformations().reset_worldDef_T_affine();
              if (m_updateImageUniforms) {
                m_updateImageUniforms(*imageUid);
              }
            }
          }
        }
        break;
      case MainMenuAction::SaveActiveImageManualTransformation:
        if (const auto imageUid = activeImageUid()) {
          const Image* image = m_appData.image(*imageUid);
          const auto selectedFile = image ? native_dialog::saveFile(native_dialog::transformFilters()) : std::nullopt;
          if (selectedFile) {
            const glm::dmat4 worldDef_T_affine{image->transformations().get_worldDef_T_affine()};
            if (serialize::saveAffineTxFile(worldDef_T_affine, *selectedFile)) {
              spdlog::info("Saved manual affine transformation matrix to file {}", *selectedFile);
            }
            else {
              spdlog::error("Error saving manual affine transformation matrix to file {}", *selectedFile);
            }
          }
        }
        break;
      case MainMenuAction::SaveActiveImageEffectiveTransformation:
        if (const auto imageUid = activeImageUid()) {
          const Image* image = m_appData.image(*imageUid);
          const auto selectedFile = image ? native_dialog::saveFile(native_dialog::transformFilters()) : std::nullopt;
          if (selectedFile) {
            const auto& tx = image->transformations();
            const glm::dmat4 affine_T_subject{tx.get_affine_T_subject()};
            const glm::dmat4 worldDef_T_affine{tx.get_worldDef_T_affine()};
            if (serialize::saveAffineTxFile(worldDef_T_affine * affine_T_subject, *selectedFile)) {
              spdlog::info("Saved effective affine transformation matrix to file {}", *selectedFile);
            }
            else {
              spdlog::error("Error saving effective affine transformation matrix to file {}", *selectedFile);
            }
          }
        }
        break;
      case MainMenuAction::ToggleApplyActiveImageWarp:
        if (const auto imageUid = activeImageUid()) {
          if (Image* image = m_appData.image(*imageUid)) {
            image->settings().setWarpEnabled(!image->settings().warpEnabled());
            if (m_updateImageUniforms) {
              m_updateImageUniforms(*imageUid);
            }
          }
        }
        break;
      case MainMenuAction::ShowRegistrationSetupWindow:
        m_appData.guiData().m_showRegistrationSetupWindow = true;
        break;
      case MainMenuAction::ToggleRegistrationJobsWindow:
        m_appData.guiData().m_showRegistrationJobsWindow = !m_appData.guiData().m_showRegistrationJobsWindow;
        break;
      case MainMenuAction::ShowOpacityMixer:
        m_appData.guiData().m_showOpacityBlenderWindow = !m_appData.guiData().m_showOpacityBlenderWindow;
        break;
      case MainMenuAction::ToggleGlobalTimeControls:
        m_appData.settings().setShowGlobalTimeControls(!m_appData.settings().showGlobalTimeControls());
        break;
      case MainMenuAction::ToggleTimePlayback:
        m_callbackHandler.toggleTimePlayback();
        break;
      case MainMenuAction::FirstTimePoint:
        m_callbackHandler.setTimePointToFirst();
        break;
      case MainMenuAction::PreviousTimePoint:
        m_callbackHandler.cycleTimePoint(-1);
        break;
      case MainMenuAction::NextTimePoint:
        m_callbackHandler.cycleTimePoint(1);
        break;
      case MainMenuAction::LastTimePoint:
        m_callbackHandler.setTimePointToLast();
        break;
      case MainMenuAction::AddIsosurface:
        m_appData.guiData().m_showIsosurfacesWindow = true;
        m_appData.guiData().m_requestAddIsosurface = true;
        break;
      case MainMenuAction::AddIsosurfaceRange:
        m_appData.guiData().m_showIsosurfacesWindow = true;
        m_appData.guiData().m_requestAddIsosurfaceRange = true;
        break;
      case MainMenuAction::ToggleActiveImageIsosurfaces:
        if (const auto imageUid = activeImageUid()) {
          if (Image* image = m_appData.image(*imageUid)) {
            image->settings().setIsosurfacesVisible(!image->settings().isosurfacesVisible());
          }
        }
        break;
      case MainMenuAction::CreateSegmentation:
        createActiveSegmentation();
        break;
      case MainMenuAction::SaveSegmentation:
        saveActiveSegmentation();
        break;
      case MainMenuAction::ClearSegmentation:
        if (const auto segUid = activeSegUid(); segUid && m_clearSeg) m_clearSeg(*segUid);
        break;
      case MainMenuAction::RemoveSegmentation:
        if (const auto segUid = activeSegUid(); segUid && m_removeSeg && m_removeSeg(*segUid)) {
          if (const auto imageUid = activeImageUid(); imageUid && m_updateImageUniforms)
            m_updateImageUniforms(*imageUid);
        }
        break;
      case MainMenuAction::PreviousForegroundLabel:
        m_callbackHandler.cycleForegroundSegLabel(-1);
        break;
      case MainMenuAction::NextForegroundLabel:
        m_callbackHandler.cycleForegroundSegLabel(1);
        break;
      case MainMenuAction::PreviousBackgroundLabel:
        m_callbackHandler.cycleBackgroundSegLabel(-1);
        break;
      case MainMenuAction::NextBackgroundLabel:
        m_callbackHandler.cycleBackgroundSegLabel(1);
        break;
      case MainMenuAction::DecreaseBrushSize:
        m_callbackHandler.cycleBrushSize(-1);
        break;
      case MainMenuAction::IncreaseBrushSize:
        m_callbackHandler.cycleBrushSize(1);
        break;
      case MainMenuAction::PaintSegmentationFromAnnotation:
        if (m_paintActiveSegmentationWithActivePolygon) m_paintActiveSegmentationWithActivePolygon();
        break;
      case MainMenuAction::SaveAnnotations:
        saveAllAnnotations();
        break;
      case MainMenuAction::RemoveAnnotation:
        if (const auto [imageUid, annotUid] = activeAnnotation(); imageUid && annotUid) {
          m_appData.removeAnnotation(*annotUid);
          ASM::synchronizeAnnotationHighlights();
        }
        break;
      case MainMenuAction::MoveAnnotationBackward:
        if (const auto [imageUid, annotUid] = activeAnnotation(); imageUid && annotUid) {
          m_appData.moveAnnotationBackwards(*imageUid, *annotUid);
        }
        break;
      case MainMenuAction::MoveAnnotationForward:
        if (const auto [imageUid, annotUid] = activeAnnotation(); imageUid && annotUid) {
          m_appData.moveAnnotationForwards(*imageUid, *annotUid);
        }
        break;
      case MainMenuAction::MoveAnnotationToBack:
        if (const auto [imageUid, annotUid] = activeAnnotation(); imageUid && annotUid) {
          m_appData.moveAnnotationToBack(*imageUid, *annotUid);
        }
        break;
      case MainMenuAction::MoveAnnotationToFront:
        if (const auto [imageUid, annotUid] = activeAnnotation(); imageUid && annotUid) {
          m_appData.moveAnnotationToFront(*imageUid, *annotUid);
        }
        break;
      case MainMenuAction::CreateLandmarkGroup:
        createActiveLandmarkGroup();
        break;
      case MainMenuAction::SaveLandmarkGroup:
        saveActiveLandmarkGroup();
        break;
      case MainMenuAction::AddLayout:
        m_appData.guiData().m_showAddLayoutPopup = true;
        break;
      case MainMenuAction::RemoveLayout: {
        auto& windowData = m_appData.windowData();
        if (windowData.numLayouts() >= 2) {
          requestLayoutRemoval(m_appData, windowData.currentLayoutIndex());
        }
        break;
      }
      case MainMenuAction::ToggleLayoutTabs:
        m_appData.settings().setShowLayoutTabs(!m_appData.settings().showLayoutTabs());
        syncLayoutTabGuiDataFromSettings(m_appData);
        if (m_readjustViewport) {
          m_readjustViewport();
        }
        break;
      case MainMenuAction::ToggleImagesWindow:
        m_appData.guiData().m_showImagePropertiesWindow = !m_appData.guiData().m_showImagePropertiesWindow;
        break;
      case MainMenuAction::ToggleSegmentationsWindow:
        m_appData.guiData().m_showSegmentationsWindow = !m_appData.guiData().m_showSegmentationsWindow;
        break;
      case MainMenuAction::ToggleLandmarksWindow:
        m_appData.guiData().m_showLandmarksWindow = !m_appData.guiData().m_showLandmarksWindow;
        break;
      case MainMenuAction::ToggleAnnotationsWindow:
        m_appData.guiData().m_showAnnotationsWindow = !m_appData.guiData().m_showAnnotationsWindow;
        break;
      case MainMenuAction::ToggleIsosurfacesWindow:
        m_appData.guiData().m_showIsosurfacesWindow = !m_appData.guiData().m_showIsosurfacesWindow;
        break;
      case MainMenuAction::ToggleSettingsWindow:
        m_appData.guiData().m_showSettingsWindow = !m_appData.guiData().m_showSettingsWindow;
        break;
      case MainMenuAction::ShowSettingsWindow:
        m_appData.guiData().m_showSettingsWindow = true;
        break;
      case MainMenuAction::ShowSynchronizeSettingsWindow:
        m_appData.guiData().m_showSettingsWindow = true;
        m_appData.guiData().m_requestedSettingsTab = GuiData::SettingsTab::Synchronization;
        break;
      case MainMenuAction::ToggleInspectorWindow:
        m_appData.guiData().m_showInspectionWindow = !m_appData.guiData().m_showInspectionWindow;
        break;
      case MainMenuAction::ToggleOpacityMixerWindow:
        m_appData.guiData().m_showOpacityBlenderWindow = !m_appData.guiData().m_showOpacityBlenderWindow;
        break;
      case MainMenuAction::ResetPanelLayout:
        ImGui::ClearIniSettings();
        m_applyDefaultPanelLayout = true;
        break;
      case MainMenuAction::ToggleImGuiDemoWindow:
        m_appData.guiData().m_showImGuiDemoWindow = !m_appData.guiData().m_showImGuiDemoWindow;
        break;
      case MainMenuAction::ToggleImPlotDemoWindow:
        m_appData.guiData().m_showImPlotDemoWindow = !m_appData.guiData().m_showImPlotDemoWindow;
        break;
      case MainMenuAction::ToggleToolbar:
        m_appData.guiData().m_showModeToolbar = !m_appData.guiData().m_showModeToolbar;
        if (m_readjustViewport) m_readjustViewport();
        break;
      case MainMenuAction::ResetProjectSettings:
        requestResetProjectSettings();
        break;
      case MainMenuAction::AddLandmark:
        addLandmarkAtCrosshairs();
        break;
    }
    if (m_postEmptyGlfwEvent) {
      m_postEmptyGlfwEvent();
    }
  };

  auto isMenuActionEnabled =
    [activeImageUid, activeSegUid, activeAnnotation, activeLandmarkGroupUid, projectHasAnnotations, this](
      MainMenuAction action) {
      const bool loaded = ProjectLoadState::Loaded == m_appData.state().projectLoadState();
      const bool backgroundTaskRunning = m_appData.state().animating();
      const bool canUseProjectActions = loaded && !backgroundTaskRunning;
      const bool hasActiveImage = activeImageUid().has_value();
      const bool hasActiveSeg = activeSegUid().has_value();
      const auto [annotImageUid, annotUid] = activeAnnotation();
      const bool hasActiveAnnotation = annotImageUid.has_value() && annotUid.has_value();
      switch (action) {
        case MainMenuAction::SetModePointer:
        case MainMenuAction::SetModeWindowLevel:
        case MainMenuAction::SetModeZoom:
        case MainMenuAction::SetModePan:
        case MainMenuAction::SetModeRotateView:
        case MainMenuAction::SetModeRotateCrosshairs:
        case MainMenuAction::SetModeSegment:
        case MainMenuAction::SetModeAnnotate:
        case MainMenuAction::SetModeTranslateImage:
        case MainMenuAction::SetModeRotateImage:
        case MainMenuAction::SetModeScaleImage:
        case MainMenuAction::Recenter:
        case MainMenuAction::ResetView:
        case MainMenuAction::ToggleImageVisibility:
        case MainMenuAction::ToggleImageEdges:
        case MainMenuAction::DecreaseActiveImageOpacity:
        case MainMenuAction::IncreaseActiveImageOpacity:
        case MainMenuAction::ToggleCrosshairsVoxelSnapping:
        case MainMenuAction::ToggleScaleBars:
        case MainMenuAction::ToggleAsciiRendering:
        case MainMenuAction::ToggleLightboxOffsets:
        case MainMenuAction::ToggleOverlays:
        case MainMenuAction::ToggleEntropyInstanceSync:
        case MainMenuAction::ToggleSync:
        case MainMenuAction::ToggleSyncSendCursor:
        case MainMenuAction::ToggleSyncReceiveCursor:
        case MainMenuAction::ToggleSyncSendZoom:
        case MainMenuAction::ToggleSyncReceiveZoom:
        case MainMenuAction::ToggleSyncSendPan:
        case MainMenuAction::ToggleSyncReceivePan:
        case MainMenuAction::ShowOpacityMixer:
        case MainMenuAction::FirstTimePoint:
        case MainMenuAction::PreviousTimePoint:
        case MainMenuAction::NextTimePoint:
        case MainMenuAction::LastTimePoint:
        case MainMenuAction::AddIsosurface:
        case MainMenuAction::AddIsosurfaceRange:
        case MainMenuAction::ToggleActiveImageIsosurfaces:
        case MainMenuAction::CreateSegmentation:
        case MainMenuAction::CreateLandmarkGroup:
        case MainMenuAction::AddLayout:
          if (
            action == MainMenuAction::FirstTimePoint || action == MainMenuAction::PreviousTimePoint ||
            action == MainMenuAction::NextTimePoint || action == MainMenuAction::LastTimePoint)
          {
            return loaded && globalTimeControlImageUid(m_appData).has_value();
          }
          return canUseProjectActions && hasActiveImage;
        case MainMenuAction::ToggleGlobalTimeControls:
          return loaded;
        case MainMenuAction::ToggleTimePlayback:
          return loaded && globalTimeControlImageUid(m_appData).has_value();
        case MainMenuAction::ToggleLayoutTabs:
          return canUseProjectActions;
        case MainMenuAction::ResetProjectSettings:
          return canUseProjectActions;
        case MainMenuAction::PreviousForegroundLabel:
        case MainMenuAction::NextForegroundLabel:
        case MainMenuAction::PreviousBackgroundLabel:
        case MainMenuAction::NextBackgroundLabel:
        case MainMenuAction::DecreaseBrushSize:
        case MainMenuAction::IncreaseBrushSize:
          return canUseProjectActions && hasActiveSeg;
        case MainMenuAction::ToggleFullScreen:
        case MainMenuAction::ToggleImagesWindow:
        case MainMenuAction::ToggleSegmentationsWindow:
        case MainMenuAction::ToggleLandmarksWindow:
        case MainMenuAction::ToggleAnnotationsWindow:
        case MainMenuAction::ToggleIsosurfacesWindow:
        case MainMenuAction::ToggleSettingsWindow:
        case MainMenuAction::ShowSettingsWindow:
        case MainMenuAction::ShowSynchronizeSettingsWindow:
        case MainMenuAction::ToggleInspectorWindow:
        case MainMenuAction::ToggleOpacityMixerWindow:
        case MainMenuAction::ResetPanelLayout:
        case MainMenuAction::ToggleImGuiDemoWindow:
        case MainMenuAction::ToggleImPlotDemoWindow:
        case MainMenuAction::ToggleToolbar:
          return !backgroundTaskRunning;
        case MainMenuAction::ToggleSegmentationVisibility:
        case MainMenuAction::ToggleSegmentationOutline:
        case MainMenuAction::DecreaseSegmentationOpacity:
        case MainMenuAction::IncreaseSegmentationOpacity:
        case MainMenuAction::SaveSegmentation:
        case MainMenuAction::ClearSegmentation:
          return canUseProjectActions && hasActiveSeg;
        case MainMenuAction::RemoveSegmentation:
          if (!canUseProjectActions || !hasActiveSeg || !hasActiveImage) return false;
          return m_appData.imageToSegUids(*activeImageUid()).size() > 1;
        case MainMenuAction::ExportActiveImage:
          if (!canUseProjectActions || !hasActiveImage) return false;
          return image_export::imageHasDicomSource(m_appData, *activeImageUid());
        case MainMenuAction::SetActiveImageAsReference:
        case MainMenuAction::RemoveActiveImage:
        case MainMenuAction::MoveActiveImageBackward:
        case MainMenuAction::MoveActiveImageForward:
        case MainMenuAction::MoveActiveImageToBack:
        case MainMenuAction::MoveActiveImageToFront:
          return canUseProjectActions && hasActiveImage;
        case MainMenuAction::ToggleActiveImageTransformationLock:
          if (!canUseProjectActions || !hasActiveImage || !m_setLockManualImageTransformation) return false;
          return activeImageUid() != m_appData.refImageUid();
        case MainMenuAction::LoadActiveImageInitialTransformation:
        case MainMenuAction::SaveActiveImageInitialTransformation:
        case MainMenuAction::ResetActiveImageInitialTransformation:
        case MainMenuAction::ResetActiveImageManualTransformation:
        case MainMenuAction::SaveActiveImageManualTransformation:
          return canUseProjectActions && hasActiveImage && activeImageUid() != m_appData.refImageUid();
        case MainMenuAction::SaveActiveImageEffectiveTransformation:
          if (!canUseProjectActions || !hasActiveImage || activeImageUid() == m_appData.refImageUid()) return false;
          if (const Image* image = m_appData.image(*activeImageUid())) {
            return image->transformations().get_enable_affine_T_subject();
          }
          return false;
        case MainMenuAction::ToggleApplyActiveImageWarp:
          return canUseProjectActions && hasActiveImage && activeImageUid() != m_appData.refImageUid() &&
                 m_appData.imageToActiveInverseWarpUid(*activeImageUid()).has_value();
        case MainMenuAction::ShowRegistrationSetupWindow:
          return canUseProjectActions;
        case MainMenuAction::ToggleRegistrationJobsWindow:
          return canUseProjectActions;
        case MainMenuAction::PaintSegmentationFromAnnotation:
          return canUseProjectActions && hasActiveSeg && hasActiveAnnotation;
        case MainMenuAction::SaveAnnotations:
          return canUseProjectActions && projectHasAnnotations();
        case MainMenuAction::RemoveAnnotation:
        case MainMenuAction::MoveAnnotationBackward:
        case MainMenuAction::MoveAnnotationForward:
        case MainMenuAction::MoveAnnotationToBack:
        case MainMenuAction::MoveAnnotationToFront:
          return canUseProjectActions && hasActiveAnnotation;
        case MainMenuAction::SaveLandmarkGroup:
          return canUseProjectActions && activeLandmarkGroupUid().has_value();
        case MainMenuAction::RemoveLayout:
          return canUseProjectActions && m_appData.windowData().numLayouts() >= 2;
        case MainMenuAction::AddLandmark:
          return canUseProjectActions && activeLandmarkGroupUid().has_value();
      }
      return false;
    };

  auto isMenuActionChecked = [this](MainMenuAction action) {
    switch (action) {
      case MainMenuAction::SetModePointer:
        return MouseMode::Pointer == m_appData.state().mouseMode();
      case MainMenuAction::SetModeWindowLevel:
        return MouseMode::WindowLevel == m_appData.state().mouseMode();
      case MainMenuAction::SetModeZoom:
        return MouseMode::CameraZoom == m_appData.state().mouseMode();
      case MainMenuAction::SetModePan:
        return MouseMode::CameraTranslate == m_appData.state().mouseMode();
      case MainMenuAction::SetModeRotateView:
        return MouseMode::CameraRotate == m_appData.state().mouseMode();
      case MainMenuAction::SetModeRotateCrosshairs:
        return MouseMode::CrosshairsRotate == m_appData.state().mouseMode();
      case MainMenuAction::SetModeSegment:
        return MouseMode::Segment == m_appData.state().mouseMode();
      case MainMenuAction::SetModeAnnotate:
        return MouseMode::Annotate == m_appData.state().mouseMode();
      case MainMenuAction::SetModeTranslateImage:
        return MouseMode::ImageTranslate == m_appData.state().mouseMode();
      case MainMenuAction::SetModeRotateImage:
        return MouseMode::ImageRotate == m_appData.state().mouseMode();
      case MainMenuAction::SetModeScaleImage:
        return MouseMode::ImageScale == m_appData.state().mouseMode();
      case MainMenuAction::ToggleImageVisibility: {
        const auto imageUid = m_appData.activeImageUid();
        const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
        if (!image) {
          return false;
        }
        const bool isMulticomponentImage = image->header().numComponentsPerPixel() > 1;
        return isMulticomponentImage ? image->settings().globalVisibility() : image->settings().visibility();
      }
      case MainMenuAction::ToggleSegmentationVisibility: {
        const auto imageUid = m_appData.activeImageUid();
        const auto segUid = imageUid ? m_appData.imageToActiveSegUid(*imageUid) : std::nullopt;
        const Image* seg = segUid ? m_appData.seg(*segUid) : nullptr;
        return seg ? seg->settings().visibility() : false;
      }
      case MainMenuAction::ToggleImageEdges: {
        const auto imageUid = m_appData.activeImageUid();
        const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
        return image ? image->settings().showAnyEdges() : false;
      }
      case MainMenuAction::ToggleSegmentationOutline:
        return SegmentationOutlineStyle::Disabled != m_appData.renderData().m_segOutlineStyle;
      case MainMenuAction::ToggleActiveImageTransformationLock: {
        const auto imageUid = m_appData.activeImageUid();
        const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
        return image ? image->transformations().is_worldDef_T_affine_locked() : false;
      }
      case MainMenuAction::ToggleApplyActiveImageWarp: {
        const auto imageUid = m_appData.activeImageUid();
        const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
        return image ? image->settings().warpEnabled() : false;
      }
      case MainMenuAction::ToggleRegistrationJobsWindow:
        return m_appData.guiData().m_showRegistrationJobsWindow;
      case MainMenuAction::ToggleScaleBars:
        return m_appData.renderData().m_showScaleBars;
      case MainMenuAction::ToggleCrosshairsVoxelSnapping:
        return CrosshairsSnapping::Disabled != m_appData.renderData().m_snapCrosshairs;
      case MainMenuAction::ToggleAsciiRendering:
        return m_appData.renderData().m_asciiEnabled;
      case MainMenuAction::ToggleLightboxOffsets:
        return m_appData.renderData().m_showLightboxOffsetLabels;
      case MainMenuAction::ToggleLayoutTabs:
        return m_appData.settings().showLayoutTabs();
      case MainMenuAction::ToggleGlobalTimeControls:
        return m_appData.settings().showGlobalTimeControls();
      case MainMenuAction::ToggleEntropyInstanceSync:
        return m_appData.settings().entropyInstanceSyncEnabled();
      case MainMenuAction::ToggleSync:
        return m_appData.settings().cursorSyncEnabled();
      case MainMenuAction::ToggleSyncSendCursor:
        return m_appData.settings().sendCursorSync();
      case MainMenuAction::ToggleSyncReceiveCursor:
        return m_appData.settings().receiveCursorSync();
      case MainMenuAction::ToggleSyncSendZoom:
        return m_appData.settings().sendZoomSync();
      case MainMenuAction::ToggleSyncReceiveZoom:
        return m_appData.settings().receiveZoomSync();
      case MainMenuAction::ToggleSyncSendPan:
        return m_appData.settings().sendPanSync();
      case MainMenuAction::ToggleSyncReceivePan:
        return m_appData.settings().receivePanSync();
      case MainMenuAction::ToggleToolbar:
        return m_appData.guiData().m_showModeToolbar;
      case MainMenuAction::ToggleImagesWindow:
        return m_appData.guiData().m_showImagePropertiesWindow;
      case MainMenuAction::ToggleSegmentationsWindow:
        return m_appData.guiData().m_showSegmentationsWindow;
      case MainMenuAction::ToggleLandmarksWindow:
        return m_appData.guiData().m_showLandmarksWindow;
      case MainMenuAction::ToggleAnnotationsWindow:
        return m_appData.guiData().m_showAnnotationsWindow;
      case MainMenuAction::ToggleIsosurfacesWindow:
        return m_appData.guiData().m_showIsosurfacesWindow;
      case MainMenuAction::ToggleSettingsWindow:
        return m_appData.guiData().m_showSettingsWindow;
      case MainMenuAction::ToggleInspectorWindow:
        return m_appData.guiData().m_showInspectionWindow;
      case MainMenuAction::ToggleOpacityMixerWindow:
        return m_appData.guiData().m_showOpacityBlenderWindow;
      case MainMenuAction::ShowRegistrationSetupWindow:
        return m_appData.guiData().m_showRegistrationSetupWindow;
      case MainMenuAction::ShowOpacityMixer:
        return m_appData.guiData().m_showOpacityBlenderWindow;
      case MainMenuAction::ToggleTimePlayback: {
        const auto imageUid = globalTimeControlImageUid(m_appData);
        const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
        return image ? image->settings().timePlaybackPlaying() : false;
      }
      case MainMenuAction::ToggleActiveImageIsosurfaces: {
        const auto imageUid = m_appData.activeImageUid();
        const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
        return image ? image->settings().isosurfacesVisible() : false;
      }
      case MainMenuAction::ToggleImGuiDemoWindow:
        return m_appData.guiData().m_showImGuiDemoWindow;
      case MainMenuAction::ToggleImPlotDemoWindow:
        return m_appData.guiData().m_showImPlotDemoWindow;
      default:
        return false;
    }
  };

  auto applyImageSelectionAndRenderModesToAllViews = [this](const uuids::uuid& viewUid) {
    m_appData.windowData().applyImageSelectionToAllCurrentViews(viewUid);
    m_appData.windowData().applyViewRenderModeAndProjectionToAllCurrentViews(viewUid);
  };

  auto applyImageSelectionToAllViews = [this](const uuids::uuid& viewUid) {
    m_appData.windowData().applyImageSelectionToAllCurrentViews(viewUid);
  };

  auto getViewCameraRotation = [this](const uuids::uuid& viewUid) -> glm::quat {
    const View* view = m_appData.windowData().getCurrentView(viewUid);
    if (!view) return k_identityRotation;

    return helper::computeCameraRotationRelativeToWorld(view->camera());
  };

  auto setViewCameraRotation = [this](const uuids::uuid& viewUid, const glm::quat& camera_T_world_rotationDelta) {
    m_callbackHandler.doCameraRotate3d(viewUid, camera_T_world_rotationDelta);
  };

  auto setViewCameraDirection = [this](const uuids::uuid& viewUid, const glm::vec3& worldFwdDirection) {
    m_callbackHandler.handleSetViewForwardDirection(viewUid, worldFwdDirection);
  };

  auto getViewNormal = [this](const uuids::uuid& viewUid) {
    View* view = m_appData.windowData().getCurrentView(viewUid);
    if (!view) return k_zeroVec;
    return helper::worldDirection(view->camera(), Directions::View::Back);
  };

  auto getObliqueViewDirections = [this](const uuids::uuid& viewUidToExclude) -> std::vector<glm::vec3> {
    std::vector<glm::vec3> obliqueViewDirections;

    for (std::size_t i = 0; i < m_appData.windowData().numLayouts(); ++i) {
      const Layout* layout = m_appData.windowData().layout(i);
      if (!layout) continue;

      for (const auto& view : layout->views()) {
        if (view.first == viewUidToExclude) continue;
        if (!view.second) continue;

        if (!helper::looksAlongOrthogonalAxis(view.second->camera())) {
          obliqueViewDirections.emplace_back(helper::worldDirection(view.second->camera(), Directions::View::Front));
        }
      }
    }

    return obliqueViewDirections;
  };

  ImGui::NewFrame();

  const ProjectLoadState projectLoadState = m_appData.state().projectLoadState();
  const bool backgroundTaskRunning = m_appData.state().animating();
  const bool hasLoadedProject =
    ProjectLoadState::Loaded == projectLoadState && 0 != m_appData.windowData().numLayouts();

  if (hasLoadedProject) {
    const ImGuiID dockspaceId = renderMainDockspace(m_appData.guiData());
    if (m_applyDefaultPanelLayout) {
      applyDefaultPanelDockLayout(dockspaceId, m_appData.guiData());
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

  auto defaultProjectSaveDirectory = [this]() {
    if (m_appData.projectFileName()) {
      return m_appData.projectFileName()->parent_path();
    }
    const auto refImageUid = m_appData.refImageUid();
    const Image* refImage = refImageUid ? m_appData.image(*refImageUid) : nullptr;
    return refImage ? refImage->header().fileName().parent_path() : fs::path{};
  };

  auto defaultProjectSaveName = [this]() {
    if (m_appData.projectFileName()) {
      return m_appData.projectFileName()->filename().string();
    }
    const auto refImageUid = m_appData.refImageUid();
    const Image* refImage = refImageUid ? m_appData.image(*refImageUid) : nullptr;
    return refImage ? (refImage->header().fileName().stem().string() + ".json") : std::string{"project.json"};
  };

  auto defaultLayoutsSaveDirectory = defaultProjectSaveDirectory;
  auto defaultLayoutsSaveName = [this]() {
    if (m_appData.projectFileName()) {
      return m_appData.projectFileName()->stem().string() + "-layouts.json";
    }
    const auto refImageUid = m_appData.refImageUid();
    const Image* refImage = refImageUid ? m_appData.image(*refImageUid) : nullptr;
    return refImage ? (refImage->header().fileName().stem().string() + "-layouts.json") : std::string{"layouts.json"};
  };

  auto layoutNames = [this]() {
    const auto& layouts = m_appData.windowData().layouts();
    std::vector<std::string> baseNames;
    baseNames.reserve(layouts.size());

    std::unordered_map<std::string, std::size_t> baseNameCounts;
    for (std::size_t index = 0; index < layouts.size(); ++index) {
      baseNames.emplace_back(m_appData.windowData().layoutDisplayName(index));
      ++baseNameCounts[baseNames.back()];
    }

    std::vector<std::string> names;
    names.reserve(layouts.size());
    std::unordered_map<std::string, std::size_t> seenBaseNameCounts;
    for (std::size_t index = 0; index < layouts.size(); ++index) {
      std::string displayName = baseNames.at(index);
      const bool managedLightbox = LayoutKind::Lightbox == layouts.at(index).kind();
      const std::string imageName = managedLightbox ? layoutImageDisplayName(m_appData, layouts.at(index)) : "";
      if (managedLightbox && !imageName.empty()) {
        displayName += " - " + imageName;
      }
      else if (baseNameCounts.at(displayName) > 1) {
        displayName += " " + std::to_string(++seenBaseNameCounts[displayName]);
      }
      names.emplace_back(std::to_string(index + 1) + ". " + displayName);
    }
    return names;
  };

  renderConfirmCloseAppPopup(m_appData, m_quitAppWithoutPrompt);
  renderUnsavedProjectPopup(
    m_appData,
    m_saveProject,
    m_saveProjectAs,
    m_closeProjectWithoutPrompt,
    m_quitAppWithoutPrompt,
    defaultProjectSaveDirectory,
    defaultProjectSaveName);

  if (m_appData.guiData().m_renderUiWindows) {
    renderConfirmSetReferenceImagePopup(m_appData, m_setReferenceImage);
    renderConfirmRemoveImagePopup(m_appData, m_removeImage);
    renderLargeImageLoadPromptPopup(m_appData, m_largeImageLoadDecision);
    renderDicomFolderPathPopup(m_appData, m_openDicomFolders);
    renderDicomSeriesSelectionPopup(m_appData, m_loadDicomSeries);

    if (m_appData.guiData().m_showImGuiDemoWindow) {
      ImGui::ShowDemoWindow(&m_appData.guiData().m_showImGuiDemoWindow);
    }

    if (m_appData.guiData().m_showImPlotDemoWindow) {
      ImPlot::ShowDemoWindow(&m_appData.guiData().m_showImPlotDemoWindow);
    }

    const auto activeImageUidForMenu = m_appData.activeImageUid();
    const auto refImageUidForMenu = m_appData.refImageUid();
    const bool canLoadDeformationFieldForActiveImage =
      ProjectLoadState::Loaded == projectLoadState && !backgroundTaskRunning && activeImageUidForMenu &&
      (!refImageUidForMenu || *activeImageUidForMenu != *refImageUidForMenu) && m_loadDeformationField;

    const MainMenuBarCallbacks mainMenuCallbacks{
      .openImageFiles = m_openImageFiles,
      .addImageFiles = m_addImageFiles,
      .openDicomFolders = m_openDicomFolders,
      .requestDicomFolderPathDialog = [this]() { m_appData.guiData().m_showDicomFolderPathPopup = true; },
      .addSegmentationFile = m_addSegmentationFile,
      .loadInverseWarpForActiveImage =
        [this](const fs::path& fileName) {
          const auto imageUid = m_appData.activeImageUid();
          const auto refImageUid = m_appData.refImageUid();
          if (!imageUid || (refImageUid && *imageUid == *refImageUid) || !m_loadDeformationField) {
            return;
          }

          const std::optional<uuids::uuid> defUid = m_loadDeformationField(fileName);
          if (!defUid) {
            spdlog::error("Unable to load inverse warp field from {}", fileName);
            return;
          }

          if (!m_appData.assignInverseWarpUidToImage(*imageUid, *defUid)) {
            spdlog::error("Unable to assign inverse warp field {} to image {}", *defUid, *imageUid);
            return;
          }

          if (m_updateImageUniforms) {
            try {
              m_updateImageUniforms(*imageUid);
            }
            catch (const std::exception& e) {
              spdlog::error(
                "Exception after assigning inverse warp field {} to active image {}: {}\n{}",
                *defUid,
                *imageUid,
                e.what(),
                stack_trace::current(1));
              throw;
            }
          }
          if (m_postEmptyGlfwEvent) {
            m_postEmptyGlfwEvent();
          }
        },
      .loadForwardWarpForActiveImage =
        [this](const fs::path& fileName) {
          const auto imageUid = m_appData.activeImageUid();
          const auto refImageUid = m_appData.refImageUid();
          if (!imageUid || (refImageUid && *imageUid == *refImageUid) || !m_loadDeformationField) {
            return;
          }

          const std::optional<uuids::uuid> defUid = m_loadDeformationField(fileName);
          if (!defUid) {
            spdlog::error("Unable to load forward warp from {}", fileName);
            return;
          }

          if (!m_appData.assignForwardWarpUidToImage(*imageUid, *defUid)) {
            spdlog::error("Unable to assign forward warp {} to image {}", *defUid, *imageUid);
            return;
          }

          if (m_postEmptyGlfwEvent) {
            m_postEmptyGlfwEvent();
          }
        },
      .openProjectFile = m_openProjectFile,
      .saveProject = m_saveProject,
      .saveProjectAs = m_saveProjectAs,
      .projectFileName = [this]() { return m_appData.projectFileName(); },
      .defaultProjectSaveDirectory = defaultProjectSaveDirectory,
      .defaultProjectSaveName = defaultProjectSaveName,
      .closeProject = m_closeProject,
      .quitApp = m_requestQuitApp,
      .loadLayoutsFile = m_loadLayoutsFile,
      .saveLayoutsFile = m_saveLayoutsFile,
      .resetProjectSettings = m_resetProjectSettings,
      .defaultLayoutsSaveDirectory = defaultLayoutsSaveDirectory,
      .defaultLayoutsSaveName = defaultLayoutsSaveName,
      .layoutNames = layoutNames,
      .currentLayoutIndex = [this]() { return m_appData.windowData().currentLayoutIndex(); },
      .setCurrentLayoutIndex =
        [this](std::size_t index) {
          m_appData.windowData().setCurrentLayoutIndex(index);
          if (m_postEmptyGlfwEvent) {
            m_postEmptyGlfwEvent();
          }
        },
      .cycleLayouts =
        [this](int step) {
          m_appData.windowData().cycleCurrentLayout(step);
          if (m_postEmptyGlfwEvent) {
            m_postEmptyGlfwEvent();
          }
        },
      .imageNames =
        [this]() {
          std::vector<std::string> names;
          names.reserve(m_appData.numImages());
          for (std::size_t i = 0; i < m_appData.numImages(); ++i) {
            names.emplace_back(std::to_string(i + 1) + ". " + getImageDisplayAndFileNames(i).first);
          }
          return names;
        },
      .activeImageIndex = getActiveImageIndex,
      .setActiveImageIndex = setActiveImageIndex,
      .performAction = performMenuAction,
      .isActionEnabled = isMenuActionEnabled,
      .isActionChecked = isMenuActionChecked,
      .showAbout = [this]() { m_appData.guiData().m_showAboutDialog = true; },
      .canOpenProject = ProjectLoadState::Loading != projectLoadState && !backgroundTaskRunning,
      .canAddImage = ProjectLoadState::Loaded == projectLoadState && !backgroundTaskRunning,
      .canAddSegmentation =
        ProjectLoadState::Loaded == projectLoadState && !backgroundTaskRunning && m_appData.activeImageUid(),
      .canLoadDeformationFieldForActiveImage = canLoadDeformationFieldForActiveImage,
      .canSaveProject = ProjectLoadState::Loaded == projectLoadState && !backgroundTaskRunning,
      .canCloseProject = ProjectLoadState::Empty != projectLoadState && !backgroundTaskRunning,
      .canUseLayouts = ProjectLoadState::Loaded == projectLoadState && !backgroundTaskRunning};

#ifdef __APPLE__
    updateMacOSNativeMainMenu(mainMenuCallbacks);
#elif defined(_WIN32)
    updateWindowsNativeMainMenu(m_window, mainMenuCallbacks);
#else
    renderMainMenuBar(m_appData.guiData(), mainMenuCallbacks);
#endif

    renderEmptyWorkspace(
      projectLoadState,
      m_openImageFiles,
      m_openDicomFolders,
      [this]() { m_appData.guiData().m_showDicomFolderPathPopup = true; },
      m_openProjectFile);

    if (backgroundTaskRunning) {
      renderLoadingStatusWindow(m_appData.guiData());
    }
    renderWarpInversionProgressPopup();

    if (hasLoadedProject) {
      renderLayoutTabs(m_appData);
      renderGlobalTimeControl(m_appData);
    }

    if (m_appData.guiData().m_showSettingsWindow) {
      static std::string s_settingsPersistenceStatus;
      const auto applyActivePreferences = [this]() {
        setUserScaleOverride(m_appData.settings().uiScaleOverride());
        requestFontReload();
        applyUiColorPreset(m_appData.settings().uiColorPreset());
        applyUiDensityPreset(m_appData.settings().uiDensityPreset());
        applyUiWindowBgOpacity(m_appData.settings().uiWindowBgOpacity());
        if (m_updateMetricUniforms) {
          m_updateMetricUniforms();
        }
      };
      const std::filesystem::path settingsFile = app_paths::userSettingsFile();
      const SettingsPersistenceCallbacks settingsPersistenceCallbacks{
        .settingsFile = settingsFile,
        .saveSettings =
          [this, settingsFile]() {
            std::string error;
            if (user_preferences::save(
                  m_appData.settings(),
                  m_appData.renderData(),
                  m_appData.guiData(),
                  settingsFile,
                  &error))
            {
              s_settingsPersistenceStatus = "Saved";
              return true;
            }
            s_settingsPersistenceStatus = "Save failed: " + error;
            return false;
          },
        .saveSettingsAs =
          [this](const std::filesystem::path& fileName) {
            std::string error;
            if (user_preferences::save(
                  m_appData.settings(),
                  m_appData.renderData(),
                  m_appData.guiData(),
                  fileName,
                  &error)) {
              s_settingsPersistenceStatus = "Saved " + fileName.filename().string();
              return true;
            }
            s_settingsPersistenceStatus = "Save failed: " + error;
            return false;
          },
        .restoreDefaults =
          [this, applyActivePreferences]() {
            user_preferences::applyDefaults(m_appData.settings(), m_appData.renderData(), m_appData.guiData());
            applyActivePreferences();
            syncLayoutTabGuiDataFromSettings(m_appData);
            s_settingsPersistenceStatus = "Defaults restored";
          },
        .resetInterfaceSettings =
          [this]() {
            ImGui::ClearIniSettings();
            ImGui::SaveIniSettingsToDisk(m_iniFileName.c_str());
            m_applyDefaultPanelLayout = true;
            if (m_readjustViewport) {
              m_readjustViewport();
            }
            if (m_postEmptyGlfwEvent) {
              m_postEmptyGlfwEvent();
            }
            s_settingsPersistenceStatus = "Interface reset";
          },
        .statusText =
          []() {
            return s_settingsPersistenceStatus;
          }};

      renderSettingsWindow(
        m_appData,
        getNumImageColorMaps,
        getImageColorMap,
        m_updateMetricUniforms,
        [this](std::optional<float> scale) { setUserScaleOverride(scale); },
        [this]() { requestFontReload(); },
        [this](UiColorPreset preset) { applyUiColorPreset(preset); },
        [this](UiDensityPreset preset) { applyUiDensityPreset(preset); },
        [this](float opacity) { applyUiWindowBgOpacity(opacity); },
        m_readjustViewport,
        settingsPersistenceCallbacks,
        m_recenterAllViews);
    }

    if (!hasLoadedProject) {
      renderAboutDialogModalPopup(m_appData.guiData().m_showAboutDialog);
      m_appData.guiData().m_showAboutDialog = false;

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
      return;
    }

    requestMissingComponentProjectionImages();

    if (m_appData.guiData().m_showIsosurfacesWindow) {
      renderIsosurfacesWindow(
        m_appData,
        std::bind(&ImGuiWrapper::storeFuture, this, _1, _2),
        std::bind(&ImGuiWrapper::addTaskToIsosurfaceGpuMeshGenerationQueue, this, _1));
    }

    using namespace std::placeholders;

    if (m_appData.guiData().m_showInspectionWindow) {
      try {
        renderInspectionWindowWithTable(
          m_appData,
          std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
          m_getSubjectPos,
          m_getVoxelPos,
          m_setSubjectPos,
          m_setVoxelPos,
          m_getImageValuesNN,
          m_getImageValuesLinear,
          m_getSegLabel,
          getLabelTable,
          m_updateImageUniforms);
      }
      catch (const std::exception& e) {
        spdlog::error("Exception while rendering voxel inspector: {}\n{}", e.what(), stack_trace::current(1));
        throw;
      }
    }

    if (m_appData.guiData().m_showImagePropertiesWindow) {
      renderImagePropertiesWindow(
        m_appData,
        m_appData.numImages(),
        std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
        getActiveImageIndex,
        setActiveImageIndex,
        getNumImageColorMaps,
        getImageColorMap,
        moveImageBackward,
        moveImageForward,
        moveImageToBack,
        moveImageToFront,
        m_updateAllImageUniforms,
        m_updateImageUniforms,
        m_updateImageInterpolationMode,
        m_updateImageColorMapInterpolationMode,
        m_loadDeformationField,
        [this](
          const uuids::uuid& imageUid,
          const uuids::uuid& sourceWarpUid,
          ComputedWarpDirection direction,
          const WarpInversionOptions& options) { requestWarpInversion(imageUid, sourceWarpUid, direction, options); },
        m_setLockManualImageTransformation,
        [this](const uuids::uuid& imageUid, ComponentProjectionMode mode) {
          requestComponentProjectionImage(imageUid, mode);
        },
        [this](const uuids::uuid& imageUid) {
          m_appData.guiData().m_pendingReferenceImageUid = imageUid;
          m_appData.guiData().m_showConfirmSetReferenceImagePopup = true;
        },
        [this](const uuids::uuid& imageUid) {
          m_appData.guiData().m_pendingRemoveImageUid = imageUid;
          m_appData.guiData().m_showConfirmRemoveImagePopup = true;
        },
        m_recenterAllViews);
    }

    if (m_appData.guiData().m_showSegmentationsWindow) {
      renderSegmentationPropertiesWindow(
        m_appData,
        getLabelTable,
        m_updateImageUniforms,
        m_updateLabelColorTableTexture,
        m_moveCrosshairsToSegLabelCentroid,
        m_createBlankSeg,
        m_addSegmentationFileToImage,
        m_clearSeg,
        m_removeSeg,
        m_recenterAllViews);
    }

    if (m_appData.guiData().m_showLandmarksWindow) {
      renderLandmarkPropertiesWindow(m_appData, m_recenterAllViews);
    }

    if (m_appData.guiData().m_showAnnotationsWindow) {
      renderAnnotationWindow(
        m_appData,
        setViewCameraDirection,
        m_paintActiveSegmentationWithActivePolygon,
        m_recenterAllViews);
    }

    if (m_appData.guiData().m_showOpacityBlenderWindow) {
      renderOpacityBlenderWindow(m_appData, m_updateImageUniforms);
    }

    if (m_appData.guiData().m_showRegistrationSetupWindow) {
      renderRegistrationSetupWindow(m_appData);
    }

    if (m_appData.guiData().m_showRegistrationJobsWindow) {
      renderRegistrationJobsWindow(m_appData, [this](const std::string& jobId) {
        importRegistrationJobOutputs(jobId);
      });
    }

    renderRegistrationProgressWindow(m_appData);

    renderModeToolbar(
      m_appData,
      getMouseMode,
      setMouseMode,
      m_readjustViewport,
      m_recenterAllViews,
      m_getOverlayVisibility,
      m_setOverlayVisibility,
      cycleViewLayout,

      m_appData.numImages(),
      std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
      getActiveImageIndex,
      setActiveImageIndex);

    renderAddLayoutModalPopup(m_appData, m_appData.guiData().m_showAddLayoutPopup, [this]() {
      if (m_recenterAllViews) {
        m_recenterAllViews(false, false, true, false, true);
      }
    });
    m_appData.guiData().m_showAddLayoutPopup = false;

    renderConfirmRemoveLayoutPopup(m_appData);

    renderAboutDialogModalPopup(m_appData.guiData().m_showAboutDialog);
    m_appData.guiData().m_showAboutDialog = false;

    renderSegToolbar(
      m_appData,
      m_appData.numImages(),
      std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
      getActiveImageIndex,
      setActiveImageIndex,
      getImageHasActiveSeg,
      setImageHasActiveSeg,
      m_readjustViewport,
      m_executePoissonSeg);

    annotationToolbar(m_paintActiveSegmentationWithActivePolygon);
  }

  if (ProjectLoadState::Loaded != m_appData.state().projectLoadState() || 0 == m_appData.windowData().numLayouts()) {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return;
  }

  Layout& currentLayout = m_appData.windowData().currentLayout();

  const float wholeWindowHeight = static_cast<float>(m_appData.windowData().getWindowSize().y);

  if (m_appData.guiData().m_renderUiOverlays && currentLayout.isLightbox()) {
    // Per-layout UI controls:

    static constexpr bool k_recenterCrosshairs = false;
    static constexpr bool k_realignCrosshairs = false;
    static constexpr bool k_doNotRecenterOnCurrentCrosshairsPosition = false;
    static constexpr bool k_resetObliqueOrientation = false;
    static constexpr bool k_resetZoom = true;

    const auto viewFrameBounds = helper::computeMindowFrameBounds(
      currentLayout.windowClipViewport(),
      m_appData.windowData().viewport().getAsVec4(),
      wholeWindowHeight);

    const ViewOverlayWindowContext overlayContext{
      currentLayout.uid(),
      viewFrameBounds,
      currentLayout.uiControls(),
      false,
      LayoutKind::Lightbox != currentLayout.kind(),
      m_appData.state().worldCrosshairs(),
      m_appData.windowData().getContentScaleRatios()};

    const ViewOverlayImageCallbacks imageCallbacks{
      m_appData.numImages(),
      [this, &currentLayout](std::size_t index) { return currentLayout.isImageRendered(m_appData, index); },
      [this, &currentLayout](std::size_t index, bool visible) {
        currentLayout.setImageRendered(m_appData, index, visible);
      },
      nullptr,
      [this, &currentLayout](std::size_t index) { return currentLayout.isImageUsedForMetric(m_appData, index); },
      [this, &currentLayout](std::size_t index, bool visible) {
        currentLayout.setImageUsedForMetric(m_appData, index, visible);
      },
      std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
      getImageIsVisibleSetting,
      getImageIsActive};

    const ViewOverlayModeCallbacks modeCallbacks{
      currentLayout.viewType(),
      currentLayout.renderMode(),
      currentLayout.intensityProjectionMode(),
      [this](const ViewType& viewType) { m_appData.windowData().setCurrentLayoutViewType(m_appData, viewType); },
      [&currentLayout](const ViewRenderMode& renderMode) { return currentLayout.setRenderMode(renderMode); },
      [&currentLayout](const IntensityProjectionMode& ipMode) {
        return currentLayout.setIntensityProjectionMode(ipMode);
      },
      [this]() {
        m_recenterAllViews(
          k_recenterCrosshairs,
          k_realignCrosshairs,
          k_doNotRecenterOnCurrentCrosshairsPosition,
          k_resetObliqueOrientation,
          k_resetZoom);
      },
      nullptr,
      LayoutKind::Lightbox == currentLayout.kind()
        ? std::vector<ViewType>{ViewType::Axial, ViewType::Coronal, ViewType::Sagittal}
        : std::vector<ViewType>{}};

    const ViewOverlayProjectionCallbacks projectionCallbacks{
      [this]() { return m_appData.renderData().m_intensityProjectionSlabThickness; },
      [this](float thickness) { m_appData.renderData().m_intensityProjectionSlabThickness = thickness; },
      [this]() { return m_appData.renderData().m_doMaxExtentIntensityProjection; },
      [this](bool set) { m_appData.renderData().m_doMaxExtentIntensityProjection = set; },
      [this]() { return m_appData.renderData().m_xrayIntensityWindow; },
      [this](float window) { m_appData.renderData().m_xrayIntensityWindow = window; },
      [this]() { return m_appData.renderData().m_xrayIntensityLevel; },
      [this](float level) { m_appData.renderData().m_xrayIntensityLevel = level; },
      [this]() { return m_appData.renderData().m_xrayEnergyKeV; },
      [this](float energy) {
        m_appData.renderData().setXrayEnergy(energy);
      }};

    renderViewSettingsComboWindow(overlayContext, imageCallbacks, modeCallbacks, projectionCallbacks);

    renderViewOrientationToolWindow(
      overlayContext,
      {currentLayout.viewType(),
       [&getViewCameraRotation, &currentLayout]() { return getViewCameraRotation(currentLayout.uid()); },
       [&setViewCameraRotation, &currentLayout](const glm::quat& q) {
         return setViewCameraRotation(currentLayout.uid(), q);
       },
       [&setViewCameraDirection, &currentLayout](const glm::vec3& dir) {
         return setViewCameraDirection(currentLayout.uid(), dir);
       },
       [&getViewNormal, &currentLayout]() { return getViewNormal(currentLayout.uid()); },
       getObliqueViewDirections});
  }
  else if (m_appData.guiData().m_renderUiOverlays && !currentLayout.isLightbox()) {
    // Per-view UI controls:

    for (const auto& viewUid : m_appData.windowData().currentViewUids()) {
      View* view = m_appData.windowData().getCurrentView(viewUid);
      if (!view) return;

      auto setViewType = [view](const ViewType& viewType) {
        if (view) view->setViewType(viewType);
      };

      auto setRenderMode = [view](const ViewRenderMode& renderMode) {
        if (view) view->setRenderMode(renderMode);
      };

      auto setIntensityProjectionMode = [view](const IntensityProjectionMode& ipMode) {
        if (view) view->setIntensityProjectionMode(ipMode);
      };

      auto recenter = [this, &viewUid]() {
        m_recenterView(viewUid);
      };

      const auto viewFrameBounds = helper::computeMindowFrameBounds(
        view->windowClipViewport(),
        m_appData.windowData().viewport().getAsVec4(),
        wholeWindowHeight);

      const ViewOverlayWindowContext overlayContext{
        viewUid,
        viewFrameBounds,
        view->uiControls(),
        true,
        true,
        m_appData.state().worldCrosshairs(),
        m_appData.windowData().getContentScaleRatios()};

      const ViewOverlayImageCallbacks imageCallbacks{
        m_appData.numImages(),
        [this, view](std::size_t index) { return view->isImageRendered(m_appData, index); },
        [this, view](std::size_t index, bool visible) { view->setImageRendered(m_appData, index, visible); },
        applyImageSelectionToAllViews,
        [this, view](std::size_t index) { return view->isImageUsedForMetric(m_appData, index); },
        [this, view](std::size_t index, bool visible) { view->setImageUsedForMetric(m_appData, index, visible); },
        std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
        getImageIsVisibleSetting,
        getImageIsActive};

      const ViewOverlayModeCallbacks modeCallbacks{
        view->viewType(),
        view->renderMode(),
        view->intensityProjectionMode(),
        setViewType,
        setRenderMode,
        setIntensityProjectionMode,
        recenter,
        applyImageSelectionAndRenderModesToAllViews};

      const ViewOverlayProjectionCallbacks projectionCallbacks{
        [this]() { return m_appData.renderData().m_intensityProjectionSlabThickness; },
        [this](float thickness) { m_appData.renderData().m_intensityProjectionSlabThickness = thickness; },
        [this]() { return m_appData.renderData().m_doMaxExtentIntensityProjection; },
        [this](bool set) { m_appData.renderData().m_doMaxExtentIntensityProjection = set; },
        [this]() { return m_appData.renderData().m_xrayIntensityWindow; },
        [this](float window) { m_appData.renderData().m_xrayIntensityWindow = window; },
        [this]() { return m_appData.renderData().m_xrayIntensityLevel; },
        [this](float level) { m_appData.renderData().m_xrayIntensityLevel = level; },
        [this]() { return m_appData.renderData().m_xrayEnergyKeV; },
        [this](float energy) {
          m_appData.renderData().setXrayEnergy(energy);
        }};

      renderViewSettingsComboWindow(overlayContext, imageCallbacks, modeCallbacks, projectionCallbacks);

      renderViewOrientationToolWindow(
        overlayContext,
        {view->viewType(),
         [&getViewCameraRotation, &viewUid]() { return getViewCameraRotation(viewUid); },
         [&setViewCameraRotation, &viewUid](const glm::quat& q) { return setViewCameraRotation(viewUid, q); },
         [&setViewCameraDirection, &viewUid](const glm::vec3& dir) { return setViewCameraDirection(viewUid, dir); },
         [&getViewNormal, &viewUid]() { return getViewNormal(viewUid); },
         getObliqueViewDirections});
    }
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

void ImGuiWrapper::annotationToolbar(const std::function<void()> paintActiveAnnotation)
{
  if (!state::annot::isInStateWhereToolbarVisible()) {
    return;
  }

  if (!ASM::current_state_ptr || !ASM::current_state_ptr->selectedViewUid()) {
    return;
  }

  // Position the annotation toolbar at the bottom of this view:
  const View* annotationView = m_appData.windowData().getView(*ASM::current_state_ptr->selectedViewUid());

  const float wholeWindowHeight = static_cast<float>(m_appData.windowData().getWindowSize().y);

  const auto mindowAnnotViewFrameBounds = helper::computeMindowFrameBounds(
    annotationView->windowClipViewport(),
    m_appData.windowData().viewport().getAsVec4(),
    wholeWindowHeight);

  renderAnnotationToolbar(m_appData, mindowAnnotViewFrameBounds, paintActiveAnnotation);
}
