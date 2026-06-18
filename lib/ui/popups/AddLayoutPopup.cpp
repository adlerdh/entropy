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

float defaultLightboxOffsetDistance(const AppData& appData, ViewType viewType)
{
  const Image* image = appData.refImage();
  if (!image) {
    image = appData.activeImage();
  }
  if (!image) {
    return 1.0f;
  }

  const glm::vec3& spacing = image->header().spacing();
  switch (viewType) {
    case ViewType::Sagittal:
      return spacing.x;
    case ViewType::Coronal:
      return spacing.y;
    case ViewType::Axial:
    default:
      return spacing.z;
  }
}

} // namespace

void renderAddLayoutModalPopup(
  AppData& appData,
  bool openAddLayoutPopup,
  const std::function<void(void)>& recenterViews)
{
  bool addLayout = false;

  static int width = 3;
  static int height = 3;
  static bool isLightbox = false;
  static ViewType lightboxViewType = ViewType::Axial;
  static float lightboxOffsetDistance = 1.0f;
  static ViewType lastLightboxViewType = ViewType::NumElements;

  if (openAddLayoutPopup && !ImGui::IsPopupOpen(sk_addLayoutPopupId)) {
    lightboxOffsetDistance = defaultLightboxOffsetDistance(appData, lightboxViewType);
    lastLightboxViewType = lightboxViewType;
    ImGui::OpenPopup(sk_addLayoutPopupId);
  }

  if (openAddLayoutPopup || ImGui::IsPopupOpen(sk_addLayoutPopupId)) {
    const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  }

  bool addLayoutDialogOpen = true;
  if (ImGui::BeginPopupModal(sk_addLayoutPopupId, &addLayoutDialogOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Please set the number of views in the new layout:");

    if (ImGui::InputInt("Horizontal", &width)) {
      width = std::max(width, 1);
    }

    if (ImGui::InputInt("Vertical", &height)) {
      height = std::max(height, 1);
    }

    if (width >= 5 && height >= 5) {
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
            lightboxOffsetDistance = defaultLightboxOffsetDistance(appData, lightboxViewType);
            lastLightboxViewType = lightboxViewType;
          }
          if (selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
      ImGui::SameLine();
      helpMarker("Choose the anatomical plane used for every lightbox tile.");

      if (lastLightboxViewType != lightboxViewType) {
        lightboxOffsetDistance = defaultLightboxOffsetDistance(appData, lightboxViewType);
        lastLightboxViewType = lightboxViewType;
      }

      if (ImGui::InputFloat("Offset distance (mm)", &lightboxOffsetDistance, 0.1f, 1.0f, "%.3f")) {
        lightboxOffsetDistance = std::max(lightboxOffsetDistance, std::numeric_limits<float>::epsilon());
      }
      ImGui::SameLine();
      helpMarker("Distance between adjacent lightbox tiles along the selected view normal.");
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
      const std::optional<float> offsetDistance =
        isLightbox ? std::optional<float>{lightboxOffsetDistance} : std::nullopt;

      auto& wd = appData.windowData();

      wd.addGridLayout(
        viewType,
        static_cast<std::size_t>(width),
        static_cast<std::size_t>(height),
        offsetViews,
        isLightbox,
        0,
        *refUid,
        offsetDistance);

      wd.setCurrentLayoutIndex(wd.numLayouts() - 1);
      wd.setDefaultRenderedImagesForLayout(wd.currentLayout(), appData);

      recenterViews();
    }
  }
}
