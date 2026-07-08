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
#include <imgui/imgui_internal.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <spdlog/fmt/std.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <numeric>
#include <optional>
#include <sstream>
#include <string_view>

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

int compareStringsCaseInsensitive(std::string_view left, std::string_view right)
{
  const auto leftIt = left.begin();
  const auto rightIt = right.begin();
  const auto minSize = std::min(left.size(), right.size());
  for (std::size_t i = 0; i < minSize; ++i) {
    const char leftChar = static_cast<char>(std::tolower(static_cast<unsigned char>(leftIt[i])));
    const char rightChar = static_cast<char>(std::tolower(static_cast<unsigned char>(rightIt[i])));
    if (leftChar < rightChar) {
      return -1;
    }
    if (rightChar < leftChar) {
      return 1;
    }
  }
  if (left.size() < right.size()) {
    return -1;
  }
  if (right.size() < left.size()) {
    return 1;
  }
  return 0;
}

template<typename T>
int compareValues(const T& left, const T& right)
{
  if (left < right) {
    return -1;
  }
  if (right < left) {
    return 1;
  }
  return 0;
}

int compareVec3(const glm::vec3& left, const glm::vec3& right)
{
  if (int result = compareValues(left.x, right.x); result != 0) {
    return result;
  }
  if (int result = compareValues(left.y, right.y); result != 0) {
    return result;
  }
  return compareValues(left.z, right.z);
}

int compareUVec3(const glm::uvec3& left, const glm::uvec3& right)
{
  if (int result = compareValues(left.x, right.x); result != 0) {
    return result;
  }
  if (int result = compareValues(left.y, right.y); result != 0) {
    return result;
  }
  return compareValues(left.z, right.z);
}

std::optional<int> parseInt(std::string_view text)
{
  try {
    std::size_t parsedChars = 0;
    const int value = std::stoi(std::string{text}, &parsedChars);
    return parsedChars == text.size() ? std::optional<int>{value} : std::nullopt;
  }
  catch (...) {
    return std::nullopt;
  }
}

int compareSeriesNumber(std::string_view left, std::string_view right)
{
  const std::optional<int> leftNumber = parseInt(left);
  const std::optional<int> rightNumber = parseInt(right);
  if (leftNumber && rightNumber) {
    return compareValues(*leftNumber, *rightNumber);
  }
  if (leftNumber != rightNumber) {
    return leftNumber ? -1 : 1;
  }
  return compareStringsCaseInsensitive(left, right);
}

int compareDicomSeriesRows(
  const GuiData::DicomSeriesSelectionPrompt& prompt,
  std::size_t leftIndex,
  std::size_t rightIndex,
  int columnIndex)
{
  const auto& left = prompt.series.at(leftIndex);
  const auto& right = prompt.series.at(rightIndex);

  switch (columnIndex) {
    case 0:
      return compareValues(
        leftIndex < prompt.selected.size() && prompt.selected.at(leftIndex),
        rightIndex < prompt.selected.size() && prompt.selected.at(rightIndex));
    case 1:
      return compareValues(
        prompt.referenceSeriesIndex == static_cast<int>(leftIndex),
        prompt.referenceSeriesIndex == static_cast<int>(rightIndex));
    case 2:
      return compareStringsCaseInsensitive(left.displayName, right.displayName);
    case 3:
      return compareStringsCaseInsensitive(left.metadata.modality, right.metadata.modality);
    case 4:
      return compareSeriesNumber(left.metadata.seriesNumber, right.metadata.seriesNumber);
    case 5:
      return compareValues(left.files.size(), right.files.size());
    case 6:
      return compareStringsCaseInsensitive(left.geometry.sliceOrientation, right.geometry.sliceOrientation);
    case 7:
      return compareUVec3(left.geometry.dimensions, right.geometry.dimensions);
    case 8:
      return compareVec3(left.geometry.spacing, right.geometry.spacing);
    case 9:
      return compareVec3(fieldOfViewMm(left.geometry), fieldOfViewMm(right.geometry));
    case 10:
      return compareVec3(left.geometry.origin, right.geometry.origin);
    case 11:
      return compareStringsCaseInsensitive(formatMat3(left.geometry.directions), formatMat3(right.geometry.directions));
    case 12:
      return compareStringsCaseInsensitive(left.seriesInstanceUid, right.seriesInstanceUid);
    case 13:
      return compareStringsCaseInsensitive(left.metadata.studyInstanceUid, right.metadata.studyInstanceUid);
    case 15:
      return compareStringsCaseInsensitive(joinWarnings(left.warnings), joinWarnings(right.warnings));
    default:
      return 0;
  }
}

float autoWidthForText(std::string_view text)
{
  return ImGui::CalcTextSize(text.data(), text.data() + text.size()).x + 2.0f * ImGui::GetStyle().CellPadding.x + 8.0f;
}

float clampColumnWidth(float width, float minWidth, float maxWidth)
{
  return std::clamp(width, minWidth, maxWidth);
}

