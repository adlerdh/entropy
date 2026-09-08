#include "ui/windows/RegionStatisticsWindow.h"

#include "ui/Clipboard.h"
#include "ui/GuiData.h"
#include "ui/Helpers.h"
#include "ui/NativeFileDialogs.h"
#include "ui/RegionStatisticsExport.h"
#include "ui/Scaling.h"
#include "ui/windows/RegionStatisticsController.h"

#include "common/ClipboardPayload.h"
#include "image/Image.h"
#include "image/RegionStatistics.h"
#include "logic/app/Data.h"
#include "logic/app/ParcellationLabelTable.h"

#include <imgui/imgui.h>
#include <implot/implot.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
using uuid = uuids::uuid;

std::vector<RegionValueSelection> valueSelections(const Image& image)
{
  const std::uint32_t components = image.header().numComponentsPerPixel();
  std::vector<RegionValueSelection> result;
  result.reserve(components + 5u);
  if (PixelType::Complex == image.header().pixelType() && components >= 2u) {
    result.insert(
      result.end(),
      {{RegionValueKind::ComplexMagnitude, 0},
       {RegionValueKind::ComplexPhase, 0},
       {RegionValueKind::ComplexReal, 0},
       {RegionValueKind::ComplexImaginary, 0}});
  }
  else {
    for (std::uint32_t component = 0; component < components; ++component) {
      result.push_back({RegionValueKind::Component, component});
    }
  }
  if (PixelType::Complex != image.header().pixelType() && components > 1u) {
    result.insert(
      result.end(),
      {{RegionValueKind::Magnitude, 0},
       {RegionValueKind::Minimum, 0},
       {RegionValueKind::Mean, 0},
       {RegionValueKind::Maximum, 0}});
    if (
      (PixelType::RGB == image.header().pixelType() || PixelType::RGBA == image.header().pixelType()) &&
      components >= 3u)
    {
      result.push_back({RegionValueKind::Luminance, 0});
    }
  }
  return result;
}

std::string valueSelectionName(const Image& image, const RegionValueSelection& selection)
{
  if (RegionValueKind::Component != selection.kind) return regionValueSelectionName(selection);
  if (PixelType::RGB == image.header().pixelType() || PixelType::RGBA == image.header().pixelType()) {
    static constexpr const char* names[] = {"Red", "Green", "Blue", "Alpha"};
    if (selection.component < 4u) return names[selection.component];
  }
  return image.header().numComponentsPerPixel() == 1u ? "Intensity" : regionValueSelectionName(selection);
}

const ParcellationLabelTable* labelTable(const AppData& appData, const Image& segmentation)
{
  const auto tableUid = appData.labelTableUid(segmentation.settings().labelTableIndex());
  return tableUid ? appData.labelTable(*tableUid) : nullptr;
}

std::string labelName(const ParcellationLabelTable* table, std::int64_t label)
{
  if (table && label >= 0 && static_cast<std::size_t>(label) < table->numLabels()) return table->getName(label);
  return "Label " + std::to_string(label);
}

RegionStatisticsDocument statisticsDocument(
  const RegionStatisticsController& state,
  const Image& image,
  const Image& segmentation,
  const ParcellationLabelTable* table,
  bool volume)
{
  RegionStatisticsDocument document{
    .imageName = image.settings().displayName(),
    .imageValueName = valueSelectionName(image, state.value),
    .timePoint = image.timeAxis().clamp(image.settings().activeTimePoint()),
    .segmentationName = segmentation.settings().displayName(),
    .isVolume = volume,
    .includesQuartiles = state.computeQuartiles};
  for (const auto& region : state.sortedRegions) {
    if (!state.includeBackground && 0 == region.label) continue;
    document.regions.push_back({.statistic = region, .name = labelName(table, region.label)});
  }
  return document;
}

void writeFile(const std::filesystem::path& path, const std::string& contents, std::string& status)
{
  std::ofstream stream(path, std::ios::binary);
  stream << contents;
  if (stream) {
    status = "Saved " + path.filename().string();
    spdlog::info("Saved segmentation region statistics to {} ({} bytes)", path, contents.size());
  }
  else {
    status = "Could not save " + path.filename().string();
    spdlog::error("Could not save segmentation region statistics to {}", path);
  }
}

