#include "ui/popups/Popups.h"
#include "ui/popups/PopupCommon.h"
#include "ui/AboutIcon.h"
#include "ui/DicomMetadataTable.h"
#include "ui/Helpers.h"
#include "ui/NativeFileDialogs.h"
#include "ui/ThirdPartyLicenses.h"

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
#include <optional>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using namespace ui::popups;

namespace
{
constexpr const char* sk_addLayoutPopupId = "Add Layout###AddLayoutModal";

constexpr std::array<ViewType, 3> sk_lightboxViewTypes{ViewType::Axial, ViewType::Coronal, ViewType::Sagittal};

const char* lightboxViewTypeName(ViewType viewType)
{
  switch (viewType) {
    case ViewType::Axial:
      return "Axial";
    case ViewType::Coronal:
      return "Coronal";
    case ViewType::Sagittal:
      return "Sagittal";
    default:
      return "Axial";
  }
}

} // namespace

void renderAddLayoutModalPopup(
  AppData& appData,
  bool openAddLayoutPopup,
  const std::function<void(void)>& recenterViews)
{
  bool addLayout = false;

  static int columns = 3;
  static int rows = 3;
  static bool isLightbox = false;
  static ViewType lightboxViewType = ViewType::Axial;
  static float lightboxSliceSpacingMm = 1.0f;
  static std::string customLayoutName = "Custom";

  if (openAddLayoutPopup && !ImGui::IsPopupOpen(sk_addLayoutPopupId)) {
    ImGui::OpenPopup(sk_addLayoutPopupId);
  }

  if (openAddLayoutPopup || ImGui::IsPopupOpen(sk_addLayoutPopupId)) {
    const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  }

  bool addLayoutDialogOpen = true;
  if (ImGui::BeginPopupModal(sk_addLayoutPopupId, &addLayoutDialogOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Please set the number of views in the new layout:");

    if (ImGui::InputInt("Rows", &rows)) {
      rows = std::max(rows, 1);
    }

    if (ImGui::InputInt("Columns", &columns)) {
      columns = std::max(columns, 1);
    }

    if (columns >= 5 && rows >= 5) {
      isLightbox = true;
    }

    ImGui::Checkbox("Lightbox mode", &isLightbox);
    ImGui::SameLine();
    helpMarker("Should all views in the layout share a common view type?");

    if (isLightbox) {
      if (ImGui::BeginCombo("View type", lightboxViewTypeName(lightboxViewType))) {
        for (const ViewType candidate : sk_lightboxViewTypes) {
          const bool selected = candidate == lightboxViewType;
          if (ImGui::Selectable(lightboxViewTypeName(candidate), selected)) {
            lightboxViewType = candidate;
          }
          if (selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
      ImGui::SameLine();
      helpMarker("Choose the anatomical plane used for every lightbox tile");

      if (ImGui::InputFloat("Slice spacing (mm)", &lightboxSliceSpacingMm, 0.1f, 1.0f, "%.3f")) {
        lightboxSliceSpacingMm = std::max(lightboxSliceSpacingMm, 0.0f);
      }
      ImGui::SameLine();
      helpMarker(
        "Distance between adjacent custom lightbox tiles. Custom lightboxes use this fixed spacing instead of image "
        "slice spacing");
    }
    else if (!(1 == rows && columns > 1)) {
      ImGui::InputText("Name", &customLayoutName);
    }

    ImGui::Separator();

    if (ImGui::Button("OK")) {
      addLayout = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      addLayout = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  if (addLayout) {
    if (const auto& refUid = appData.refImageUid()) {
      // Apply offsets to views if using lightbox mode
      const bool offsetViews = (isLightbox);
      const ViewType viewType = isLightbox ? lightboxViewType : ViewType::Axial;

      auto& wd = appData.windowData();

      wd.addGridLayout(
        viewType,
        static_cast<std::size_t>(columns),
        static_cast<std::size_t>(rows),
        offsetViews,
        isLightbox,
        0,
        *refUid,
        isLightbox ? std::optional<float>{lightboxSliceSpacingMm} : std::nullopt);

      wd.setCurrentLayoutIndex(wd.numLayouts() - 1);
      if (!isLightbox && LayoutKind::Custom == wd.currentLayout().kind()) {
        wd.currentLayout().setDisplayName(customLayoutName);
      }
      wd.setDefaultRenderedImagesForLayout(wd.currentLayout(), appData);

      recenterViews();
    }
  }
}
