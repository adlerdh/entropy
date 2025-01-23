#include "ui/IsosurfaceHeader.h"
#include "ui/Helpers.h"

#include "common/UuidUtility.h"

#include "image/Image.h"
#include "image/ImageColorMap.h"
#include "image/SurfaceUtility.h"

#include "logic/app/Data.h"

#include "mesh/MeshLoading.h"

#include <IconFontCppHeaders/IconsForkAwesome.h>

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#undef min
#undef max

namespace
{

static const ImVec4 sk_whiteText(1, 1, 1, 1);
static const ImVec4 sk_blackText(0, 0, 0, 1);

/// Size of small toolbar buttons (pixels)
//static const ImVec2 sk_smallToolbarButtonSize( 24, 24 );

std::pair<ImVec4, ImVec4> computeHeaderBgAndTextColors(const glm::vec3& color)
{
  glm::vec3 darkerBorderColorHsv = glm::hsvColor(color);
  darkerBorderColorHsv[2] = std::max(0.5f * darkerBorderColorHsv[2], 0.0f);
  const glm::vec3 darkerBorderColorRgb = glm::rgbColor(darkerBorderColorHsv);

  const ImVec4
    headerColor(darkerBorderColorRgb.r, darkerBorderColorRgb.g, darkerBorderColorRgb.b, 1.0f);
  const ImVec4 headerTextColor = (glm::luminosity(darkerBorderColorRgb) < 0.75f) ? sk_whiteText
                                                                                 : sk_blackText;

  return {headerColor, headerTextColor};
}

static const ImGuiTableFlags sk_isosurfaceTableFlags = ImGuiTableFlags_Resizable
                                                       | ImGuiTableFlags_Reorderable
                                                       | ImGuiTableFlags_Hideable
                                                       | ImGuiTableFlags_Sortable
                                                       | ImGuiTableFlags_SortMulti
                                                       | ImGuiTableFlags_RowBg
                                                       | ImGuiTableFlags_Borders
                                                       | ImGuiTableFlags_NoBordersInBody
                                                       | ImGuiTableFlags_ScrollX
                                                       | ImGuiTableFlags_ScrollY
                                                       | ImGuiTableFlags_SizingFixedFit;

/// Isosurface table columns
enum TableColumnId : uint32_t
{
  /// "Name" column shows several things: visibility checkbox, surface name, surface color picker
  Name = 0,

  /// "Value" column shows isosurface value input selector
  Value = 1
};

static const ImGuiTableColumnFlags sk_nameColumnFlags = ImGuiTableColumnFlags_DefaultSort
                                                        | ImGuiTableColumnFlags_PreferSortDescending
                                                        | ImGuiTableColumnFlags_WidthFixed
                                                        | ImGuiTableColumnFlags_NoHide;

static const ImGuiTableColumnFlags sk_isoValueColumnFlags = ImGuiTableColumnFlags_PreferSortDescending
                                                            | ImGuiTableColumnFlags_WidthFixed
                                                            | ImGuiTableColumnFlags_NoHide;

//        ( sk_isosurfaceTableFlags & ImGuiTableFlags_NoHostExtendX ) ? 0 : ImGuiTableColumnFlags_WidthStretch;

/**
 * @brief Represents a table row for one isosurface
 */
struct IsosurfaceTableItem
{
  IsosurfaceTableItem(const uuids::uuid& surfaceUid, Isosurface* surface)
    : m_surfaceUid(surfaceUid)
    , m_surface(surface)
  {
  }

