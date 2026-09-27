#define IMGUI_DEFINE_MATH_OPERATORS
#include "ui/imgui/Layout.h"

#include "logic/app/Data.h"
#include "ui/Helpers.h"
#include "ui/ImGuiCustomControls.h"
#include "ui/Scaling.h"
#include "ui/dialogs/NativeMessageDialogs.h"

#include <IconsForkAwesome.h>
#include <imgui/imgui_internal.h>

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <numeric>
#include <unordered_map>
#include <utility>

namespace ui::imgui_detail
{
constexpr float k_layoutTabWindowPaddingX = 6.0f;
constexpr float k_layoutTabWindowPaddingY = 0.0f;
constexpr float k_layoutTabFrameRounding = 3.0f;

struct LayoutTabMetrics
{
  ImVec2 windowPadding;
  float frameRounding = 0.0f;
  float windowHeight = 0.0f;
  float innerGap = 0.0f;
};

LayoutTabMetrics layoutTabMetrics()
{
  const ImVec2 windowPadding{ui::scaledSize(k_layoutTabWindowPaddingX, k_layoutTabWindowPaddingY)};
  const float windowHeight = ImGui::GetFrameHeight() + (2.0f * windowPadding.y);
  const float dockspaceClearance = std::max(1.0f, ImGui::GetStyle().TabBarBorderSize);
  return LayoutTabMetrics{
    .windowPadding = windowPadding,
    .frameRounding = ui::scaledPixel(k_layoutTabFrameRounding),
    .windowHeight = windowHeight,
    .innerGap = dockspaceClearance};
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
  ImGuiViewport* viewport = ImGui::GetMainViewport();
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
  ImGuiViewport* viewport = ImGui::GetMainViewport();
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
  constexpr ImGuiDockNodeFlags k_hiddenTabFlags = static_cast<ImGuiDockNodeFlags>(ImGuiDockNodeFlags_AutoHideTabBar) |
                                                  static_cast<ImGuiDockNodeFlags>(ImGuiDockNodeFlags_HiddenTabBar) |
                                                  static_cast<ImGuiDockNodeFlags>(ImGuiDockNodeFlags_NoTabBar);
  const ImGuiDockNodeFlags visibleTabFlags = static_cast<ImGuiDockNodeFlags>(node->LocalFlags & ~k_hiddenTabFlags);
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
};

DefaultDockLayoutFractions defaultDockLayoutFractions(const ImVec2& dockspaceSize)
{
  const float aspectRatio = dockspaceSize.y > 1.0f ? dockspaceSize.x / dockspaceSize.y : 1.0f;
  const bool wideWorkspace = aspectRatio >= 2.10f;
  const bool narrowWorkspace = aspectRatio <= 1.35f;

  const float leftTargetFraction = wideWorkspace ? 0.23f : (narrowWorkspace ? 0.18f : 0.20f);
  const float leftMinSize = narrowWorkspace ? 220.0f : 280.0f;
  const float leftMaxSize = wideWorkspace ? 640.0f : 520.0f;

  const float rightTargetFraction = wideWorkspace ? 0.20f : (narrowWorkspace ? 0.16f : 0.17f);
  const float rightMinSize = narrowWorkspace ? 200.0f : 240.0f;
  const float rightMaxSize = wideWorkspace ? 560.0f : 440.0f;

  return DefaultDockLayoutFractions{
    .leftPanel = clampedDockSplitFraction(dockspaceSize.x, leftTargetFraction, leftMinSize, leftMaxSize),
    .rightPanel = clampedDockSplitFraction(dockspaceSize.x, rightTargetFraction, rightMinSize, rightMaxSize),
    .inspector = clampedDockSplitFraction(dockspaceSize.y, 0.10f, 120.0f, 190.0f)};
}

void applyDefaultPanelDockLayout(ImGuiID dockspaceId, const AppData& appData)
{
  if (0 == dockspaceId) {
    return;
  }

  const DockspaceGeometry geometry = mainDockspaceGeometry(appData.guiData());

  ImGui::DockBuilderRemoveNode(dockspaceId);
  constexpr ImGuiDockNodeFlags k_dockspaceFlags = static_cast<ImGuiDockNodeFlags>(ImGuiDockNodeFlags_DockSpace) |
                                                  ImGuiDockNodeFlags_PassthruCentralNode |
                                                  ImGuiDockNodeFlags_NoDockingInCentralNode;
  ImGui::DockBuilderAddNode(dockspaceId, k_dockspaceFlags);
  ImGui::DockBuilderSetNodePos(dockspaceId, geometry.pos);
  ImGui::DockBuilderSetNodeSize(dockspaceId, geometry.size);

  ImGuiID centerNode = dockspaceId;
  ImGuiID leftNode = 0;
  ImGuiID rightNode = 0;
  ImGuiID rightMiddleNode = 0;
  ImGuiID rightBottomNode = 0;
  ImGuiID bottomNode = 0;

  const DefaultDockLayoutFractions fractions = defaultDockLayoutFractions(geometry.size);

  ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Left, fractions.leftPanel, &leftNode, &centerNode);
  ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Right, fractions.rightPanel, &rightNode, &centerNode);
  ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Down, fractions.inspector, &bottomNode, &centerNode);
  ImGui::DockBuilderSplitNode(rightNode, ImGuiDir_Down, 1.0f / 3.0f, &rightBottomNode, &rightNode);
  ImGui::DockBuilderSplitNode(rightNode, ImGuiDir_Down, 0.5f, &rightMiddleNode, &rightNode);

  ImGui::DockBuilderDockWindow("Images##Images", leftNode);
  ImGui::DockBuilderDockWindow("Segmentations##Segmentations", leftNode);
  ImGui::DockBuilderDockWindow("Segmentation Label Region Statistics##RegionStatistics", bottomNode);

  ImGui::DockBuilderDockWindow("Annotations", rightNode);
  ImGui::DockBuilderDockWindow("Landmarks", rightMiddleNode);
  ImGui::DockBuilderDockWindow("Isosurfaces", rightBottomNode);

  ImGui::DockBuilderDockWindow("Voxel Inspector##InspectionWindow", bottomNode);
  ImGui::DockBuilderFinish(dockspaceId);
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
    case LayoutKind::Custom:
    case LayoutKind::NumElements:
      return displayName;
    case LayoutKind::Lightbox:
      return "Lightbox";
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
  const WindowData& windowData = appData.windowData();
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
    if (appData.renderSettings().m_synchronizeThreeDCameras) {
      windowData.synchronizeCurrentLayoutThreeDCameras();
    }
  }
  windowData.removeLayout(index);
}

void renderConfirmRemoveLayoutPopup(AppData& appData)
{
  const char* const popupTitle = "Remove Layout?";
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

  // cppcheck-suppress threadsafety-threadsafety

  static thread_local std::optional<uuids::uuid> lastSyncedSelectedLayoutUid;

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
        // ImGui::BeginTabItem can set tabOpen to false when its close button is clicked.
        // cppcheck-suppress knownConditionTrueFalse
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
            requestedLayoutIndex = selectedLayoutIndex;
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
      if (appData.renderSettings().m_synchronizeThreeDCameras) {
        windowData.synchronizeCurrentLayoutThreeDCameras();
      }
    }
    lastSyncedSelectedLayoutUid = windowData.layouts().at(windowData.currentLayoutIndex()).uid();
  }

  ImGui::End();
  ImGui::PopStyleColor(1);
  ImGui::PopStyleVar(5);
}

} // namespace ui::imgui_detail