void renderSelectionCombo(const char* label, const std::string& preview, const std::function<void()>& contents)
{
  if (ImGui::BeginCombo(label, preview.c_str())) {
    contents();
    ImGui::EndCombo();
  }
}

void sortRows(
  RegionStatisticsController& state,
  const ParcellationLabelTable* table,
  const ImGuiTableSortSpecs* sortSpecs)
{
  if (!sortSpecs || 0 == sortSpecs->SpecsCount) return;
  const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[0];
  const auto less = [&](const RegionStatistic& left, const RegionStatistic& right) {
    switch (spec.ColumnUserID) {
      case 0:
        return left.label < right.label;
      case 1:
        return labelName(table, left.label) < labelName(table, right.label);
      case 2:
        return left.elementCount < right.elementCount;
      case 3:
        return left.physicalSize < right.physicalSize;
      case 4:
        return left.minimum < right.minimum;
      case 5:
        return left.mean < right.mean;
      case 6:
        return left.maximum < right.maximum;
      case 7:
        return left.standardDeviation < right.standardDeviation;
      case 8:
        return left.firstQuartile.value_or(0.0) < right.firstQuartile.value_or(0.0);
      case 9:
        return left.median.value_or(0.0) < right.median.value_or(0.0);
      case 10:
        return left.thirdQuartile.value_or(0.0) < right.thirdQuartile.value_or(0.0);
      default:
        return left.label < right.label;
    }
  };
  std::stable_sort(state.sortedRegions.begin(), state.sortedRegions.end(), [&](const auto& left, const auto& right) {
    return ImGuiSortDirection_Descending == spec.SortDirection ? less(right, left) : less(left, right);
  });
}

std::vector<double> displayedHistogramValues(const RegionHistogram& histogram, bool cumulative, bool density)
{
  std::vector<double> values = histogram.counts;
  if (cumulative) {
    for (std::size_t index = 1; index < values.size(); ++index)
      values[index] += values[index - 1u];
  }
  if (!density || 0 == histogram.finiteValueCount) return values;

  const double binWidth = histogram.binCenters.size() > 1u ? histogram.binCenters[1] - histogram.binCenters[0] : 1.0;
  const double denominator = cumulative ? static_cast<double>(histogram.finiteValueCount)
                                        : static_cast<double>(histogram.finiteValueCount) * binWidth;
  if (denominator > 0.0) {
    for (double& value : values)
      value /= denominator;
  }
  return values;
}

const char* histogramCountAxisLabel(bool cumulative, bool density, bool logarithmic)
{
  if (density) {
    if (cumulative) return logarithmic ? "log(Probability)" : "Probability";
    return logarithmic ? "log(Density)" : "Density";
  }
  return logarithmic ? "log(Count)" : "Count";
}
} // namespace

