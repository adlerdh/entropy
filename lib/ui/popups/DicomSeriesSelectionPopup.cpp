#include "ui/popups/Popups.h"
#include "ui/popups/PopupCommon.h"
#include "ui/AboutIcon.h"
#include "ui/DicomMetadataTable.h"
#include "ui/Helpers.h"
#include "ui/NativeFileDialogs.h"
#include "ui/ThirdPartyLicenses.h"

#include "image/Image.h"

#include "logic/app/AppPaths.h"
#include "logic/app/Data.h"

#include "BuildStamp.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <spdlog/fmt/std.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <sstream>

namespace fs = std::filesystem;
using namespace entropy::ui::popups;

namespace
{
std::string formatVec3(const glm::vec3& value)
{
  std::ostringstream ss;
  ss << value.x << ", " << value.y << ", " << value.z;
  return ss.str();
}

std::string formatUVec3(const glm::uvec3& value)
{
  return std::to_string(value.x) + " x " + std::to_string(value.y) + " x " + std::to_string(value.z);
}

std::string formatMat3(const glm::mat3& value)
{
  return formatVec3(value[0]) + " | " + formatVec3(value[1]) + " | " + formatVec3(value[2]);
}

glm::vec3 fieldOfViewMm(const dicom::SeriesGeometry& geometry)
{
  return glm::vec3{geometry.dimensions} * geometry.spacing;
}

std::string joinWarnings(const std::vector<std::string>& warnings)
{
  std::string result;
  for (const auto& warning : warnings) {
    if (!result.empty()) {
      result += "; ";
    }
    result += warning;
  }
  return result;
}

void renderSlicePreview(const dicom::SlicePreview& preview, float maxHeight, float maxWidth)
{
  if (preview.empty()) {
    ImGui::TextDisabled("No preview available.");
    return;
  }

  const float scale = std::max(
    0.25f,
    std::min(maxWidth / static_cast<float>(preview.width), maxHeight / static_cast<float>(preview.height)));
  const ImVec2 imageSize(static_cast<float>(preview.width) * scale, static_cast<float>(preview.height) * scale);
  const ImVec2 topLeft = ImGui::GetCursorScreenPos();
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  for (std::uint32_t y = 0; y < preview.height; ++y) {
    for (std::uint32_t x = 0; x < preview.width; ++x) {
      const std::uint8_t value = preview.pixels.at(static_cast<std::size_t>(y) * preview.width + x);
      const ImU32 color = IM_COL32(value, value, value, 255);
      const ImVec2 p0(topLeft.x + static_cast<float>(x) * scale, topLeft.y + static_cast<float>(y) * scale);
      const ImVec2 p1(topLeft.x + static_cast<float>(x + 1) * scale, topLeft.y + static_cast<float>(y + 1) * scale);
      drawList->AddRectFilled(p0, p1, color);
    }
  }

  drawList->AddRect(topLeft, ImVec2(topLeft.x + imageSize.x, topLeft.y + imageSize.y), IM_COL32(120, 120, 120, 255));
  ImGui::InvisibleButton("##dicomPreviewImage", imageSize);
}

void renderDicomPreviewPanel(GuiData::DicomSeriesSelectionPrompt& prompt, float panelHeight)
{
  if (!prompt.previewSeriesIndex || *prompt.previewSeriesIndex >= prompt.series.size()) {
    ImGui::TextDisabled("Select a series row to preview its slices.");
    return;
  }

  const std::size_t index = *prompt.previewSeriesIndex;
  const auto& series = prompt.series.at(index);
  ImGui::Text("Preview: %s", series.displayName.c_str());
  ImGui::SameLine();
  ImGui::TextDisabled(
    "%s",
    series.geometry.sliceOrientation.empty() ? "Unknown orientation" : series.geometry.sliceOrientation.c_str());
  const int maxPreviewSlices = static_cast<int>(
    std::min<std::size_t>(series.files.size(), static_cast<std::size_t>(std::numeric_limits<int>::max())));
  bool previewCountChanged = false;
  if (maxPreviewSlices > 0 && prompt.previewSliceCount > maxPreviewSlices) {
    prompt.previewSliceCount = maxPreviewSlices;
    previewCountChanged = true;
  }
  if (prompt.previewSliceCount < 1) {
    prompt.previewSliceCount = 1;
    previewCountChanged = true;
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(220.0f);
  previewCountChanged =
    ImGui::SliderInt("Slices", &prompt.previewSliceCount, 1, std::max(1, maxPreviewSlices)) || previewCountChanged;

  if (prompt.previewCache.size() != prompt.series.size()) {
    prompt.previewCache.resize(prompt.series.size());
  }
  if (prompt.previewErrors.size() != prompt.series.size()) {
    prompt.previewErrors.resize(prompt.series.size());
  }

  if (previewCountChanged && index < prompt.previewCache.size()) {
    prompt.previewCache.at(index).clear();
    prompt.previewErrors.at(index).clear();
  }

  if (prompt.previewCache.at(index).empty() && prompt.previewErrors.at(index).empty()) {
    prompt.previewCache.at(index) =
      dicom::loadSlicePreviews(series, 128, static_cast<std::size_t>(prompt.previewSliceCount));
    if (prompt.previewCache.at(index).empty()) {
      prompt.previewErrors.at(index) = "Unable to load preview.";
    }
  }

  if (!prompt.previewCache.at(index).empty()) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float headerHeight = ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.y;
    const float availableStripHeight = std::max(80.0f, panelHeight - headerHeight - style.ItemSpacing.y);
    const float labelHeight = ImGui::GetTextLineHeightWithSpacing();
    const float childPaddingY = 2.0f * style.WindowPadding.y;
    const float horizontalScrollbarHeight = style.ScrollbarSize;
    const float imageMaxHeight = std::max(
      48.0f,
      availableStripHeight - labelHeight - childPaddingY - horizontalScrollbarHeight - style.ItemSpacing.y);
    const float stripHeight =
      labelHeight + imageMaxHeight + childPaddingY + horizontalScrollbarHeight + style.ItemSpacing.y;
    ImGui::BeginChild("##dicomPreviewSlices", ImVec2(0.0f, stripHeight), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (std::size_t i = 0; i < prompt.previewCache.at(index).size(); ++i) {
      if (i > 0) {
        ImGui::SameLine();
      }
      const auto& preview = prompt.previewCache.at(index).at(i);
      ImGui::PushID(static_cast<int>(i));
      ImGui::BeginGroup();
      ImGui::TextDisabled("%zu/%zu", preview.sliceIndex + 1, series.files.size());
      const float imageMaxWidth =
        imageMaxHeight * static_cast<float>(preview.width) / static_cast<float>(preview.height);
      renderSlicePreview(preview, imageMaxHeight, imageMaxWidth);
      ImGui::EndGroup();
      ImGui::PopID();
    }
    ImGui::EndChild();
  }
  else {
    ImGui::TextDisabled("%s", prompt.previewErrors.at(index).c_str());
  }
}

} // namespace

void renderDicomSeriesSelectionPopup(
  AppData& appData,
  const std::function<void(
    const std::vector<dicom::SeriesInfo>& series,
    std::optional<std::size_t> referenceSeriesIndex,
    bool addToExistingProject)>& loadSelectedSeries)
{
  constexpr const char* popupTitle = "Load DICOM Series";
  auto& guiData = appData.guiData();

  if (guiData.m_dicomSeriesScanInProgress) {
    ImGui::OpenPopup("Scanning DICOM Series", ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize);
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
  ImGui::SetNextWindowSize(scaledSize(760.0f, 130.0f), ImGuiCond_Appearing);
  ImGui::SetNextWindowSizeConstraints(scaledSize(640.0f, 120.0f), ImVec2(FLT_MAX, FLT_MAX));
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal("Scanning DICOM Series", nullptr, ImGuiWindowFlags_Modal)) {
    if (!guiData.m_dicomSeriesScanInProgress) {
      ImGui::CloseCurrentPopup();
    }
    else {
      ImGui::Text("Scanning DICOM series...");
      if (!guiData.m_pendingDicomScanRoot.empty()) {
        ImGui::PushTextWrapPos(
          ImGui::GetCursorPosX() + std::max(scaledPixel(620.0f), ImGui::GetContentRegionAvail().x));
        ImGui::TextWrapped("%s", guiData.m_pendingDicomScanRoot.string().c_str());
        ImGui::PopTextWrapPos();
      }
    }
    ImGui::EndPopup();
  }

  if (guiData.m_showDicomSeriesSelectionPopup && !ImGui::IsPopupOpen(popupTitle)) {
    ImGui::OpenPopup(popupTitle, ImGuiWindowFlags_Modal);
  }

  ImGui::SetNextWindowSize(ImVec2(1480.0f, 680.0f), ImGuiCond_Appearing);
  ImGui::SetNextWindowSizeConstraints(ImVec2(980.0f, 520.0f), ImVec2(FLT_MAX, FLT_MAX));
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  bool dicomSeriesDialogOpen = true;
  if (ImGui::BeginPopupModal(popupTitle, &dicomSeriesDialogOpen, ImGuiWindowFlags_Modal)) {
    if (!dicomSeriesDialogOpen) {
      guiData.m_pendingDicomSeriesSelectionPrompt = std::nullopt;
      guiData.m_showDicomSeriesSelectionPopup = false;
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
      return;
    }
    if (!guiData.m_pendingDicomSeriesSelectionPrompt) {
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
      return;
    }

    auto& prompt = *guiData.m_pendingDicomSeriesSelectionPrompt;
    const bool hasReference = appData.refImageUid().has_value();
    const bool allowReferenceColumn = prompt.allowReferenceSelection && !hasReference;

    ImGui::Text("Select DICOM series to load.");
    if (!prompt.allowReferenceSelection || hasReference) {
      ImGui::SameLine();
      ImGui::TextDisabled("Reference image is already set.");
    }

    for (const auto& warning : prompt.warnings) {
      ImGui::TextWrapped("Warning: %s", warning.c_str());
    }

    ImGui::Separator();

    constexpr std::array<const char*, 16> kSeriesColumnNames{
      "Load",
      "Reference",
      "Description",
      "Modality",
      "Series No.",
      "Slices",
      "Orientation",
      "Dimensions",
      "Spacing (mm)",
      "Field of View (mm)",
      "Origin",
      "Direction",
      "Series UID",
      "Study UID",
      "Metadata",
      "Warnings"};
    constexpr std::array<bool, 16> kSeriesColumnNoHide{
      true,
      true,
      false,
      false,
      false,
      false,
      false,
      false,
      false,
      false,
      false,
      false,
      false,
      false,
      true,
      false};

    if (ImGui::Button("Columns...")) {
      ImGui::OpenPopup("DICOM Series Columns");
    }
    if (ImGui::BeginPopup("DICOM Series Columns")) {
      for (std::size_t column = 0; column < prompt.seriesColumnVisible.size(); ++column) {
        if (column == 1 && !allowReferenceColumn) {
          continue;
        }
        if (kSeriesColumnNoHide.at(column)) {
          ImGui::BeginDisabled();
        }
        bool visible = prompt.seriesColumnVisible.at(column);
        if (ImGui::Checkbox(kSeriesColumnNames.at(column), &visible) && !kSeriesColumnNoHide.at(column)) {
          prompt.seriesColumnVisible.at(column) = visible;
        }
        if (kSeriesColumnNoHide.at(column)) {
          ImGui::EndDisabled();
        }
      }
      ImGui::EndPopup();
    }
    const float controlsHeight = 2.0f * ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    const float previewHeight = std::max(230.0f, ImGui::GetContentRegionAvail().y * 0.36f);
    const float footerHeight = controlsHeight + previewHeight + 3.0f * ImGui::GetStyle().ItemSpacing.y;
    const float tableHeight = std::max(240.0f, ImGui::GetContentRegionAvail().y - footerHeight);

    if (ImGui::BeginTable(
          "DicomSeriesTableV3",
          16,
          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
            ImGuiTableFlags_Hideable | ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_ScrollX |
            ImGuiTableFlags_ScrollY,
          ImVec2(0.0f, tableHeight)))
    {
      ImGui::TableSetupColumn("Load", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 52.0f);
      ImGui::TableSetupColumn("Reference", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 92.0f);
      ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthFixed, 560.0f);
      ImGui::TableSetupColumn("Modality", ImGuiTableColumnFlags_WidthFixed, 84.0f);
      ImGui::TableSetupColumn("Series No.", ImGuiTableColumnFlags_WidthFixed, 82.0f);
      ImGui::TableSetupColumn("Slices", ImGuiTableColumnFlags_WidthFixed, 70.0f);
      ImGui::TableSetupColumn("Orientation", ImGuiTableColumnFlags_WidthFixed, 120.0f);
      ImGui::TableSetupColumn("Dimensions", ImGuiTableColumnFlags_WidthFixed, 128.0f);
      ImGui::TableSetupColumn("Spacing (mm)", ImGuiTableColumnFlags_WidthFixed, 172.0f);
      ImGui::TableSetupColumn("Field of View (mm)", ImGuiTableColumnFlags_WidthFixed, 172.0f);
      ImGui::TableSetupColumn("Origin", ImGuiTableColumnFlags_DefaultHide | ImGuiTableColumnFlags_WidthFixed, 172.0f);
      ImGui::TableSetupColumn(
        "Direction",
        ImGuiTableColumnFlags_DefaultHide | ImGuiTableColumnFlags_WidthFixed,
        320.0f);
      ImGui::TableSetupColumn("Series UID", ImGuiTableColumnFlags_WidthFixed, 560.0f);
      ImGui::TableSetupColumn("Study UID", ImGuiTableColumnFlags_WidthFixed, 520.0f);
      ImGui::TableSetupColumn("Metadata", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 92.0f);
      ImGui::TableSetupColumn("Warnings", ImGuiTableColumnFlags_WidthFixed, 320.0f);

      for (std::size_t column = 0; column < prompt.seriesColumnVisible.size(); ++column) {
        const bool columnVisible = prompt.seriesColumnVisible.at(column) && (column != 1 || allowReferenceColumn);
        ImGui::TableSetColumnEnabled(static_cast<int>(column), columnVisible);
      }

      ImGui::TableHeadersRow();

      for (std::size_t i = 0; i < prompt.series.size(); ++i) {
        const auto& series = prompt.series.at(i);
        const bool loadable = series.loadable();
        ImGui::PushID(static_cast<int>(i));
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (!loadable) {
          ImGui::BeginDisabled();
        }
        bool selected = i < prompt.selected.size() && prompt.selected.at(i);
        if (ImGui::Checkbox("##load", &selected) && i < prompt.selected.size()) {
          prompt.selected.at(i) = selected;
          prompt.previewSeriesIndex = i;
          if (selected && prompt.referenceSeriesIndex < 0) {
            prompt.referenceSeriesIndex = static_cast<int>(i);
          }
        }
        if (!loadable) {
          ImGui::EndDisabled();
        }

        ImGui::TableSetColumnIndex(1);
        const bool referenceEnabled = prompt.allowReferenceSelection && selected && loadable;
        if (!referenceEnabled) {
          ImGui::BeginDisabled();
        }
        if (ImGui::RadioButton("##ref", &prompt.referenceSeriesIndex, static_cast<int>(i))) {
          prompt.previewSeriesIndex = i;
        }
        if (!referenceEnabled) {
          ImGui::EndDisabled();
        }

        ImGui::TableSetColumnIndex(2);
        if (ImGui::Selectable(
              series.displayName.c_str(),
              prompt.previewSeriesIndex && *prompt.previewSeriesIndex == i,
              ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
        {
          prompt.previewSeriesIndex = i;
        }
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted(series.metadata.modality.c_str());
        ImGui::TableSetColumnIndex(4);
        ImGui::TextUnformatted(series.metadata.seriesNumber.c_str());
        ImGui::TableSetColumnIndex(5);
        ImGui::Text("%zu", series.files.size());
        ImGui::TableSetColumnIndex(6);
        ImGui::TextUnformatted(series.geometry.sliceOrientation.c_str());
        ImGui::TableSetColumnIndex(7);
        const std::string dims = formatUVec3(series.geometry.dimensions);
        ImGui::TextUnformatted(dims.c_str());
        ImGui::TableSetColumnIndex(8);
        const std::string spacing = formatVec3(series.geometry.spacing);
        ImGui::TextUnformatted(spacing.c_str());
        ImGui::TableSetColumnIndex(9);
        const std::string fov = formatVec3(fieldOfViewMm(series.geometry));
        ImGui::TextUnformatted(fov.c_str());
        ImGui::TableSetColumnIndex(10);
        const std::string origin = formatVec3(series.geometry.origin);
        ImGui::TextUnformatted(origin.c_str());
        ImGui::TableSetColumnIndex(11);
        const std::string direction = formatMat3(series.geometry.directions);
        ImGui::TextWrapped("%s", direction.c_str());
        ImGui::TableSetColumnIndex(12);
        ImGui::TextUnformatted(series.seriesInstanceUid.c_str());
        ImGui::TableSetColumnIndex(13);
        ImGui::TextUnformatted(series.metadata.studyInstanceUid.c_str());
        ImGui::TableSetColumnIndex(14);
        if (ImGui::SmallButton("View...")) {
          prompt.metadataSeriesIndex = i;
        }
        ImGui::TableSetColumnIndex(15);
        const std::string warnings = joinWarnings(series.warnings);
        if (!warnings.empty()) {
          ImGui::TextWrapped("%s", warnings.c_str());
        }
        ImGui::PopID();
      }
      ImGui::EndTable();
    }

    ImGui::BeginChild("##dicomPreviewPanel", ImVec2(0.0f, previewHeight), true);
    renderDicomPreviewPanel(prompt, previewHeight);
    ImGui::EndChild();

    if (prompt.metadataSeriesIndex && *prompt.metadataSeriesIndex < prompt.series.size()) {
      ImGui::OpenPopup("DICOM Metadata");
    }

    ImGui::SetNextWindowSize(ImVec2(1100.0f, 620.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(760.0f, 420.0f), ImVec2(FLT_MAX, FLT_MAX));
    bool dicomMetadataDialogOpen = true;
    if (ImGui::BeginPopupModal("DICOM Metadata", &dicomMetadataDialogOpen, ImGuiWindowFlags_Modal)) {
      if (!dicomMetadataDialogOpen) {
        prompt.metadataSeriesIndex = std::nullopt;
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
      }
      if (!prompt.metadataSeriesIndex || *prompt.metadataSeriesIndex >= prompt.series.size()) {
        ImGui::CloseCurrentPopup();
      }
      else {
        const auto& series = prompt.series.at(*prompt.metadataSeriesIndex);
        ImGui::TextWrapped("%s", series.displayName.c_str());
        ImGui::TextWrapped("Study UID: %s", series.metadata.studyInstanceUid.c_str());
        ImGui::TextWrapped("Series UID: %s", series.seriesInstanceUid.c_str());
        ImGui::Text("Slices: %zu", series.files.size());
        ImGui::Separator();

        const float metadataFooterHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        const ImVec2 metadataTableSize(
          ImGui::GetContentRegionAvail().x,
          std::max(220.0f, ImGui::GetContentRegionAvail().y - metadataFooterHeight));

        renderDicomMetadataTable("DicomMetadataTable", series.metadataSummary, metadataTableSize);

        if (ImGui::Button("Close")) {
          prompt.metadataSeriesIndex = std::nullopt;
          ImGui::CloseCurrentPopup();
        }
      }
      ImGui::EndPopup();
    }

    const float footerPadding = ImGui::GetContentRegionAvail().y - controlsHeight;
    if (footerPadding > 0.0f) {
      ImGui::Dummy(ImVec2(0.0f, footerPadding));
    }

    std::size_t selectedCount = 0;
    for (bool selected : prompt.selected) {
      if (selected) {
        ++selectedCount;
      }
    }

    if (ImGui::Button("Select All Loadable")) {
      for (std::size_t i = 0; i < prompt.series.size() && i < prompt.selected.size(); ++i) {
        prompt.selected.at(i) = prompt.series.at(i).loadable();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Selection")) {
      std::fill(prompt.selected.begin(), prompt.selected.end(), false);
    }

    ImGui::SameLine();
    ImGui::TextDisabled("%zu selected", selectedCount);
    ImGui::Separator();

    const bool canLoad = selectedCount > 0;
    if (!canLoad) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("Load")) {
      std::vector<dicom::SeriesInfo> selectedSeries;
      selectedSeries.reserve(selectedCount);
      std::optional<std::size_t> referenceIndex;
      for (std::size_t i = 0; i < prompt.series.size() && i < prompt.selected.size(); ++i) {
        if (!prompt.selected.at(i)) {
          continue;
        }
        if (prompt.allowReferenceSelection && prompt.referenceSeriesIndex == static_cast<int>(i)) {
          referenceIndex = selectedSeries.size();
        }
        selectedSeries.push_back(prompt.series.at(i));
      }
      const bool addToExistingProject = prompt.addToExistingProject;
      guiData.m_pendingDicomSeriesSelectionPrompt = std::nullopt;
      guiData.m_showDicomSeriesSelectionPopup = false;
      ImGui::CloseCurrentPopup();
      if (loadSelectedSeries) {
        loadSelectedSeries(selectedSeries, referenceIndex, addToExistingProject);
      }
    }
    if (!canLoad) {
      ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      guiData.m_pendingDicomSeriesSelectionPrompt = std::nullopt;
      guiData.m_showDicomSeriesSelectionPopup = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  guiData.m_showDicomSeriesSelectionPopup = false;
}
