#define IMGUI_DEFINE_MATH_OPERATORS
#include "ui/imgui/Workspace.h"

#include "logic/app/Data.h"
#include "ui/GradientBackgroundRenderer.h"
#include "ui/ImGuiCustomControls.h"
#include "ui/Scaling.h"
#include "ui/windows/LoadingStatusModel.h"

#include <IconsForkAwesome.h>
#include <imgui/imgui_internal.h>

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <format>
#include <functional>
#include <utility>

namespace fs = std::filesystem;

namespace ui::imgui_detail
{
namespace
{
float buttonWidthForLabel(const char* label)
{
  return ImGui::CalcTextSize(label, nullptr, true).x + 2.0f * ImGui::GetStyle().FramePadding.x;
}
} // namespace

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
  const float margin = ui::scaledPixel(12.0f);
  const ImVec2 pos{viewport->WorkPos.x + margin, viewport->WorkPos.y + viewport->WorkSize.y - margin};

  ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2{0.0f, 1.0f});

  constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                                     ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ui::scaledSize(12.0f, 10.0f));

  const std::string windowTitle = (title.empty() ? "Loading Images" : title) + "###LoadingStatus";
  if (ImGui::Begin(windowTitle.c_str(), nullptr, flags)) {
    const int dotCount = static_cast<int>(ImGui::GetTime() * 3.0) % 4;
    std::string text = title.empty() ? "Loading Images" : title;
    text.append(static_cast<std::size_t>(dotCount), '.');
    ImGui::TextUnformatted(text.c_str());

    if (!items.empty()) {
      const auto byteProgress = ui::loading_status_model::loadingProgress(items);
      const float progress = ui::loading_status_model::progressFraction(byteProgress);
      const std::string progressLabel = ui::loading_status_model::progressPercentLabel(progress);
      ImGui::ProgressBar(progress, ImVec2{ui::scaledPixel(320.0f), 0.0f}, progressLabel.c_str());

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

void renderMeshExtractionStatusWindow(const GuiData& guiData)
{
  if (!guiData.m_meshExtractionStatus) {
    return;
  }

  std::string title;
  std::size_t activeJobs = 0;
  std::vector<std::string> descriptions;
  {
    std::scoped_lock lock(guiData.m_meshExtractionStatus->mutex);
    if (!guiData.m_meshExtractionStatus->visible) {
      return;
    }
    title = guiData.m_meshExtractionStatus->title;
    activeJobs = guiData.m_meshExtractionStatus->activeJobs;
    descriptions = guiData.m_meshExtractionStatus->descriptions;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float margin = ui::scaledPixel(12.0f);
  const float width = ui::scaledPixel(360.0f);
  const ImVec2 pos{viewport->WorkPos.x + margin, viewport->WorkPos.y + viewport->WorkSize.y - margin};

  ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2{0.0f, 1.0f});
  ImGui::SetNextWindowSizeConstraints(ImVec2{width, 0.0f}, ImVec2{width, std::numeric_limits<float>::max()});

  constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                                     ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_AlwaysAutoResize;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ui::scaledSize(12.0f, 10.0f));

  const std::string windowTitle = (title.empty() ? "Computing Meshes" : title) + "###MeshExtractionStatus";
  if (ImGui::Begin(windowTitle.c_str(), nullptr, flags)) {
    const int dotCount = static_cast<int>(ImGui::GetTime() * 3.0) % 4;
    if (!descriptions.empty()) {
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
      ImGui::TextUnformatted(descriptions.front().c_str());
      ImGui::PopTextWrapPos();
    }
    std::string status =
      activeJobs == 1 ? "1 extraction in progress" : std::format("{} extractions in progress", activeJobs);
    status.append(static_cast<std::size_t>(dotCount), '.');
    ImGui::TextDisabled("%s", status.c_str());
  }

  ImGui::End();
  ImGui::PopStyleVar();
}

std::string recentPathLabel(const std::vector<fs::path>& paths)
{
  if (paths.empty()) {
    return {};
  }

  const std::string first =
    paths.front().filename().empty() ? paths.front().string() : paths.front().filename().string();
  if (paths.size() == 1) {
    return first;
  }
  return std::format("{}  (+{} more)", first, paths.size() - 1);
}

std::string recentPathTooltip(const std::vector<fs::path>& paths)
{
  std::ostringstream out;
  for (std::size_t i = 0; i < paths.size(); ++i) {
    if (i > 0) {
      out << '\n';
    }
    out << paths[i].string();
  }
  return out.str();
}

bool recentPathsExist(const std::vector<fs::path>& paths)
{
  return !paths.empty() && std::all_of(paths.begin(), paths.end(), [](const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec);
  });
}