void renderRegionStatisticsWindow(AppData& appData, RegionStatisticsController& state)
{
  if (appData.guiData().m_requestedRegionStatisticsImageUid) {
    state.requestSelection(
      std::exchange(appData.guiData().m_requestedRegionStatisticsImageUid, std::nullopt),
      std::exchange(appData.guiData().m_requestedRegionStatisticsSegmentationUid, std::nullopt));
  }
  if (!state.imageUid || !appData.image(*state.imageUid)) {
    const auto activeImageUid = appData.activeImageUid();
    state.setImage(activeImageUid, activeImageUid ? appData.imageToActiveSegUid(*activeImageUid) : std::nullopt);
  }
  if (state.imageUid) {
    const auto segmentations = appData.imageToSegUids(*state.imageUid);
    if (
      !state.segmentationUid ||
      std::find(segmentations.begin(), segmentations.end(), *state.segmentationUid) == segmentations.end())
    {
      state.setSegmentation(appData.imageToActiveSegUid(*state.imageUid));
    }
  }

  setNextWindowSizeConstraintsToMainViewport(ui::scaledPixel(560.0f), ui::scaledPixel(360.0f));
  ImGui::SetNextWindowSize(ui::viewportClampedScaledSize(900.0f, 650.0f), ImGuiCond_FirstUseEver);
  setNextDockablePanelWindowClass();
  if (!ImGui::Begin(
        "Segmentation Region Statistics##RegionStatistics",
        &appData.guiData().m_showRegionStatisticsWindow))
  {
    ImGui::End();
    return;
  }

  const Image* image = state.imageUid ? appData.image(*state.imageUid) : nullptr;
  std::string imagePreview = image ? image->settings().displayName() : "No image";
  renderSelectionCombo("Image", imagePreview, [&]() {
    for (const auto& uid : appData.imageUidsOrdered()) {
      const Image* candidate = appData.image(uid);
      if (!candidate) continue;
      const bool selected = state.imageUid && uid == *state.imageUid;
      if (ImGui::Selectable(candidate->settings().displayName().c_str(), selected)) {
        state.setImage(uid, appData.imageToActiveSegUid(uid));
      }
    }
  });

  image = state.imageUid ? appData.image(*state.imageUid) : nullptr;
  const Image* segmentation = state.segmentationUid ? appData.seg(*state.segmentationUid) : nullptr;
  if (image) {
    const auto selections = valueSelections(*image);
    if (std::find(selections.begin(), selections.end(), state.value) == selections.end() && !selections.empty()) {
      state.setValueSelection(selections.front());
    }
    if (selections.size() > 1u) {
      renderSelectionCombo("Image value", valueSelectionName(*image, state.value), [&]() {
        for (const auto& selection : selections) {
          const bool selected = selection == state.value;
          if (ImGui::Selectable(valueSelectionName(*image, selection).c_str(), selected)) {
            state.setValueSelection(selection);
          }
        }
      });
    }

    const auto segmentations = appData.imageToSegUids(*state.imageUid);
    renderSelectionCombo(
      "Segmentation",
      segmentation ? segmentation->settings().displayName() : "No segmentation",
      [&]() {
        for (const auto& uid : segmentations) {
          const Image* candidate = appData.seg(uid);
          if (!candidate) continue;
          const bool selected = state.segmentationUid && uid == *state.segmentationUid;
          if (ImGui::Selectable(candidate->settings().displayName().c_str(), selected)) {
            state.setSegmentation(uid);
          }
        }
      });
  }

  segmentation = state.segmentationUid ? appData.seg(*state.segmentationUid) : nullptr;
  if (!image || !segmentation) {
    ImGui::TextDisabled("Select an image and segmentation to compute statistics.");
    ImGui::End();
    return;
  }

  const std::uint32_t timePoint = image->timeAxis().clamp(image->settings().activeTimePoint());
  const RegionStatisticsCalculationKey key{
    .imageUid = *state.imageUid,
    .segmentationUid = *state.segmentationUid,
    .value = state.value,
    .timePoint = timePoint,
    .imageRevision = image->pixelDataRevision(),
    .segmentationRevision = segmentation->pixelDataRevision(),
    .segmentationGeometryRevision = segmentation->geometryRevision(),
    .computeQuartiles = state.computeQuartiles};
  state.updateStatistics(key, *image, *segmentation, ImGui::IsMouseDown(ImGuiMouseButton_Left));

  const ParcellationLabelTable* table = labelTable(appData, *segmentation);
  if (
    ImGui::Checkbox("Include background (label 0)", &state.includeBackground) && state.includeBackground &&
    !state.selectedLabel && !state.sortedRegions.empty())
  {
    state.selectedLabel = state.sortedRegions.front().label;
  }
  ImGui::SameLine();
  helpMarker("Include the segmentation background as a table row and export record");
  ImGui::Checkbox("Include empty labels", &state.includeEmptyLabels);
  ImGui::SameLine();
  helpMarker("Add every unused label from the segmentation color table with count and volume zero");
  bool computeQuartiles = state.computeQuartiles;
  if (ImGui::Checkbox("Compute quartiles", &computeQuartiles)) state.setComputeQuartiles(computeQuartiles);
  ImGui::SameLine();
  helpMarker("Compute exact 25th, 50th, and 75th percentiles. This requires additional time and temporary memory");
  ImGui::TextDisabled("%s", state.status.c_str());

  if (state.result) {
    const bool rowsChanged = state.synchronizeEmptyLabelRows(table ? table->numLabels() : 0u);
    constexpr float minimumTableHeight = 280.0f;
    constexpr float minimumHistogramHeight = 300.0f;
    const float lineHeight = ImGui::GetFrameHeightWithSpacing();
    const float fixedHeight = 2.0f * lineHeight + 2.0f * ImGui::GetStyle().ItemSpacing.y;
    const float histogramControlHeight =
      state.histogramOpen ? (state.histogramUseCustomRange ? 6.0f : 5.0f) * lineHeight : 0.0f;
    const float availableHeight = ImGui::GetContentRegionAvail().y;
    float tableHeight = std::max(ui::scaledPixel(minimumTableHeight), availableHeight - fixedHeight);
    if (state.histogramOpen) {
      const float extraHeight = std::max(
        0.0f,
        availableHeight - fixedHeight - histogramControlHeight - ui::scaledPixel(minimumTableHeight) -
          ui::scaledPixel(minimumHistogramHeight));
      tableHeight = ui::scaledPixel(minimumTableHeight) + 0.5f * extraHeight;
    }
    constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX |
                                           ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable |
                                           ImGuiTableFlags_Reorderable | ImGuiTableFlags_SizingStretchProp |
                                           ImGuiTableFlags_NoSavedSettings;
    const int columnCount = state.computeQuartiles ? 11 : 8;
    if (ImGui::BeginTable("##RegionStatisticsTable", columnCount, tableFlags, ImVec2(0.0f, tableHeight))) {
      ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_DefaultSort, 0.0f, 0);
      ImGui::TableSetupColumn("Label", 0, 0.0f, 1);
      ImGui::TableSetupColumn("Count", 0, 0.0f, 2);
      ImGui::TableSetupColumn(state.result->isVolume ? "Volume (mm³)" : "Area (mm²)", 0, 0.0f, 3);
      ImGui::TableSetupColumn("Minimum", 0, 0.0f, 4);
      if (state.computeQuartiles) {
        ImGui::TableSetupColumn("Q1 (25%)", 0, 0.0f, 8);
        ImGui::TableSetupColumn("Median (50%)", 0, 0.0f, 9);
        ImGui::TableSetupColumn("Q3 (75%)", 0, 0.0f, 10);
      }
      ImGui::TableSetupColumn("Maximum", 0, 0.0f, 6);
      ImGui::TableSetupColumn("Mean", 0, 0.0f, 5);
      ImGui::TableSetupColumn("Std. deviation", 0, 0.0f, 7);
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
      for (int column = 0; column < columnCount; ++column) {
        ImGui::TableSetColumnIndex(column);
        ImGui::TableHeader(ImGui::TableGetColumnName(column));
        if (ImGui::IsItemHovered()) {
          if (0 == column)
            ImGui::SetTooltip("Label index");
          else if (1 == column)
            ImGui::SetTooltip("Label region");
          else if (2 == column)
            ImGui::SetTooltip("Voxel count");
        }
      }
      if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && (specs->SpecsDirty || rowsChanged)) {
        sortRows(state, table, specs);
        specs->SpecsDirty = false;
      }
      const char* imageValueFormat = appData.guiData().m_imageValuePrecisionFormat.c_str();
      const char* physicalSizeFormat = appData.guiData().m_coordsPrecisionFormat.c_str();
      for (const auto& region : state.sortedRegions) {
        if (!state.includeBackground && 0 == region.label) continue;
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const bool selected = state.selectedLabel == region.label;
        const std::string rowId = std::to_string(region.label) + "##RegionRow";
        if (ImGui::Selectable(rowId.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
          state.selectedLabel = region.label;
        }
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(labelName(table, region.label).c_str());
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%llu", static_cast<unsigned long long>(region.elementCount));
        ImGui::TableSetColumnIndex(3);
        ImGui::Text(physicalSizeFormat, region.physicalSize);
        ImGui::TableSetColumnIndex(4);
        if (region.finiteValueCount)
          ImGui::Text(imageValueFormat, region.minimum);
        else
          ImGui::TextDisabled("N/A");
        if (state.computeQuartiles) {
          ImGui::TableNextColumn();
          if (region.firstQuartile)
            ImGui::Text(imageValueFormat, *region.firstQuartile);
          else
            ImGui::TextDisabled("N/A");
          ImGui::TableNextColumn();
          if (region.median)
            ImGui::Text(imageValueFormat, *region.median);
          else
            ImGui::TextDisabled("N/A");
          ImGui::TableNextColumn();
          if (region.thirdQuartile)
            ImGui::Text(imageValueFormat, *region.thirdQuartile);
          else
            ImGui::TextDisabled("N/A");
        }
        ImGui::TableNextColumn();
        if (region.finiteValueCount)
          ImGui::Text(imageValueFormat, region.maximum);
        else
          ImGui::TextDisabled("N/A");
        ImGui::TableNextColumn();
        if (region.finiteValueCount)
          ImGui::Text(imageValueFormat, region.mean);
        else
          ImGui::TextDisabled("N/A");
        ImGui::TableNextColumn();
        if (region.finiteValueCount)
          ImGui::Text(imageValueFormat, region.standardDeviation);
        else
          ImGui::TextDisabled("N/A");
        if (region.nonFiniteValueCount > 0 && ImGui::IsItemHovered()) {
          ImGui::SetTooltip(
            "%llu non-finite values were excluded",
            static_cast<unsigned long long>(region.nonFiniteValueCount));
        }
      }
      ImGui::EndTable();
    }

    const RegionStatisticsDocument document =
      statisticsDocument(state, *image, *segmentation, table, state.result->isVolume);
    if (ImGui::Button("Copy")) {
      ui::setClipboardPayload({.plainText = regionStatisticsDelimitedText(document, '\t')});
      state.status = "Copied table to clipboard";
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Copy the displayed statistics as tab-separated text");
    ImGui::SameLine();
    if (ImGui::Button("Save CSV...")) {
      if (const auto path = native_dialog::saveFile({{"CSV", "csv"}}, {}, "region-statistics.csv")) {
        writeFile(*path, regionStatisticsDelimitedText(document, ','), state.status);
      }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save the displayed statistics as comma-separated values");
    ImGui::SameLine();
    if (ImGui::Button("Save JSON...")) {
      if (const auto path = native_dialog::saveFile({{"JSON", "json"}}, {}, "region-statistics.json")) {
        writeFile(*path, regionStatisticsJson(document), state.status);
      }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save the displayed statistics and selection metadata as JSON");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    state.histogramOpen = ImGui::CollapsingHeader("Histogram");
    if (state.histogramOpen) {
      if (ImGui::RadioButton("Selected region", !state.histogramWholeImage)) {
        state.histogramWholeImage = false;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("Whole image", state.histogramWholeImage)) {
        state.histogramWholeImage = true;
      }

      const int maximumBins = static_cast<int>(std::max<std::size_t>(
        1u,
        std::min<std::size_t>(image->header().numPixels(), static_cast<std::size_t>(std::numeric_limits<int>::max()))));
      const std::array<double, 2> dataRange = state.histogramDataRange.value_or(std::array<double, 2>{0.0, 1.0});
      const std::array<double, 2> activeRange =
        state.histogramUseCustomRange ? state.histogramValueRange.value_or(dataRange) : dataRange;
      const double rangeWidth = activeRange[1] - activeRange[0];
      float binWidth = static_cast<float>(rangeWidth / static_cast<double>(std::max(1, state.histogramBins)));
      const float binWidthSpeed = std::max(static_cast<float>(rangeWidth / 1000.0), 1.0e-6f);

      const std::optional<std::int64_t> histogramLabel = state.histogramWholeImage ? std::nullopt : state.selectedLabel;
      if (!state.histogramWholeImage && !histogramLabel) {
        ImGui::TextDisabled("Select a table row to display its histogram.");
      }
      else {
        RegionHistogramOptions histogramOptions{.binCount = static_cast<std::size_t>(state.histogramBins)};
        if (state.histogramUseCustomRange) histogramOptions.valueRange = state.histogramValueRange;
        state.updateHistogram(key, *image, *segmentation, histogramLabel, histogramOptions);
        if (state.histogram && !state.histogram->binCenters.empty()) {
          const std::vector<double> displayedValues =
            displayedHistogramValues(*state.histogram, state.histogramCumulative, state.histogramDensity);
          const char* countAxisLabel =
            histogramCountAxisLabel(state.histogramCumulative, state.histogramDensity, state.histogramLogScale);
          const float plotHeight = std::max(
            ui::scaledPixel(minimumHistogramHeight),
            ImGui::GetContentRegionAvail().y - histogramControlHeight);
          if (ImPlot::BeginPlot("##RegionHistogramPlot", ImVec2{-1.0f, plotHeight}, ImPlotFlags_NoLegend)) {
            if (state.histogramHorizontal) {
              ImPlot::SetupAxes(countAxisLabel, "Image value", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
              if (state.histogramLogScale) ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
            }
            else {
              ImPlot::SetupAxes("Image value", countAxisLabel, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
              if (state.histogramLogScale) ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
            }
            const double width = state.histogram->binCenters.size() > 1
                                   ? state.histogram->binCenters[1] - state.histogram->binCenters[0]
                                   : 1.0;
            if (state.histogramHorizontal) {
              ImPlot::PlotBars(
                "##Histogram",
                displayedValues.data(),
                state.histogram->binCenters.data(),
                static_cast<int>(displayedValues.size()),
                width,
                ImPlotBarsFlags_Horizontal);
            }
            else {
              ImPlot::PlotBars(
                "##Histogram",
                state.histogram->binCenters.data(),
                displayedValues.data(),
                static_cast<int>(displayedValues.size()),
                width);
            }
            ImPlot::EndPlot();
          }
        }
      }

      ImGui::DragInt("Bin count", &state.histogramBins, 1.0f, 1, maximumBins);
      if (
        ImGui::DragFloat(
          "Bin width",
          &binWidth,
          binWidthSpeed,
          0.0f,
          static_cast<float>(rangeWidth),
          appData.guiData().m_imageValuePrecisionFormat.c_str()) &&
        binWidth > 0.0f)
      {
        state.histogramBins =
          std::clamp(static_cast<int>(std::ceil(rangeWidth / static_cast<double>(binWidth))), 1, maximumBins);
      }

      ImGui::Checkbox("Cumulative", &state.histogramCumulative);
      ImGui::SameLine();
      ImGui::Checkbox("Density", &state.histogramDensity);
      ImGui::SameLine();
      ImGui::Checkbox("Horizontal", &state.histogramHorizontal);
      ImGui::SameLine();
      ImGui::Checkbox("Log scale", &state.histogramLogScale);
      ImGui::Checkbox("Set intensity range", &state.histogramUseCustomRange);

      if (state.histogramUseCustomRange && state.histogramValueRange) {
        float rangeLow = static_cast<float>((*state.histogramValueRange)[0]);
        float rangeHigh = static_cast<float>((*state.histogramValueRange)[1]);
        const std::string minimumFormat = "Min: " + appData.guiData().m_imageValuePrecisionFormat;
        const std::string maximumFormat = "Max: " + appData.guiData().m_imageValuePrecisionFormat;
        if (ImGui::DragFloatRange2(
              "Intensity range",
              &rangeLow,
              &rangeHigh,
              binWidthSpeed,
              static_cast<float>(dataRange[0]),
              static_cast<float>(dataRange[1]),
              minimumFormat.c_str(),
              maximumFormat.c_str(),
              ImGuiSliderFlags_AlwaysClamp))
        {
          state.histogramValueRange = std::array<double, 2>{rangeLow, rangeHigh};
        }
      }
    }
  }
  ImGui::End();
}
