#include "ui/windows/InspectionWindow.h"

#include "ui/Helpers.h"
#include "ui/headers/HeaderCommon.h"
#include "logic/app/Data.h"

#include "image/Image.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/color_space.hpp>
#include <glm/gtx/component_wise.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cinttypes>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
using uuid = uuids::uuid;

const ImVec4 whiteText(1, 1, 1, 1);
const ImVec4 blackText(0, 0, 0, 1);

enum class InspectorColumn : int
{
  Image = 0,
  Value,
  InterpolatedValue,
  Percentile,
  Label,
  Region,
  Voxel,
  Subject,
  Count
};

constexpr std::size_t sk_inspectorColumnCount = static_cast<std::size_t>(InspectorColumn::Count);

constexpr std::array<const char*, sk_inspectorColumnCount> sk_inspectorColumnNames{
  "Image",
  "Value",
  "Value (interp.)",
  "Percentile",
  "Label",
  "Region",
  "Voxel",
  "Subject (mm)"};

constexpr std::array<float, sk_inspectorColumnCount>
  k_inspectorColumnMinWidths{120.0f, 64.0f, 72.0f, 78.0f, 48.0f, 72.0f, 96.0f, 148.0f};

constexpr std::array<float, sk_inspectorColumnCount>
  k_inspectorColumnDefaultWidths{150.0f, 82.0f, 122.0f, 100.0f, 62.0f, 140.0f, 125.0f, 210.0f};

constexpr std::array<float, sk_inspectorColumnCount>
  k_inspectorColumnMaxWidths{280.0f, 110.0f, 130.0f, 115.0f, 72.0f, 180.0f, 150.0f, 230.0f};

constexpr std::array<bool, sk_inspectorColumnCount>
  sk_inspectorColumnCanHide{false, true, true, true, true, true, true, true};

constexpr int columnIndex(InspectorColumn column)
{
  return static_cast<int>(column);
}

std::optional<double> activeComponentPercentile(const Image& image, const std::vector<double>& imageValuesNN)
{
  const uint32_t activeComponent = image.settings().activeComponent();
  if (activeComponent >= imageValuesNN.size()) {
    return std::nullopt;
  }

  const double value = imageValuesNN.at(activeComponent);
  const QuantileOfValue quantile = isComponentFloatingPoint(image.header().memoryComponentType())
                                     ? image.valueToQuantile(activeComponent, value)
                                     : image.valueToQuantile(activeComponent, static_cast<int64_t>(value));

  const double percentile = 50.0 * (quantile.lowerQuantile + quantile.upperQuantile);
  return std::clamp(percentile, 0.0, 100.0);
}

float inspectionColumnWidthForText(const char* text)
{
  const ImGuiStyle& style = ImGui::GetStyle();
  return ImGui::CalcTextSize(text).x + 2.0f * (style.CellPadding.x + style.FramePadding.x);
}

float clampInspectionColumnWidth(InspectorColumn column, float width)
{
  const std::size_t index = static_cast<std::size_t>(column);
  return std::clamp(width, k_inspectorColumnMinWidths.at(index), k_inspectorColumnMaxWidths.at(index));
}

const char* inspectionColumnTooltip(InspectorColumn column)
{
  switch (column) {
    case InspectorColumn::Value:
      return "Nearest image voxel value";
    case InspectorColumn::InterpolatedValue:
      return "Linearly interpolated image voxel value";
    case InspectorColumn::Percentile:
      return "Image voxel value percentile";
    case InspectorColumn::Label:
      return "Segmentation label value";
    case InspectorColumn::Region:
      return "Segmentation label region name";
    case InspectorColumn::Voxel:
      return "Voxel index (i: column, j: row, k: slice)";
    case InspectorColumn::Subject:
      return "Subject-space LPS coordinates in millimeters (x: R->L, y: A->P, z: I->S)";
    case InspectorColumn::Image:
    case InspectorColumn::Count:
      break;
  }

  return nullptr;
}
} // namespace