template<typename EntryRange, typename PathsForEntry, typename OpenEntry>
void renderRecentEntries(
  const char* title,
  const char* emptyText,
  const EntryRange& entries,
  PathsForEntry pathsForEntry,
  OpenEntry openEntry,
  float rowWidth,
  bool addTopSpacing = false)
{
  if (addTopSpacing) {
    ImGui::Spacing();
  }
  ImGui::SeparatorText(title);
  if (entries.empty()) {
    ImGui::TextDisabled("%s", emptyText);
    return;
  }

  int index = 0;
  for (const auto& entry : entries) {
    const std::vector<fs::path> paths = pathsForEntry(entry);
    const bool pathsExist = recentPathsExist(paths);
    const std::string label =
      std::string{ICON_FK_HISTORY} + " " + recentPathLabel(paths) + "##recent" + title + std::to_string(index++);

    ImGui::BeginDisabled(!pathsExist);
    if (ImGui::Button(label.c_str(), ImVec2{rowWidth, 0.0f})) {
      openEntry(paths);
    }
    ImGui::EndDisabled();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      const std::string tooltip =
        pathsExist ? recentPathTooltip(paths) : "Path not found:\n" + recentPathTooltip(paths);
      ImGui::SetTooltip("%s", tooltip.c_str());
    }
  }
}

