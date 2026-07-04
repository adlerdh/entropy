#include "ui/windows/InspectionWindow.h"

#include "ui/Helpers.h"
#include "ui/headers/HeaderCommon.h"
#include "logic/app/Data.h"

#include "image/Image.h"

#include <IconsForkAwesome.h>
#include <glm/glm.hpp>
#include <imgui/imgui.h>

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdio>
#include <optional>
#include <string>
#include <unordered_map>

namespace
{
using uuid = uuids::uuid;

std::string formatDouble(const char* format, double value)
{
  std::array<char, 64> buffer{};
  std::snprintf(buffer.data(), buffer.size(), format, value);
  return buffer.data();
}
} // namespace

// enum class PopupWindowPosition
//{
//     Custom,
//     TopLeft,
//     TopRight,
//     BottomLeft,
//     BottomRight
// };

void renderInspectionWindow(
  AppData& appData,
  size_t numImages,
  const std::function<std::pair<std::string, std::string>(std::size_t index)>& getImageDisplayAndFileName,
  const std::function<glm::vec3()>& getWorldDeformedPos,
  const std::function<std::optional<glm::vec3>(std::size_t imageIndex)>& getSubjectPos,
  const std::function<std::optional<glm::ivec3>(std::size_t imageIndex)>& getVoxelPos,
  const std::function<std::optional<double>(std::size_t imageIndex)>& getImageValueNN,
  const std::function<std::optional<double>(std::size_t imageIndex)>& getImageValueLinear,
  const std::function<std::optional<int64_t>(std::size_t imageIndex)>& getSegLabel,
  const std::function<ParcellationLabelTable*(std::size_t tableIndex)>& getLabelTable)
{
  std::ignore = getImageValueLinear;

  static constexpr size_t sk_refIndex = 0; // Index of the reference image
  static bool s_firstRun = false;          // Is this the first run?

  static constexpr float sk_pad = 10.0f;
  static int corner = 2;
  //    static PopupWindowPosition pos = PopupWindowPosition::BottomLeft;

  // Show World coordinates?
  static bool s_showWorldCoords = false;

  bool selectionButtonShown = false;

  static const ImVec4 buttonColor(0.0f, 0.0f, 0.0f, 0.0f);
  static const ImVec4 blueColor(0.0f, 0.5f, 1.0f, 1.0f);
  const std::size_t visibleImageCount =
    std::min(numImages, appData.guiData().m_visibleImageCountDuringLoad.value_or(numImages));

  // For which images to show coordinates?
  static std::unordered_map<uuid, bool> s_showSubject;

  if (s_firstRun) {
    // Show the first (reference) image coordinates by default:
    if (const auto imageUid = appData.imageUid(sk_refIndex)) {
      s_showSubject.insert({*imageUid, true});
    }

    s_firstRun = false;
  }

  auto contextMenu = [&visibleImageCount, &appData, &getImageDisplayAndFileName]() {
    if (ImGui::BeginMenu("Show")) {
      //                if ( ImGui::MenuItem( "World", nullptr, s_showWorldCoords ) )
      //                {
      //                    s_showWorldCoords = ! s_showWorldCoords;
      //                }
      //                if ( ImGui::IsItemHovered() )
      //                {
      //                    ImGui::SetTooltip( "Show World-space crosshairs coordinates" );
      //                }

      for (std::size_t imageIndex = 0; imageIndex < visibleImageCount; ++imageIndex) {
        const auto imageUid = appData.imageUid(imageIndex);
        if (!imageUid) continue;

        bool& visible = s_showSubject[*imageUid];

        const auto names = getImageDisplayAndFileName(imageIndex);

        if (ImGui::MenuItem(names.first.c_str(), nullptr, visible)) {
          visible = !visible;
        }

        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", names.second.c_str());
        }
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Position")) {
      if (ImGui::MenuItem("Custom", nullptr, corner == -1)) corner = -1;
      if (ImGui::MenuItem("Top-left", nullptr, corner == 0)) corner = 0;
      if (ImGui::MenuItem("Top-right", nullptr, corner == 1)) corner = 1;
      if (ImGui::MenuItem("Bottom-left", nullptr, corner == 2)) corner = 2;
      if (ImGui::MenuItem("Bottom-right", nullptr, corner == 3)) corner = 3;

      ImGui::EndMenu();
    }

    if (appData.guiData().m_showInspectionWindow && ImGui::MenuItem("Close")) {
      appData.guiData().m_showInspectionWindow = false;
    }

    ImGui::EndPopup();
  };

  auto showSelectionButton = []() {
    ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
    if (ImGui::Button(ICON_FK_LIST_ALT)) {
      ImGui::OpenPopup("selectionPopup");
    }
    ImGui::PopStyleColor(1);

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Select image(s) to inspect");
    }
  };

  ImGuiIO& io = ImGui::GetIO();

  ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

  if (corner != -1) {
    windowFlags |= ImGuiWindowFlags_NoMove;

    const ImVec2 windowPos =
      ImVec2((corner & 1) ? io.DisplaySize.x - sk_pad : sk_pad, (corner & 2) ? io.DisplaySize.y - sk_pad : sk_pad);

    const ImVec2 windowPosPivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPosPivot);
  }

  ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background

  setNextWindowSizeConstraintsToMainViewport();
  if (ImGui::Begin("##InspectionWindow", &(appData.guiData().m_showInspectionWindow), windowFlags)) {
    //        if ( ImGui::IsMousePosValid() && ImGui::IsAnyMouseDown() )
    //        {
    //            ImGui::Text( "Mouse Position: (%.0f, %.0f)",
    //                         static_cast<double>( io.MousePos.x ),
    //                         static_cast<double>( io.MousePos.y ) );
    //        }

    if (s_showWorldCoords) {
      const glm::vec3 worldPos = getWorldDeformedPos();

      //            ImGui::Text( "World:" );
      ImGui::Text(
        "(%.3f, %.3f, %.3f) mm",
        static_cast<double>(worldPos.x),
        static_cast<double>(worldPos.y),
        static_cast<double>(worldPos.z));

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("World-space coordinates");
      }
    }

    bool firstImageShown = true;
    bool showedAtLeastOneImage = false; // is info for at least one image shown?

    for (std::size_t imageIndex = 0; imageIndex < visibleImageCount; ++imageIndex) {
      const auto imageUid = appData.imageUid(imageIndex);
      const Image* image = (imageUid ? appData.image(*imageUid) : nullptr);
      if (!image) continue;

      if (0 == s_showSubject.count(*imageUid)) {
        // The reference image is shown by default in this list
        s_showSubject.insert({*imageUid, (sk_refIndex == imageIndex) ? true : false});
      }

      const bool visible = s_showSubject[*imageUid];
      if (!visible) continue;

      showedAtLeastOneImage = true;

      if (s_showWorldCoords || !firstImageShown) {
        ImGui::Separator();
      }

      firstImageShown = false;

      const auto names = getImageDisplayAndFileName(imageIndex);
      const bool isRef = appData.refImageUid() && *appData.refImageUid() == *imageUid;
      const bool isActiveImage = appData.activeImageUid() && *appData.activeImageUid() == *imageUid;
      const std::string displayName = entropy::ui::headers::imageDisplayNameWithShortReferenceRole(
        names.first,
        isRef,
        isActiveImage,
        appData.numImages());

      ImGui::TextColored(blueColor, "%s:", displayName.c_str());

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", names.second.c_str());
      }

      /// @todo Do we want to show subject coords for all images?
      if (sk_refIndex == imageIndex) {
        // Show subject coordinates for the reference image only:
        if (const auto subjectPos = getSubjectPos(imageIndex)) {
          const glm::dvec3 p{*subjectPos};
          const char* coordFormat = appData.guiData().m_coordsPrecisionFormat.c_str();
          ImGui::Text(
            "(%s, %s, %s) mm",
            formatDouble(coordFormat, p.x).c_str(),
            formatDouble(coordFormat, p.y).c_str(),
            formatDouble(coordFormat, p.z).c_str());
        }
      }

      if (const auto voxelPos = getVoxelPos(imageIndex)) {
        ImGui::Text("(%d, %d, %d) vox", voxelPos->x, voxelPos->y, voxelPos->z);
      }
      else {
        ImGui::Text("<N/A>");
      }

      if (const auto imageValue = getImageValueNN(imageIndex)) {
        if (isComponentFloatingPoint(image->header().memoryComponentType())) {
          const char* valueFormat = appData.guiData().m_imageValuePrecisionFormat.c_str();
          if (image->header().numComponentsPerPixel() > 1) {
            ImGui::Text(
              "Value (comp. %d): %s",
              image->settings().activeComponent(),
              formatDouble(valueFormat, *imageValue).c_str());
          }
          else {
            ImGui::Text("Value: %s", formatDouble(valueFormat, *imageValue).c_str());
          }
        }
        else {
          if (image->header().numComponentsPerPixel() > 1) {
            // Multi-component case: show the value of the active component
            ImGui::Text("Value (comp. %d): %d", image->settings().activeComponent(), static_cast<int>(*imageValue));
          }
          else {
            // Single component case
            ImGui::Text("Value: %d", static_cast<int32_t>(*imageValue));
          }
        }
      }

      const auto segUid = appData.imageToActiveSegUid(*imageUid);
      const Image* seg = (segUid ? appData.seg(*segUid) : nullptr);
      if (!seg) continue;

      if (const auto segLabel = getSegLabel(imageIndex)) {
        ImGui::Text("Label: %" PRId64, static_cast<int64_t>(*segLabel));

        const auto* table = getLabelTable(seg->settings().labelTableIndex());
        if (table && 0 != *segLabel) {
          const char* labelName = table->getName(static_cast<size_t>(*segLabel)).c_str();
          ImGui::SameLine();
          ImGui::Text("(%s)", labelName);
        }
      }

      if (!selectionButtonShown) {
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 24);
        showSelectionButton();
        selectionButtonShown = true;
      }
    }

    if (!showedAtLeastOneImage) {
      showSelectionButton();
    }

    if (ImGui::BeginPopupContextWindow()) {
      // Show context menu on right-button click:
      contextMenu();
    }
    else if (ImGui::BeginPopup("selectionPopup")) {
      // Show context menu if the user has clicked the popup button:
      contextMenu();
      ImGui::EndPopup();
    }
  }

  ImGui::End();
}