float dicomSeriesColumnAutoWidth(
  const GuiData::DicomSeriesSelectionPrompt& prompt,
  int columnIndex,
  std::string_view header)
{
  float width = autoWidthForText(header);
  auto addText = [&](std::string_view text) {
    width = std::max(width, autoWidthForText(text));
  };

  for (std::size_t i = 0; i < prompt.series.size(); ++i) {
    const auto& series = prompt.series.at(i);
    switch (columnIndex) {
      case 0:
        width = std::max(width, ImGui::GetFrameHeight() + 2.0f * ImGui::GetStyle().CellPadding.x + 12.0f);
        break;
      case 1:
        width = std::max(width, ImGui::GetFrameHeight() + 2.0f * ImGui::GetStyle().CellPadding.x + 12.0f);
        break;
      case 2:
        addText(series.displayName);
        break;
      case 3:
        addText(series.metadata.modality);
        break;
      case 4:
        addText(series.metadata.seriesNumber);
        break;
      case 5:
        addText(std::to_string(series.files.size()));
        break;
      case 6:
        addText(series.geometry.sliceOrientation);
        break;
      case 7:
        addText(formatUVec3(series.geometry.dimensions));
        break;
      case 8:
        addText(formatVec3(series.geometry.spacing));
        break;
      case 9:
        addText(formatVec3(fieldOfViewMm(series.geometry)));
        break;
      case 10:
        addText(formatVec3(series.geometry.origin));
        break;
      case 11:
        addText(formatMat3(series.geometry.directions));
        break;
      case 12:
        addText(series.seriesInstanceUid);
        break;
      case 13:
        addText(series.metadata.studyInstanceUid);
        break;
      case 14:
        addText("View...");
        break;
      case 15:
        addText(joinWarnings(series.warnings));
        break;
      default:
        break;
    }
  }

  switch (columnIndex) {
    case 0:
      return clampColumnWidth(width, 52.0f, 72.0f);
    case 1:
      return clampColumnWidth(width, 92.0f, 112.0f);
    case 2:
      return clampColumnWidth(width, 220.0f, 620.0f);
    case 3:
      return clampColumnWidth(width, 72.0f, 120.0f);
    case 4:
      return clampColumnWidth(width, 82.0f, 120.0f);
    case 5:
      return clampColumnWidth(width, 64.0f, 96.0f);
    case 6:
      return clampColumnWidth(width, 100.0f, 160.0f);
    case 7:
      return clampColumnWidth(width, 116.0f, 180.0f);
    case 8:
    case 9:
    case 10:
      return clampColumnWidth(width, 140.0f, 220.0f);
    case 11:
      return clampColumnWidth(width, 220.0f, 420.0f);
    case 12:
    case 13:
      return clampColumnWidth(width, 260.0f, 620.0f);
    case 14:
      return clampColumnWidth(width, 92.0f, 116.0f);
    case 15:
      return clampColumnWidth(width, 180.0f, 420.0f);
    default:
      return width;
  }
}

std::vector<std::size_t> sortedDicomSeriesRowIndices(
  const GuiData::DicomSeriesSelectionPrompt& prompt,
  const ImGuiTableSortSpecs* sortSpecs)
{
  std::vector<std::size_t> rowIndices(prompt.series.size());
  std::iota(rowIndices.begin(), rowIndices.end(), std::size_t{0});

  if (!sortSpecs || sortSpecs->SpecsCount == 0 || rowIndices.size() < 2) {
    return rowIndices;
  }

  std::stable_sort(
    rowIndices.begin(),
    rowIndices.end(),
    [prompt = &prompt, sortSpecs](std::size_t left, std::size_t right) {
      for (int specIndex = 0; specIndex < sortSpecs->SpecsCount; ++specIndex) {
        const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[specIndex];
        const int result = compareDicomSeriesRows(*prompt, left, right, spec.ColumnIndex);
        if (result == 0) {
          continue;
        }
        return spec.SortDirection == ImGuiSortDirection_Descending ? result > 0 : result < 0;
      }
      return left < right;
    });

  return rowIndices;
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
  const ImVec2 minScanningSize = scaledSize(640.0f, 120.0f);
  setNextWindowSizeConstraintsToMainViewport(minScanningSize.x, minScanningSize.y);
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
  setNextWindowSizeConstraintsToMainViewport(980.0f, 520.0f);
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
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti,
          ImVec2(0.0f, tableHeight)))
    {
      ImGui::TableSetupColumn("Load", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 52.0f);
      ImGui::TableSetupColumn("Reference", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 92.0f);
      ImGui::TableSetupColumn(
        "Description",
        ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort |
          ImGuiTableColumnFlags_PreferSortAscending,
        560.0f);
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

      if (prompt.autoSizeColumnsOnNextRender) {
        for (std::size_t column = 0; column < kSeriesColumnNames.size(); ++column) {
          const bool columnVisible = prompt.seriesColumnVisible.at(column) && (column != 1 || allowReferenceColumn);
          if (!columnVisible) {
            continue;
          }
          ImGui::TableSetColumnWidth(
            static_cast<int>(column),
            dicomSeriesColumnAutoWidth(prompt, static_cast<int>(column), kSeriesColumnNames.at(column)));
        }
        prompt.autoSizeColumnsOnNextRender = false;
      }

      ImGui::TableHeadersRow();
      const std::vector<std::size_t> rowIndices = sortedDicomSeriesRowIndices(prompt, ImGui::TableGetSortSpecs());

      for (const std::size_t i : rowIndices) {
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
    setNextWindowSizeConstraintsToMainViewport(760.0f, 420.0f);
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
        if (series.temporal.isTimeSeries()) {
          ImGui::Text(
            "Time frames: %u (spacing %.6g %s)",
            series.temporal.numTimePoints,
            series.temporal.spacing,
            series.temporal.units.c_str());
        }
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
      if (selectedCount == 1 && prompt.allowReferenceSelection) {
        referenceIndex = 0;
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