void renderEmptyWorkspace(
  ProjectLoadState projectLoadState,
  const AppSettings& settings,
  const std::function<void(const std::vector<fs::path>& fileNames)>& openImageFiles,
  const std::function<void(const std::vector<fs::path>& folderNames)>& openDicomFolders,
  const std::function<void()>& requestDicomFolderPathDialog,
  const std::function<void(const fs::path& fileName)>& openProjectFile,
  const std::function<void()>& clearRecents)
{
  if (ProjectLoadState::Empty != projectLoadState && ProjectLoadState::Failed != projectLoadState) {
    return;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const ImGuiStyle& style = ImGui::GetStyle();
  ui::renderGradientBackground();

  const float panelMargin = ui::scaledPixel(16.0f);
  const ImVec2 windowPadding{panelMargin, ui::scaledPixel(20.0f)};
  const char* text = ProjectLoadState::Failed == projectLoadState ? "Project failed to load" : "No images loaded.";
  const std::array<std::string, 3> buttonLabels{
    std::string{ICON_FK_PICTURE_O} + " Open Image(s)...",
    std::string{ICON_FK_FILES_O} + " Open DICOM Series...",
    std::string{ICON_FK_FOLDER_OPEN_O} + " Open Project..."};

  const float buttonWidth =
    std::accumulate(buttonLabels.begin(), buttonLabels.end(), 0.0f, [](const float current, const std::string& label) {
      return std::max(current, buttonWidthForLabel(label.c_str()));
    });

  const float buttonsWidth = 3.0f * buttonWidth + 2.0f * style.ItemSpacing.x;
  const float recentWidth = ui::scaledPixel(560.0f);
  const float contentWidth = std::max({ImGui::CalcTextSize(text).x, buttonsWidth, recentWidth});
  const bool hasRecents = !settings.recentProjectFiles().empty() || !settings.recentImageGroups().empty() ||
                          !settings.recentDicomGroups().empty();
  std::vector<RecentPathGroup> recentSingleImages;
  std::vector<RecentPathGroup> recentImageGroups;
  for (const RecentPathGroup& group : settings.recentImageGroups()) {
    if (group.paths.size() == 1) {
      recentSingleImages.push_back(group);
    }
    else {
      recentImageGroups.push_back(group);
    }
  }

  const float launcherContentHeight = ImGui::GetTextLineHeight() + style.ItemSpacing.y + ImGui::GetFrameHeight();
  const ImVec2 launcherPanelSize{contentWidth + 2.0f * windowPadding.x, launcherContentHeight + 2.0f * windowPadding.y};
  const float panelGap = ui::scaledPixel(12.0f);

  const auto visibleEntryRows = [](std::size_t entryCount) {
    return static_cast<float>(std::max<std::size_t>(1, entryCount));
  };
  const float recentRows = visibleEntryRows(settings.recentProjectFiles().size()) +
                           visibleEntryRows(recentSingleImages.size()) + visibleEntryRows(recentImageGroups.size()) +
                           visibleEntryRows(settings.recentDicomGroups().size());
  constexpr float recentSectionCount = 4.0f;
  const float recentNaturalContentHeight =
    ImGui::GetTextLineHeightWithSpacing() + style.ItemSpacing.y +
    recentSectionCount * (ImGui::GetTextLineHeightWithSpacing() + style.ItemSpacing.y) +
    recentRows * ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.y + style.SeparatorTextPadding.y +
    ImGui::GetFrameHeightWithSpacing();
  const float recentListContentHeight =
    ImGui::GetTextLineHeightWithSpacing() + style.ItemSpacing.y +
    recentSectionCount * (ImGui::GetTextLineHeightWithSpacing() + style.ItemSpacing.y) +
    recentRows * ImGui::GetFrameHeightWithSpacing();
  const float recentNaturalPanelHeight = recentNaturalContentHeight + 2.0f * windowPadding.y;
  const float recentAvailableHeight = std::max(
    ui::scaledPixel(120.0f),
    viewport->WorkSize.y - launcherPanelSize.y - 2.0f * windowPadding.y - 3.0f * panelGap);
  const float recentMaxHeight = std::min(2.0f * viewport->WorkSize.y / 3.0f, recentAvailableHeight);
  const float recentPanelHeight = hasRecents ? std::min(recentNaturalPanelHeight, recentMaxHeight) : 0.0f;

  const float totalHeight = launcherPanelSize.y + (hasRecents ? panelGap + recentPanelHeight : 0.0f);
  const ImVec2 topLeft{
    viewport->WorkPos.x + 0.5f * (viewport->WorkSize.x - launcherPanelSize.x),
    viewport->WorkPos.y + 0.5f * (viewport->WorkSize.y - totalHeight)};

  ImGui::SetNextWindowPos(topLeft, ImGuiCond_Always);
  ImGui::SetNextWindowSize(launcherPanelSize, ImGuiCond_Always);

  constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, windowPadding);
  if (ImGui::Begin("EmptyWorkspaceLauncher", nullptr, flags)) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (contentWidth - ImGui::CalcTextSize(text).x) * 0.5f));
    ImGui::TextUnformatted(text);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + style.ItemSpacing.y);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (contentWidth - buttonsWidth) * 0.5f));

    if (ImGui::Button(buttonLabels[0].c_str(), ImVec2{buttonWidth, 0.0f})) {
      const auto selectedFiles = native_dialog::openFiles(native_dialog::imageFilters());
      if (!selectedFiles.empty() && openImageFiles) {
        openImageFiles(selectedFiles);
      }
    }

    ImGui::SameLine();

    if (ImGui::Button(buttonLabels[1].c_str(), ImVec2{buttonWidth, 0.0f})) {
      const auto selectedFolders = native_dialog::pickFoldersWithStatus();
      if (!selectedFolders.paths.empty() && openDicomFolders) {
        openDicomFolders(selectedFolders.paths);
      }
      else if (native_dialog::PathDialogStatus::Canceled != selectedFolders.status && requestDicomFolderPathDialog) {
        requestDicomFolderPathDialog();
      }
    }

    ImGui::SameLine();

    if (ImGui::Button(buttonLabels[2].c_str(), ImVec2{buttonWidth, 0.0f})) {
      if (const auto selectedFile = native_dialog::openFile(native_dialog::projectFilters())) {
        if (openProjectFile) {
          openProjectFile(*selectedFile);
        }
      }
    }
  }
  ImGui::End();
  ImGui::PopStyleVar();

  if (hasRecents) {
    const ImVec2 recentPos{topLeft.x, topLeft.y + launcherPanelSize.y + panelGap};
    const ImVec2 recentSize{launcherPanelSize.x, recentPanelHeight};
    ImGui::SetNextWindowPos(recentPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(recentSize, ImGuiCond_Always);

    constexpr ImGuiWindowFlags recentFlags = flags | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, windowPadding);
    if (ImGui::Begin("EmptyWorkspaceRecents", nullptr, recentFlags)) {
      const float footerHeight = style.ItemSpacing.y + ui::scaledPixel(1.0f) + style.ItemSpacing.y +
                                 ImGui::GetFrameHeight() + style.ItemSpacing.y;
      const float listHeight = std::max(0.0f, ImGui::GetContentRegionAvail().y - footerHeight);
      const bool listNeedsScrolling = recentListContentHeight > listHeight + ui::scaledPixel(1.0f);
      const ImGuiWindowFlags listFlags =
        listNeedsScrolling ? ImGuiWindowFlags_None : ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

      if (ImGui::BeginChild("RecentDataList", ImVec2{0.0f, listHeight}, false, listFlags)) {
        ImGui::TextUnformatted("Recent data:");
        ImGui::Spacing();
        const float rowWidth = ImGui::GetContentRegionAvail().x;

        renderRecentEntries(
          "Projects",
          "None",
          settings.recentProjectFiles(),
          [](const fs::path& path) { return std::vector<fs::path>{path}; },
          [&openProjectFile](const std::vector<fs::path>& paths) {
            if (!paths.empty() && openProjectFile) {
              openProjectFile(paths.front());
            }
          },
          rowWidth);

        renderRecentEntries(
          "Images",
          "None",
          recentSingleImages,
          [](const RecentPathGroup& group) { return group.paths; },
          [&openImageFiles](const std::vector<fs::path>& paths) {
            if (!paths.empty() && openImageFiles) {
              openImageFiles(paths);
            }
          },
          rowWidth,
          true);

        renderRecentEntries(
          "Image Groups",
          "None",
          recentImageGroups,
          [](const RecentPathGroup& group) { return group.paths; },
          [&openImageFiles](const std::vector<fs::path>& paths) {
            if (!paths.empty() && openImageFiles) {
              openImageFiles(paths);
            }
          },
          rowWidth,
          true);

        renderRecentEntries(
          "DICOM Series",
          "None",
          settings.recentDicomGroups(),
          [](const RecentPathGroup& group) { return group.paths; },
          [&openDicomFolders](const std::vector<fs::path>& paths) {
            if (!paths.empty() && openDicomFolders) {
              openDicomFolders(paths);
            }
          },
          rowWidth,
          true);
      }
      ImGui::EndChild();

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
      static const std::string clearRecentsButtonLabel = std::string{ICON_FK_ERASER} + " Clear Recents";
      if (
        ImGui::Button(
          clearRecentsButtonLabel.c_str(),
          ImVec2{buttonWidthForLabel(clearRecentsButtonLabel.c_str()), 0.0f}) &&
        clearRecents)
      {
        clearRecents();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Remove all recent projects, images, image groups, and DICOM series from this list");
      }
    }
    ImGui::End();
    ImGui::PopStyleVar();
  }
}

} // namespace ui::imgui_detail