  uuids::uuid m_surfaceUid; //!< UID of the surface
  Isosurface* m_surface;    //!< Reference to the surface
};

enum class IsosurfaceTableItemContentsType
{
  Selectable,
  SelectableSpanRow
};

/**
 * @brief Custom \c std::sort function for isosurface table rows
 * @param[in] a First item to sort
 * @param[in] b Second item to sort
 * @param[in] sortSpecs
 */
bool compareWithSortSpecs(
  const IsosurfaceTableItem& a, const IsosurfaceTableItem& b, const ImGuiTableSortSpecs& sortSpecs
)
{
  for (int i = 0; i < sortSpecs.SpecsCount; ++i)
  {
    const ImGuiTableColumnSortSpecs& spec = sortSpecs.Specs[i];

    double delta = 0.0;

    switch (spec.ColumnUserID)
    {
    case TableColumnId::Name:
    {
      delta = static_cast<double>(a.m_surface->name.compare(b.m_surface->name));
      break;
    }
    case TableColumnId::Value:
    {
      delta = a.m_surface->value - b.m_surface->value;
      break;
    }
    default:
    {
      IM_ASSERT(0);
      break;
    }
    }

    if (delta > 0.0)
    {
      return (spec.SortDirection == ImGuiSortDirection_Ascending) ? true : false;
    }
    else if (delta <= 0.0)
    {
      return (spec.SortDirection == ImGuiSortDirection_Ascending) ? false : true;
    }
  }

  return (a.m_surface->value > b.m_surface->value);
}

std::optional<uuids::uuid> addNewSurface(
  AppData& appData,
  const Image* image,
  const uuids::uuid& imageUid,
  uint32_t component,
  size_t index,
  std::function<void(const uuids::uuid& taskUid, std::future<AsyncTaskDetails> future)> storeFuture,
  std::function<void(const uuids::uuid& taskUid)> addTaskToIsosurfaceGpuMeshGenerationQueue
)
{
  static constexpr uint32_t sk_defaultIsovalueQuantile = 75;

  if (!image)
  {
    return std::nullopt;
  }

  const auto& stats = image->settings().componentStatistics(component);

  Isosurface surface;
  surface.name = "Surface " + std::to_string(index);
  surface.value = stats.m_quantiles[sk_defaultIsovalueQuantile];
  surface.color = glm::vec3{0.5f, 0.75f, 1.0f};
  surface.opacity = 1.0f;
  surface.meshInSync = false;

  if (const auto isosurfaceUid = appData.addIsosurface(imageUid, component, std::move(surface)))
  {
    spdlog::debug(
      "Added new isosurface {} for image {} (component {}) at isovalue {}",
      *isosurfaceUid,
      imageUid,
      component,
      surface.value
    );

    // Function to update the mesh record in AppData after the mesh is generated
    auto meshCpuRecordUpdater =
      [&appData,
       &imageUid,
       component](const uuids::uuid& _isosurfaceUid, std::unique_ptr<MeshCpuRecord> meshCpuRecord)
      -> bool
    {
      if (appData.updateIsosurfaceMeshCpuRecord(
            imageUid, component, _isosurfaceUid, std::move(meshCpuRecord)
          ))
      {
        spdlog::debug(
          "Updated isosurface {} for image {} (component {}) with new mesh record",
          _isosurfaceUid,
          imageUid,
          component
        );
        return true;
      }

      spdlog::error(
        "Error updating isosurface {} for image {} (component {}) with new mesh record",
        _isosurfaceUid,
        imageUid,
        component
      );

      return false;
    };

    // Generate a new UID for the mesh generation task
    uuids::uuid taskUid = generateRandomUuid();

    // Need to store the future so that its destructor is not called.
    // Calling the destructor will cause us to wait on the future.
    // Note: Bind the task ID to addTaskToIsosurfaceGpuMeshGenerationQueue
    storeFuture(
      taskUid,
      generateIsosurfaceMeshCpuRecord(
        *image,
        imageUid,
        component,
        surface.value,
        *isosurfaceUid,
        meshCpuRecordUpdater,
        std::bind(addTaskToIsosurfaceGpuMeshGenerationQueue, taskUid)
      )
    );

    return isosurfaceUid;
  }
  else
  {
    spdlog::error("Unable to add new isosurface for image {}", imageUid);
    return std::nullopt;
  }
}

} // namespace