void renderInspectionWindowWithTable(
  AppData& appData,
  const std::function<std::pair<std::string, std::string>(std::size_t index)>& getImageDisplayAndFileName,
  const std::function<std::optional<glm::vec3>(std::size_t imageIndex)>& getSubjectPos,
  const std::function<std::optional<glm::ivec3>(std::size_t imageIndex)>& getVoxelPos,
  const std::function<void(std::size_t imageIndex, const glm::vec3& subjectPos)> setSubjectPos,
  const std::function<void(std::size_t imageIndex, const glm::ivec3& voxelPos)> setVoxelPos,
  const std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)>& getImageValuesNN,
  const std::function<std::vector<double>(std::size_t imageIndex, bool getOnlyActiveComponent)>& getImageValuesLinear,
  const std::function<std::optional<int64_t>(std::size_t imageIndex)>& getSegLabel,
  const std::function<ParcellationLabelTable*(std::size_t tableIndex)>& getLabelTable)
{
  static bool s_firstRun = true; // Is this the first run?

  static constexpr float sk_pad = 10.0f;
  static int corner = -1;

  //    bool selectionButtonShown = false;

  static const ImVec2 sk_windowPadding(0.0f, 0.0f);
  static const ImVec2 sk_framePadding(0.0f, 0.0f);
  static const ImVec2 sk_itemInnerSpacing(1.0f, 1.0f);
  static const ImVec2 sk_cellPadding(0.0f, 0.0f);
  static const float sk_windowRounding(0.0f);

  //    static const ImVec4 blueColor( 0.0f, 0.5f, 1.0f, 1.0f );

  static const ImGuiTableFlags sk_tableFlags =
    ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders |
    ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;

  static const ImGuiWindowFlags sk_windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoFocusOnAppearing;

  static bool s_showTitleBar = false;
  static bool s_autoSizeColumnsRequested = false;
  static std::array<bool, sk_inspectorColumnCount> s_autoSizeColumnRequested{};

  // For which images to show coordinates?
  static std::unordered_map<uuid, bool> s_showSubject;

  if (s_firstRun) {
    // Show all images by default:
    for (const auto& imageUid : appData.imageUidsOrdered()) {
      s_showSubject.insert({imageUid, true});
    }

    s_firstRun = false;
  }

  auto renderColumnVisibilityItems = [&appData]() {
    if (ImGui::MenuItem("Auto-size columns")) {
      s_autoSizeColumnsRequested = true;
    }
    ImGui::Separator();

    for (std::size_t column = 0; column < sk_inspectorColumnNames.size(); ++column) {
      bool visible = appData.guiData().m_inspectionColumnVisible.at(column);
      if (!sk_inspectorColumnCanHide.at(column)) {
        ImGui::BeginDisabled();
      }
      if (ImGui::MenuItem(sk_inspectorColumnNames.at(column), nullptr, visible) && sk_inspectorColumnCanHide.at(column))
      {
        const bool newVisible = !visible;
        appData.guiData().m_inspectionColumnVisible.at(column) = newVisible;
        if (newVisible) {
          s_autoSizeColumnRequested.at(column) = true;
        }
      }
      if (!sk_inspectorColumnCanHide.at(column)) {
        ImGui::EndDisabled();
      }
    }
  };

  auto renderColumnVisibilityMenu = [&renderColumnVisibilityItems]() {
    if (ImGui::BeginMenu("Columns")) {
      renderColumnVisibilityItems();
      ImGui::EndMenu();
    }
  };

  auto renderImagesItems = [&appData, &getImageDisplayAndFileName]() {
    for (std::size_t imageIndex = 0; imageIndex < appData.numImages(); ++imageIndex) {
      const auto imageUid = appData.imageUid(imageIndex);
      if (!imageUid) continue;

      auto it = s_showSubject.find(*imageUid);
      if (std::end(s_showSubject) == it) {
        s_showSubject.insert({*imageUid, true});
      }

      bool& visible = s_showSubject[*imageUid];
      const auto names = getImageDisplayAndFileName(imageIndex);

      if (ImGui::MenuItem(names.first.c_str(), nullptr, visible)) {
        visible = !visible;
      }

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", names.second.c_str());
      }
    }
  };

  auto renderImagesMenu = [&renderImagesItems]() {
    if (ImGui::BeginMenu("Images...")) {
      renderImagesItems();
      ImGui::EndMenu();
    }
  };

  auto renderWindowItems = [&appData]() {
    if (ImGui::BeginMenu("Position")) {
      if (ImGui::MenuItem("Custom", nullptr, corner == -1)) corner = -1;
      if (ImGui::MenuItem("Top-left", nullptr, corner == 0)) corner = 0;
      if (ImGui::MenuItem("Top-right", nullptr, corner == 1)) corner = 1;
      if (ImGui::MenuItem("Bottom-left", nullptr, corner == 2)) corner = 2;
      if (ImGui::MenuItem("Bottom-right", nullptr, corner == 3)) corner = 3;
      ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Show title bar", nullptr, s_showTitleBar)) {
      s_showTitleBar = !s_showTitleBar;
    }

    ImGui::Separator();
    if (appData.guiData().m_showInspectionWindow && ImGui::MenuItem("Close")) {
      appData.guiData().m_showInspectionWindow = false;
    }
  };

  auto renderWindowMenu = [&renderWindowItems]() {
    if (ImGui::BeginMenu("Window")) {
      renderWindowItems();
      ImGui::EndMenu();
    }
  };

  auto contextMenu = [&renderImagesMenu, &renderColumnVisibilityMenu, &renderWindowMenu]() {
    renderImagesMenu();
    renderColumnVisibilityMenu();
    renderWindowMenu();
  };

  auto dockedMenu = [&renderImagesMenu, &renderColumnVisibilityMenu]() {
    renderImagesMenu();
    renderColumnVisibilityMenu();
  };

  auto columnWidths = [&]() {
    std::array<float, sk_inspectorColumnCount> widths{};
    for (std::size_t column = 0; column < widths.size(); ++column) {
      widths[column] = std::max(
        k_inspectorColumnDefaultWidths.at(column),
        inspectionColumnWidthForText(sk_inspectorColumnNames.at(column)));
    }

    auto expandWidth = [&widths](InspectorColumn column, const char* text) {
      const std::size_t index = static_cast<std::size_t>(column);
      widths[index] = std::max(widths[index], inspectionColumnWidthForText(text));
    };

    expandWidth(InspectorColumn::Value, "-0000.000");
    expandWidth(InspectorColumn::InterpolatedValue, "-0000.000");
    expandWidth(InspectorColumn::Percentile, "100.00");
    expandWidth(InspectorColumn::Label, "00000");
    expandWidth(InspectorColumn::Voxel, "0000, 0000, 0000");
    expandWidth(InspectorColumn::Subject, "-000.000, -000.000, -000.000");

    for (std::size_t imageIndex = 0; imageIndex < appData.numImages(); ++imageIndex) {
      const auto imageUid = appData.imageUid(imageIndex);
      if (!imageUid) continue;

      auto it = s_showSubject.find(*imageUid);
      if (std::end(s_showSubject) != it && !it->second) continue;

      const auto names = getImageDisplayAndFileName(imageIndex);
      expandWidth(InspectorColumn::Image, names.first.c_str());

      const Image* image = appData.image(*imageUid);
      if (!image) continue;

      if (image->header().numComponentsPerPixel() > 1) {
        expandWidth(InspectorColumn::Value, "-0000.000, -0000.000, -0000.000");
        expandWidth(InspectorColumn::InterpolatedValue, "-0000.000, -0000.000, -0000.000");
      }

      if (const auto segLabel = getSegLabel(imageIndex)) {
        const auto labelText = std::to_string(*segLabel);
        expandWidth(InspectorColumn::Label, labelText.c_str());

        const auto segUid = appData.imageToActiveSegUid(*imageUid);
        const Image* seg = segUid ? appData.seg(*segUid) : nullptr;
        ParcellationLabelTable* table = seg ? getLabelTable(seg->settings().labelTableIndex()) : nullptr;
        if (table) {
          const std::string labelName = table->getName(static_cast<uint64_t>(*segLabel));
          expandWidth(InspectorColumn::Region, labelName.c_str());
        }
      }
    }

    for (std::size_t column = 0; column < widths.size(); ++column) {
      widths[column] = clampInspectionColumnWidth(static_cast<InspectorColumn>(column), widths[column]);
    }

    return widths;
  };

  const bool useOverlayPresentation = corner != -1;
  ImGuiWindowFlags windowFlags = sk_windowFlags;

  if (corner != -1) {
    windowFlags |= ImGuiWindowFlags_NoMove;

    ImGuiIO& io = ImGui::GetIO();

    const ImVec2 windowPos(
      (corner & 1) ? io.DisplaySize.x - sk_pad : sk_pad,
      (corner & 2) ? io.DisplaySize.y - sk_pad : sk_pad);

    const ImVec2 windowPosPivot((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPosPivot);
  }

  if (corner != -1 && !s_showTitleBar) {
    windowFlags |= ImGuiWindowFlags_NoDecoration;
  }

  const ImVec4* colors = ImGui::GetStyle().Colors;
  ImVec4 menuBarBgColor = colors[ImGuiCol_MenuBarBg];
  menuBarBgColor.w /= 2.0f;

  if (useOverlayPresentation) {
    ImGui::SetNextWindowBgAlpha(0.0f);
  }

  const std::array<float, sk_inspectorColumnCount> inspectorColumnWidths = columnWidths();

  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, sk_cellPadding);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImGui::GetStyle().FramePadding);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, sk_itemInnerSpacing);
  ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(
    ImGuiStyleVar_WindowPadding,
    useOverlayPresentation ? sk_windowPadding : ImGui::GetStyle().WindowPadding);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, sk_windowRounding);

  setNextWindowSizeConstraintsToMainViewport(480.0f, 120.0f);
  ImGui::SetNextWindowSize(ImVec2{720.0f, 180.0f}, ImGuiCond_FirstUseEver);
  setNextDockablePanelWindowClass();
  if (ImGui::Begin("Voxel Inspector##InspectionWindow", &(appData.guiData().m_showInspectionWindow), windowFlags)) {
    if (useOverlayPresentation) {
      ImGui::PushStyleColor(ImGuiCol_MenuBarBg, menuBarBgColor);
    }
    if (ImGui::BeginMenuBar()) {
      if (useOverlayPresentation) {
        contextMenu();
      }
      else {
        dockedMenu();
      }
      ImGui::EndMenuBar();
    }
    if (useOverlayPresentation) {
      ImGui::PopStyleColor(1); // ImGuiCol_MenuBarBg
    }

    if (ImGui::BeginTable("Image Information", static_cast<int>(sk_inspectorColumnCount), sk_tableFlags)) {
      ImGui::TableSetupScrollFreeze(1, 1);

      ImGui::TableSetupColumn(
        sk_inspectorColumnNames.at(columnIndex(InspectorColumn::Image)),
        ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_WidthFixed,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Image)));

      ImGui::TableSetupColumn(
        sk_inspectorColumnNames.at(columnIndex(InspectorColumn::Value)),
        ImGuiTableColumnFlags_WidthFixed,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Value)));
      ImGui::TableSetupColumn(
        sk_inspectorColumnNames.at(columnIndex(InspectorColumn::InterpolatedValue)),
        ImGuiTableColumnFlags_WidthFixed,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::InterpolatedValue)));
      ImGui::TableSetupColumn(
        sk_inspectorColumnNames.at(columnIndex(InspectorColumn::Percentile)),
        ImGuiTableColumnFlags_WidthFixed,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Percentile)));
      ImGui::TableSetupColumn(
        sk_inspectorColumnNames.at(columnIndex(InspectorColumn::Label)),
        ImGuiTableColumnFlags_WidthFixed,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Label)));
      ImGui::TableSetupColumn(
        sk_inspectorColumnNames.at(columnIndex(InspectorColumn::Region)),
        ImGuiTableColumnFlags_WidthFixed,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Region)));

      ImGui::TableSetupColumn(
        sk_inspectorColumnNames.at(columnIndex(InspectorColumn::Voxel)),
        ImGuiTableColumnFlags_WidthFixed,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Voxel)));
      ImGui::TableSetupColumn(
        sk_inspectorColumnNames.at(columnIndex(InspectorColumn::Subject)),
        ImGuiTableColumnFlags_WidthFixed,
        inspectorColumnWidths.at(columnIndex(InspectorColumn::Subject)));

      for (std::size_t column = 0; column < sk_inspectorColumnNames.size(); ++column) {
        ImGui::TableSetColumnEnabled(
          static_cast<int>(column),
          appData.guiData().m_inspectionColumnVisible.at(column) || !sk_inspectorColumnCanHide.at(column));
      }

      ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
      if (s_autoSizeColumnsRequested) {
        for (std::size_t column = 0; column < inspectorColumnWidths.size(); ++column) {
          if (0 != (ImGui::TableGetColumnFlags(static_cast<int>(column)) & ImGuiTableColumnFlags_IsEnabled)) {
            ImGui::TableSetColumnWidth(static_cast<int>(column), inspectorColumnWidths.at(column));
          }
        }
        s_autoSizeColumnRequested.fill(false);
        s_autoSizeColumnsRequested = false;
      }
      else {
        for (std::size_t column = 0; column < s_autoSizeColumnRequested.size(); ++column) {
          if (!s_autoSizeColumnRequested.at(column)) continue;
          if (0 == (ImGui::TableGetColumnFlags(static_cast<int>(column)) & ImGuiTableColumnFlags_IsEnabled)) continue;

          ImGui::TableSetColumnWidth(static_cast<int>(column), inspectorColumnWidths.at(column));
          s_autoSizeColumnRequested.at(column) = false;
        }
      }

      for (std::size_t column = 0; column < sk_inspectorColumnNames.size(); ++column) {
        ImGui::TableSetColumnIndex(static_cast<int>(column));
        ImGui::TableHeader(sk_inspectorColumnNames.at(column));

        const char* tooltip = inspectionColumnTooltip(static_cast<InspectorColumn>(column));
        if (tooltip && ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", tooltip);
        }
      }

      for (std::size_t imageIndex = 0; imageIndex < appData.numImages(); ++imageIndex) {
        const auto imageUid = appData.imageUid(imageIndex);
        Image* image = (imageUid ? appData.image(*imageUid) : nullptr);
        if (!image) continue;

        auto it = s_showSubject.find(*imageUid);
        if (std::end(s_showSubject) == it) {
          s_showSubject.insert({*imageUid, true});
        }

        if (!s_showSubject[*imageUid]) continue;

        ImGui::PushID(static_cast<int>(imageIndex)); /** PushID: imageIndex **/

        const auto segUid = appData.imageToActiveSegUid(*imageUid);
        const Image* seg = (segUid ? appData.seg(*segUid) : nullptr);

        ParcellationLabelTable* table = (seg ? getLabelTable(seg->settings().labelTableIndex()) : nullptr);

        // Get all image component values
        static constexpr bool sk_getOnlyActiveComponent = false;
        std::vector<double> imageValuesNN = getImageValuesNN(imageIndex, sk_getOnlyActiveComponent);
        std::vector<double> imageValuesLinear = getImageValuesLinear(imageIndex, sk_getOnlyActiveComponent);

        const std::optional<int64_t> segLabel = getSegLabel(imageIndex);

        const std::optional<glm::ivec3> voxelPos = getVoxelPos(imageIndex);
        const std::optional<glm::vec3> subjectPos = getSubjectPos(imageIndex);

        ImGui::TableNextColumn(); // "Image"

        glm::vec3 darkerBorderColorHsv = glm::hsvColor(image->settings().borderColor());
        darkerBorderColorHsv[2] = std::max(0.5f * darkerBorderColorHsv[2], 0.0f);
        const glm::vec3 darkerBorderColorRgb = glm::rgbColor(darkerBorderColorHsv);

        const ImVec4 inputTextBgColor(darkerBorderColorRgb.r, darkerBorderColorRgb.g, darkerBorderColorRgb.b, 1.0f);
        const ImVec4 inputTextFgColor = (glm::luminosity(darkerBorderColorRgb) < 0.75f) ? whiteText : blackText;

        const bool isRef = appData.refImageUid() && *appData.refImageUid() == *imageUid;
        const bool isActiveImage = appData.activeImageUid() && *appData.activeImageUid() == *imageUid;
        const std::string roleSuffix =
          entropy::ui::headers::imageRoleSuffixShortReference(isRef, isActiveImage, appData.numImages());
        const float suffixWidth =
          roleSuffix.empty() ? 0.0f : ImGui::CalcTextSize(roleSuffix.c_str()).x + ImGui::GetStyle().ItemSpacing.x;
        const float nameWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - suffixWidth);

        ImGui::PushStyleColor(ImGuiCol_FrameBg, inputTextBgColor);
        ImGui::PushStyleColor(ImGuiCol_Text, inputTextFgColor);
        ImGui::PushItemWidth(nameWidth);
        bool nameHovered = false;
        {
          std::string displayName = image->settings().displayName();
          if (ImGui::InputText("##displayName", &displayName)) {
            image->settings().setDisplayName(displayName);
          }
          nameHovered = ImGui::IsItemHovered();
        }
        ImGui::PopItemWidth();
        ImGui::PopStyleColor(2); // ImGuiCol_FrameBg, ImGuiCol_Text

        if (!roleSuffix.empty()) {
          ImGui::SameLine();
          ImGui::TextUnformatted(roleSuffix.c_str());
          nameHovered = nameHovered || ImGui::IsItemHovered();
        }

        if (nameHovered) {
          ImGui::SetTooltip("%s", image->header().fileName().c_str());
        }

        ImGui::TableNextColumn(); // "Value (NN)"

        if (!imageValuesNN.empty()) {
          if (isComponentFloatingPoint(image->header().memoryComponentType())) {
            if (image->header().numComponentsPerPixel() > 1) {
              ImGui::PushItemWidth(-1);
              ImGui::InputScalarN(
                "##imageValuesNN",
                ImGuiDataType_Double,
                imageValuesNN.data(),
                static_cast<int>(imageValuesNN.size()),
                nullptr,
                nullptr,
                appData.guiData().m_imageValuePrecisionFormat.c_str(),
                ImGuiInputTextFlags_ReadOnly);
              ImGui::PopItemWidth();

              if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Active component: %d", image->settings().activeComponent());
              }
            }
            else {
              double a = imageValuesNN[0];

              ImGui::PushItemWidth(-1);
              ImGui::InputScalar(
                "##imageValuesNN",
                ImGuiDataType_Double,
                &a,
                nullptr,
                nullptr,
                appData.guiData().m_imageValuePrecisionFormat.c_str(),
                ImGuiInputTextFlags_ReadOnly);
              ImGui::PopItemWidth();
            }
          }
          else {
            if (image->header().numComponentsPerPixel() > 1) {
              std::vector<int64_t> imageValuesNNInt;

              for (std::size_t i = 0; i < imageValuesNN.size(); ++i) {
                imageValuesNNInt.push_back(static_cast<int64_t>(imageValuesNN[i]));
              }

              ImGui::PushItemWidth(-1);
              ImGui::InputScalarN(
                "##imageValuesNN",
                ImGuiDataType_S64,
                imageValuesNNInt.data(),
                static_cast<int>(imageValuesNNInt.size()),
                nullptr,
                nullptr,
                "%ld",
                ImGuiInputTextFlags_ReadOnly);
              ImGui::PopItemWidth();

              if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Active component: %d", image->settings().activeComponent());
              }
            }
            else {
              int64_t a = static_cast<int64_t>(imageValuesNN[0]);

              ImGui::PushItemWidth(-1);
              ImGui::InputScalar(
                "##imageValuesNN",
                ImGuiDataType_S64,
                &a,
                nullptr,
                nullptr,
                "%ld",
                ImGuiInputTextFlags_ReadOnly);
              ImGui::PopItemWidth();
            }
          }
        }
        else {
          ImGui::Text("<N/A>");
        }

        ImGui::TableNextColumn(); // "Value (Linear)"

        if (!imageValuesLinear.empty()) {
          // Display linearly interpolated image values using doubles for both floating point and
          // integer images
          // if ( isComponentFloatingPoint( image->header().memoryComponentType() ) )
          {
            if (image->header().numComponentsPerPixel() > 1) {
              ImGui::PushItemWidth(-1);
              ImGui::InputScalarN(
                "##imageValuesLinear",
                ImGuiDataType_Double,
                imageValuesLinear.data(),
                static_cast<int>(imageValuesLinear.size()),
                nullptr,
                nullptr,
                appData.guiData().m_imageValuePrecisionFormat.c_str(),
                ImGuiInputTextFlags_ReadOnly);
              ImGui::PopItemWidth();

              if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Active component: %d", image->settings().activeComponent());
              }
            }
            else {
              double a = imageValuesLinear[0];

              ImGui::PushItemWidth(-1);
              ImGui::InputScalar(
                "##imageValuesLinear",
                ImGuiDataType_Double,
                &a,
                nullptr,
                nullptr,
                appData.guiData().m_imageValuePrecisionFormat.c_str(),
                ImGuiInputTextFlags_ReadOnly);
              ImGui::PopItemWidth();
            }
          }
          /*
                    else
                    {
                        if ( image->header().numComponentsPerPixel() > 1 )
                        {
                            std::vector< int64_t > imageValuesLinearInt;

                            for ( size_t i = 0; i < imageValuesLinear.size(); ++i )
                            {
                                imageValuesLinearInt.push_back( static_cast<int64_t>(
             imageValuesLinear[i] ) );
                            }

                            ImGui::PushItemWidth( -1 );
                            ImGui::InputScalarN( "##imageValuesLinear", ImGuiDataType_S64,
                                                imageValuesLinearInt.data(),
             imageValuesLinearInt.size(), nullptr, nullptr, "%ld", ImGuiInputTextFlags_ReadOnly );
                            ImGui::PopItemWidth();

                            if ( ImGui::IsItemHovered() )
                            {
                                ImGui::SetTooltip( "Active component: %d",
             image->settings().activeComponent() );
                            }
                        }
                        else
                        {
                            int64_t a = static_cast<int64_t>( imageValuesLinear[0] );

                            ImGui::PushItemWidth( -1 );
                            ImGui::InputScalar( "##imageValuesLinear", ImGuiDataType_S64, &a,
             nullptr, nullptr, "%ld", ImGuiInputTextFlags_ReadOnly ); ImGui::PopItemWidth();
                        }
                    }
                    */
        }
        else {
          ImGui::Text("<N/A>");
        }

        ImGui::TableNextColumn(); // "Percentile"

        if (const std::optional<double> percentile = activeComponentPercentile(*image, imageValuesNN)) {
          double a = *percentile;
          ImGui::PushItemWidth(-1);
          ImGui::InputScalar(
            "##imagePercentile",
            ImGuiDataType_Double,
            &a,
            nullptr,
            nullptr,
            appData.guiData().m_percentilePrecisionFormat.c_str(),
            ImGuiInputTextFlags_ReadOnly);
          ImGui::PopItemWidth();

          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Active component: %d", image->settings().activeComponent());
          }
        }
        else {
          ImGui::Text("<N/A>");
        }

        if (segLabel) {
          ImGui::TableNextColumn(); // "Label"

          // Segmentation labels are unsigned, so we can cast:
          uint64_t l = static_cast<uint64_t>(*segLabel);
          ImGui::PushItemWidth(-1);
          ImGui::InputScalar("##segLabel", ImGuiDataType_U64, &l, nullptr, nullptr, "%ld");
          ImGui::PopItemWidth();

          if (table) {
            std::string labelName = table->getName(l);

            if (ImGui::IsItemHovered()) {
              // Tooltip for the segmentation label
              ImGui::SetTooltip("%s", labelName.c_str());
            }

            ImGui::TableNextColumn(); // "Region"

            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##labelName", &labelName)) {
              table->setName(l, labelName);
            }
            ImGui::PopItemWidth();
          }
          else {
            ImGui::TableNextColumn(); // "Region"
            ImGui::Text("<N/A>");
          }
        }
        else {
          ImGui::TableNextColumn(); // "Label"
          ImGui::Text("<N/A>");

          ImGui::TableNextColumn(); // "Region"
          ImGui::Text("<N/A>");
        }

        if (voxelPos) {
          static const glm::ivec3 sk_zero{0};
          static const glm::ivec3 sk_minDim{0};

          ImGui::TableNextColumn(); // "Voxel"

          const glm::ivec3 sk_maxDim = static_cast<glm::ivec3>(image->header().pixelDimensions()) - glm::ivec3{1, 1, 1};

          glm::ivec3 a = *voxelPos;
          ImGui::PushItemWidth(-1);
          if (ImGui::DragScalarN(
                "##voxelPos",
                ImGuiDataType_S32,
                glm::value_ptr(a),
                3,
                1.0f,
                glm::value_ptr(sk_minDim),
                glm::value_ptr(sk_maxDim),
                "%d"))
          {
            if (
              glm::all(glm::greaterThanEqual(a, sk_zero)) &&
              glm::all(glm::lessThan(a, glm::ivec3{image->header().pixelDimensions()})))
            {
              setVoxelPos(imageIndex, a);
            }
          }
          ImGui::PopItemWidth();
        }
        else {
          ImGui::TableNextColumn(); // "Voxel"
          ImGui::Text("<N/A>");
        }

        if (subjectPos) {
          ImGui::TableNextColumn(); // "Subject LPS"

          // Step size is the  minimum voxel spacing
          const float stepSize = glm::compMin(image->header().spacing());
          glm::vec3 a = *subjectPos;

          ImGui::PushItemWidth(-1);
          if (ImGui::DragScalarN(
                "##physicalPos",
                ImGuiDataType_Float,
                glm::value_ptr(a),
                3,
                stepSize,
                nullptr,
                nullptr,
                appData.guiData().m_coordsPrecisionFormat.c_str()))
          {
            setSubjectPos(imageIndex, a);
          }
          ImGui::PopItemWidth();
        }
        else {
          ImGui::TableNextColumn(); // "Subject LPS"
          ImGui::Text("<N/A>");
        }

        ImGui::PopID(); /** PopID: imageIndex **/
      }

      ImGui::EndTable();
    }

    if (ImGui::BeginPopupContextWindow()) {
      // Show context menu on right-button click:
      contextMenu();
      ImGui::EndPopup();
    }
    else if (ImGui::BeginPopup("selectionPopup")) {
      // Show context menu if the user has clicked the popup button:
      contextMenu();
      ImGui::EndPopup();
    }
  }

  ImGui::End();

  // ImGuiStyleVar_CellPadding
  // ImGuiStyleVar_FramePadding
  // ImGuiStyleVar_ItemInnerSpacing
  // ImGuiStyleVar_ScrollbarSize
  // ImGuiStyleVar_WindowBorderSize
  // ImGuiStyleVar_WindowPadding
  // ImGuiStyleVar_WindowRounding
  ImGui::PopStyleVar(7);
}