void renderIsosurfacesHeader(
  AppData& appData,
  const uuids::uuid& imageUid,
  size_t imageIndex,
  bool isActiveImage,
  std::function<void(const uuids::uuid& taskUid, std::future<AsyncTaskDetails> future)> storeFuture,
  std::function<void(const uuids::uuid& taskUid)> addTaskToIsosurfaceGpuMeshGenerationQueue
)
{
  static const ImGuiColorEditFlags sk_colorNoAlphaEditFlags = ImGuiColorEditFlags_PickerHueBar
                                                              | ImGuiColorEditFlags_DisplayRGB
                                                              | ImGuiColorEditFlags_DisplayHex
                                                              | ImGuiColorEditFlags_Uint8
                                                              | ImGuiColorEditFlags_InputRGB;

  static const ImGuiColorEditFlags sk_colorAlphaEditFlags = ImGuiColorEditFlags_NoInputs
                                                            | ImGuiColorEditFlags_PickerHueBar
                                                            | ImGuiColorEditFlags_DisplayRGB
                                                            | ImGuiColorEditFlags_DisplayHex
                                                            | ImGuiColorEditFlags_AlphaBar
                                                            | ImGuiColorEditFlags_AlphaPreviewHalf
                                                            | ImGuiColorEditFlags_Uint8
                                                            | ImGuiColorEditFlags_InputRGB;

  static const std::string sk_addSurfaceButtonText = std::string(ICON_FK_FILE_O)
                                                     + std::string(" Add surface");

  static const std::string sk_removeSurfaceButtonText = std::string(ICON_FK_TRASH_O)
                                                        + std::string(" Remove");

  static const std::string sk_saveSurfacesButtonText = std::string(ICON_FK_FLOPPY_O)
                                                       + std::string(" Save...");

  //    static const char* sk_saveSurfaceDialogTitle( "Save Isosurface Mesh" );
  //    static const std::vector< const char* > sk_saveSurfaceDialogFilters{};

  static const float sk_textBaseHeight = ImGui::GetTextLineHeightWithSpacing();

  static const IsosurfaceTableItemContentsType sk_contentsType
    = IsosurfaceTableItemContentsType::SelectableSpanRow;

  static const ImGuiSelectableFlags sk_selectableFlags
    = (IsosurfaceTableItemContentsType::SelectableSpanRow == sk_contentsType)
        ? (ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)
        : ImGuiSelectableFlags_None;

  static constexpr int sk_freezeCols = 1;
  static constexpr int sk_freezeRows = 1;

  static const ImVec2 sk_outerSizeValue = ImVec2(0.0f, sk_textBaseHeight * 12);
  static constexpr float sk_minRowHeight = 0.0f;         // Auto
  static constexpr float sk_innerWidthWithScroll = 0.0f; // Auto-extend

  static constexpr bool sk_outerSizeEnabled = true;
  static constexpr bool sk_showHeaders = true;

  // Force sorting of table items:
  static bool itemsNeedSort = false;

  // UID of the currently selected isosurface in the table
  static std::unordered_map<uuids::uuid, uuids::uuid> imageToSelectedSurfaceUid;

  // UID of the currently selected surface:
  std::optional<uuids::uuid> selectedSurfaceUid = std::nullopt;

  Image* image = appData.image(imageUid);
  if (!image)
    return;

  ImageSettings& imgSettings = image->settings();

  ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_CollapsingHeader;

  /// @todo This annoyingly pops up the active header each time... not sure why
  if (isActiveImage)
  {
    headerFlags |= ImGuiTreeNodeFlags_DefaultOpen;
  }

  ImGui::PushID(uuids::to_string(imageUid).c_str()); /** PushID imageUid **/

  // Header is ID'ed only by the image index.
  // ### allows the header name to change without changing its ID.
  const std::string headerName = std::to_string(imageIndex) + ") " + image->settings().displayName()
                                 + "###" + std::to_string(imageIndex);

  const auto headerColors = computeHeaderBgAndTextColors(image->settings().borderColor());
  ImGui::PushStyleColor(ImGuiCol_Header, headerColors.first);
  ImGui::PushStyleColor(ImGuiCol_Text, headerColors.second);

  const bool open = ImGui::CollapsingHeader(headerName.c_str(), headerFlags);

  ImGui::PopStyleColor(2); // ImGuiCol_Header, ImGuiCol_Text

  if (!open)
  {
    ImGui::PopID(); // imageUid
    return;
  }

  ImGui::Spacing();

  // By default, adjust image component 0:
  static uint32_t componentToAdjust = 0;

  // Component selection combo selection list. The component selection is shown only for
  // multi-component images, where each component is stored as a separate image.
  const bool showComponentSelection =
            ( image->header().numComponentsPerPixel() > 1 &&
              Image::MultiComponentBufferType::SeparateImages == image->bufferType() );

  if (showComponentSelection)
  {
    if (ImGui::BeginCombo("Image component", std::to_string(componentToAdjust).c_str()))
    {
      for (uint32_t comp = 0; comp < image->header().numComponentsPerPixel(); ++comp)
      {
        const bool isSelected = (componentToAdjust == comp);
        if (ImGui::Selectable(std::to_string(comp).c_str(), isSelected))
        {
          componentToAdjust = comp;
        }

        if (isSelected)
          ImGui::SetItemDefaultFocus();
      }

      ImGui::EndCombo();
    }

    ImGui::SameLine();
    helpMarker("Select the image component for which to adjust isosurfaces");

    ImGui::Separator();
    ImGui::Spacing();
  }

  const auto isosurfaceUids = appData.isosurfaceUids(imageUid, componentToAdjust);

  if (isosurfaceUids.empty())
  {
    ImGui::Text("This image has no isosurfaces.");

    ImGui::Spacing();
    const bool addSurface = ImGui::Button(sk_addSurfaceButtonText.c_str());
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("Add new isosurface");
    }

    if (addSurface)
    {
      if (const auto uid = addNewSurface(appData, image, imageUid, componentToAdjust, 1, storeFuture, addTaskToIsosurfaceGpuMeshGenerationQueue))
      {
        selectedSurfaceUid = *uid;
        imageToSelectedSurfaceUid[imageUid] = *uid;
      }

      return;
    }

    ImGui::PopID(); // imageUid
    return;
  }

  const ImVec4* colors = ImGui::GetStyle().Colors;
  ImGui::PushStyleColor(ImGuiCol_Header, colors[ImGuiCol_ButtonActive]);

  // Items representing isosurfaces in the table
  std::vector<IsosurfaceTableItem> tableItems;

  // Is the selected isosurface UID valid?
  bool validSelectedUid = false;

  const auto it = imageToSelectedSurfaceUid.find(imageUid);

  if (std::end(imageToSelectedSurfaceUid) == it)
  {
    selectedSurfaceUid = std::nullopt;
  }
  else
  {
    selectedSurfaceUid = it->second;
  }

  for (const auto& uid : appData.isosurfaceUids(imageUid, componentToAdjust))
  {
    Isosurface* surface = appData.isosurface(imageUid, componentToAdjust, uid);

    if (!surface)
    {
      spdlog::error("Isosurface {} is null: it is being removed", uid);
      appData.removeIsosurface(imageUid, componentToAdjust, uid);
      continue;
    }

    tableItems.push_back(IsosurfaceTableItem(uid, surface));

    // The selected UID is valid if there is a surface with this UID
    validSelectedUid = validSelectedUid | (selectedSurfaceUid && *selectedSurfaceUid == uid);
  }

  if (selectedSurfaceUid && !validSelectedUid)
  {
    // Selected UID was invalid, so remove it
    spdlog::warn("Invalid isosurface UID {} selected", *selectedSurfaceUid);
    selectedSurfaceUid = std::nullopt;
    imageToSelectedSurfaceUid.erase(imageUid);
  }

  const float innerWidthToUse = (sk_isosurfaceTableFlags & ImGuiTableFlags_ScrollX)
                                  ? sk_innerWidthWithScroll
                                  : 0.0f;

  const ImVec2 outerSize = sk_outerSizeEnabled ? sk_outerSizeValue : ImVec2(0, 0);

  if (ImGui::BeginTable(
        "isosurfaceSettingsTable", 2, sk_isosurfaceTableFlags, outerSize, innerWidthToUse
      ))
  {
    // Declare columns:
    ImGui::TableSetupColumn("Surface", sk_nameColumnFlags, 150.0f, TableColumnId::Name);
    ImGui::TableSetupColumn("Isovalue", sk_isoValueColumnFlags, 150.0f, TableColumnId::Value);

    ImGui::TableSetupScrollFreeze(sk_freezeCols, sk_freezeRows);

    // Sort the table items if sort specifications have been changed
    if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs())
    {
      // Force the sort to always happen:
      if (sortSpecs->SpecsDirty | true)
      {
        itemsNeedSort = true;
      }

      if (itemsNeedSort && tableItems.size() > 1)
      {
        std::sort(
          std::begin(tableItems),
          std::end(tableItems),
          [sortSpecs](const IsosurfaceTableItem& a, const IsosurfaceTableItem& b)
          { return compareWithSortSpecs(a, b, *sortSpecs); }
        );

        sortSpecs->SpecsDirty = false;
      }
    }

    itemsNeedSort = false;

    if (sk_showHeaders)
    {
      ImGui::TableHeadersRow();
    }

    ImGui::PushButtonRepeat(true);

    // Always selected at least one item (the first one, by default):
    if (!selectedSurfaceUid)
    {
      selectedSurfaceUid = tableItems.front().m_surfaceUid;
      imageToSelectedSurfaceUid[imageUid] = tableItems.front().m_surfaceUid;
    }

    for (IsosurfaceTableItem& item : tableItems)
    {
      const bool itemIsSelected = (selectedSurfaceUid && *selectedSurfaceUid == item.m_surfaceUid);

      /*** PushID item.surfaceUid ***/
      ImGui::PushID(uuids::to_string(item.m_surfaceUid).c_str());

      ImGui::TableNextRow(ImGuiTableRowFlags_None, sk_minRowHeight);

      // Column with visibility checkbox, color picker, and name field:
      ImGui::TableSetColumnIndex(TableColumnId::Name);

      ImGui::Checkbox("##visible", &(item.m_surface->visible));
      ImGui::SameLine();

      glm::vec4 color
        = getIsosurfaceColor(appData, *(item.m_surface), imgSettings, componentToAdjust);

      // Disable editing the surface color when the image colormap is used:
      const bool disableEdit = imgSettings.applyImageColormapToIsosurfaces();

      const ImGuiColorEditFlags disableEditFlag = (disableEdit) ? ImGuiColorEditFlags_NoPicker : 0;

      if (ImGui::ColorEdit4(
            "##color", glm::value_ptr(color), sk_colorAlphaEditFlags | disableEditFlag
          ))
      {
        if (!disableEdit)
        {
          item.m_surface->color = glm::vec3{color};
          item.m_surface->opacity = color.a;
        }
      }

      ImGui::SameLine();

      if (ImGui::Selectable(
            item.m_surface->name.c_str(),
            itemIsSelected,
            sk_selectableFlags,
            ImVec2(0, sk_minRowHeight)
          ))
      {
        selectedSurfaceUid = item.m_surfaceUid;
        imageToSelectedSurfaceUid[imageUid] = item.m_surfaceUid;
      }

      // Column with isosurface value:
      if (ImGui::TableSetColumnIndex(TableColumnId::Value))
      {
        const auto& stats = image->settings().componentStatistics(componentToAdjust);

        //                const double k_step = ( valueMax - valueMin ) / 2000.0;
        //                const double k_stepFast = sk_step / 100.0;

        static constexpr double sk_step = 0.1;
        static constexpr double sk_stepFast = 10.0;

        ImGui::PushItemWidth(-1);

        double value = item.m_surface->value;

        if (ImGui::InputDouble(
              "##isovalue",
              &value,
              sk_step,
              sk_stepFast,
              appData.guiData().m_imageValuePrecisionFormat.c_str()
            ))
        {
          if (stats.m_minimum <= value && value <= stats.m_maximum)
          {
            item.m_surface->value = value;
          }

          // To avoid triggering a sort while holding the button;
          // only trigger it when the button has been released
          if (ImGui::IsItemDeactivated())
          {
            itemsNeedSort = true;
          }
        }

        ImGui::PopItemWidth();
      }

      ImGui::PopID(); /*** item.surfaceUid ***/
    }
    ImGui::PopButtonRepeat();

    ImGui::EndTable();
  }

  ImGui::Spacing();
  const bool addSurface = ImGui::Button(sk_addSurfaceButtonText.c_str());
  if (ImGui::IsItemHovered())
  {
    ImGui::SetTooltip("Add new isosurface");
  }

  if (addSurface)
  {
    if (const auto uid = addNewSurface(appData, image, imageUid, componentToAdjust, tableItems.size() + 1, storeFuture, addTaskToIsosurfaceGpuMeshGenerationQueue))
    {
      selectedSurfaceUid = *uid;
      imageToSelectedSurfaceUid[imageUid] = *uid;
      return;
    }
  }

  if (selectedSurfaceUid)
  {
    ImGui::SameLine();
    const bool removeSurface = ImGui::Button(sk_removeSurfaceButtonText.c_str());
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("Remove isosurface");
    }

    if (removeSurface)
    {
      if (appData.removeIsosurface(imageUid, componentToAdjust, *selectedSurfaceUid))
      {
        spdlog::info("Removed isosurface {}", *selectedSurfaceUid);
        selectedSurfaceUid = std::nullopt;
        imageToSelectedSurfaceUid.erase(imageUid);
        return;
      }
    }

    ImGui::SameLine();
    const bool saveSurface = ImGui::Button(sk_saveSurfacesButtonText.c_str());
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("Save isosurface...");
    }

    if (saveSurface)
    {
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    Isosurface* surface = appData.isosurface(imageUid, componentToAdjust, *selectedSurfaceUid);

    // Open Surface Properties on first appearance
    ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);

    if (ImGui::TreeNode("Properties"))
    {
      ImGui::InputText("Name", &surface->name);
      ImGui::SameLine();
      helpMarker("Edit the name of the surface");

      const double valueMin = image->settings().componentStatistics(componentToAdjust).m_minimum;
      const double valueMax = image->settings().componentStatistics(componentToAdjust).m_maximum;

      if (mySliderF64(
            "Isovalue",
            &(surface->value),
            valueMin,
            valueMax,
            appData.guiData().m_imageValuePrecisionFormat.c_str()
          ))
      {
        // updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Surface iso-value");

      ImGui::Spacing();
      ImGui::Checkbox("Visible", &surface->visible);
      ImGui::SameLine();
      helpMarker("Show/hide the surface");

      glm::vec4 color = getIsosurfaceColor(appData, *surface, imgSettings, componentToAdjust);
      glm::vec3 color3{color};

      // Disable editing the surface color when the image colormap is used:
      const bool disableEdit = imgSettings.applyImageColormapToIsosurfaces();

      const ImGuiColorEditFlags disableEditFlag = (disableEdit) ? ImGuiColorEditFlags_NoPicker : 0;

      if (ImGui::ColorEdit3(
            "Color", glm::value_ptr(color3), sk_colorNoAlphaEditFlags | disableEditFlag
          ))
      {
        if (!disableEdit)
        {
          surface->color = color3;
        }
      }
      ImGui::SameLine();
      helpMarker("Surface color");

      mySliderF32("Opacity", &surface->opacity, 0.0f, 1.0f);
      ImGui::SameLine();
      helpMarker("Surface opacity");

      int edgeStrength = surface->edgeStrength;
      if (mySliderS32("Edges", &edgeStrength, 0, 5))
      {
        surface->edgeStrength = static_cast<float>(edgeStrength);
      }

      ImGui::SameLine();
      helpMarker("Strength of surface edges");

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::TreePop();
    }

    if (ImGui::TreeNode("Image Settings"))
    {
      ImGui::Text("Settings for all image isosurfaces: ");
      ImGui::Spacing();

      bool hideAll = !imgSettings.isosurfacesVisible();
      if (ImGui::Checkbox("Hide all isosurfaces", &hideAll))
      {
        imgSettings.setIsosurfacesVisible(!hideAll);
      }
      ImGui::SameLine();
      helpMarker("Hide all isosurfaces");

      if (imgSettings.isosurfacesVisible())
      {
        bool showIn2d = imgSettings.showIsosurfacesIn2d();
        if (ImGui::Checkbox("Show isosurface outlines in 2D", &showIn2d))
        {
          imgSettings.setShowIsosurfacesIn2d(showIn2d);
        }
        ImGui::SameLine();
        helpMarker("Show isosurface outlines in 2D image planes");

        bool applyColormap = imgSettings.applyImageColormapToIsosurfaces();
        if (ImGui::Checkbox("Color isosurfaces using image colormap", &applyColormap))
        {
          imgSettings.setApplyImageColormapToIsosurfaces(applyColormap);
        }
        ImGui::SameLine();
        helpMarker("Color isosurfaces using the image colormap");

        bool useDistMap = imgSettings.useDistanceMapForRaycasting();
        if (ImGui::Checkbox("Accelerate raycasting using distance map", &useDistMap))
        {
          imgSettings.setUseDistanceMapForRaycasting(useDistMap);
        }
        ImGui::SameLine();
        helpMarker("Accelerate raycasting using distance map");

        float opacityMod = imgSettings.isosurfaceOpacityModulator();
        if (mySliderF32("Global opacity", &opacityMod, 0.0f, 1.0f, "%0.2f"))
        {
          imgSettings.setIsosurfaceOpacityModulator(opacityMod);
        }
        ImGui::SameLine();
        helpMarker("Global opacity modulator for all image isosurfaces");

        if (imgSettings.showIsosurfacesIn2d())
        {
          float width = static_cast<float>(imgSettings.isosurfaceWidthIn2d());
          // if ( mySliderF32( "Iso-line width", &width, 0.001f, 10.000f, "%0.3f \%" ) )
          if (ImGui::DragFloat(
                "Iso-line width", &width, 0.001f, 0.001f, 10.000f, "%0.3f \%", ImGuiSliderFlags_AlwaysClamp
              ))
          {
            imgSettings.setIsosurfaceWidthIn2d(static_cast<double>(width));
          }
          ImGui::SameLine();
          helpMarker(
            "Width of isosurface lines in 2D views, as a percentage of the image intensity range"
          );
        }
      }

      ImGui::TreePop();
    }
  }

  ImGui::PopID(); /** PopID surfaceUid **/
}
