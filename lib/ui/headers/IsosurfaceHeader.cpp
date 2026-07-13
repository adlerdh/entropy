#include "ui/headers/IsosurfaceHeader.h"
#include "ui/Helpers.h"
#include "ui/IsosurfaceRangeModel.h"
#include "ui/headers/HeaderCommon.h"

#include "logic/SurfaceUtility.h"
#include "logic/app/Data.h"

#include <IconsForkAwesome.h>

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
static constexpr const char* k_addSurfacesPopupName = "Add Isosurfaces";
static constexpr glm::vec3 k_defaultIsosurfaceColor{63.0f / 255.0f, 127.0f / 255.0f, 255.0f / 255.0f};
static constexpr glm::vec3 k_defaultIsosurfaceRangeEndColor{255.0f / 255.0f, 63.0f / 255.0f, 63.0f / 255.0f};

struct AddSurfacesDialogState
{
  ui::IsosurfaceRangeParameters range;
  bool colorRange = false;
  glm::vec3 startColor{k_defaultIsosurfaceColor};
  glm::vec3 endColor{k_defaultIsosurfaceRangeEndColor};
};

std::unordered_map<uuids::uuid, AddSurfacesDialogState> addSurfacesDialogStateByImage;

glm::vec3 defaultIsosurfaceColor(const glm::vec3& imageBorderColor)
{
  glm::vec3 hsv = glm::hsvColor(glm::clamp(imageBorderColor, glm::vec3{0.0f}, glm::vec3{1.0f}));
  hsv.y = std::clamp(0.5f * hsv.y, 0.0f, 1.0f);
  return glm::clamp(glm::rgbColor(hsv), glm::vec3{0.0f}, glm::vec3{1.0f});
}

/**
 * @brief Compute the ImGui header background and text colors from an image border color.
 *
 * @param color RGB image border color.
 * @return Pair of header background color and readable text color.
 */
std::pair<ImVec4, ImVec4> computeHeaderBgAndTextColors(const glm::vec3& color)
{
  glm::vec3 darkerBorderColorHsv = glm::hsvColor(color);
  darkerBorderColorHsv[2] = std::max(0.5f * darkerBorderColorHsv[2], 0.0f);
  const glm::vec3 darkerBorderColorRgb = glm::rgbColor(darkerBorderColorHsv);

  const ImVec4 headerColor(darkerBorderColorRgb.r, darkerBorderColorRgb.g, darkerBorderColorRgb.b, 1.0f);
  const ImVec4 headerTextColor = (glm::luminosity(darkerBorderColorRgb) < 0.75f) ? sk_whiteText : sk_blackText;

  return {headerColor, headerTextColor};
}

/**
 * @brief Add an isosurface with a specific isovalue for one image component.
 *
 * @param appData Application data store that owns images and isosurfaces.
 * @param image Image receiving the new isosurface.
 * @param imageUid UID of the image receiving the new isosurface.
 * @param component Image component index receiving the new isosurface.
 * @param index One-based display index used to build the default surface name.
 * @param value Isovalue for the new surface.
 * @param color Display color for the new surface.
 * @param storeFuture Callback reserved for asynchronous mesh-generation task ownership.
 * @param addTaskToIsosurfaceGpuMeshGenerationQueue Callback reserved for queueing GPU mesh generation.
 * @return UID of the created isosurface, or std::nullopt when creation fails.
 */
std::optional<uuids::uuid> addSurfaceAtValue(
  AppData& appData,
  const Image* image,
  const uuids::uuid& imageUid,
  uint32_t component,
  size_t index,
  double value,
  const glm::vec3& color,
  std::function<void(const uuids::uuid& taskUid, std::future<AsyncTaskDetails> future)> /*storeFuture*/,
  std::function<void(const uuids::uuid& taskUid)> /*addTaskToIsosurfaceGpuMeshGenerationQueue*/
)
{
  if (!image) {
    return std::nullopt;
  }

  Isosurface surface;
  surface.name = ui::defaultIsosurfaceName(index);
  surface.value = value;
  surface.color = color;
  surface.opacity = 1.0f;
  surface.meshInSync = false;

  if (const auto isosurfaceUid = appData.addIsosurface(imageUid, component, std::move(surface))) {
    spdlog::debug(
      "Added new isosurface {} for image {} (component {}) at isovalue {}",
      *isosurfaceUid,
      imageUid,
      component,
      value);

#if 0
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
        value,
        *isosurfaceUid,
        meshCpuRecordUpdater,
        std::bind(addTaskToIsosurfaceGpuMeshGenerationQueue, taskUid)
      )
    );
#endif

    return isosurfaceUid;
  }

  spdlog::error("Unable to add new isosurface for image {}", imageUid);
  return std::nullopt;
}

/**
 * @brief Open and render the batch isosurface creation dialog.
 *
 * @param appData Application data store that owns images and isosurfaces.
 * @param image Image receiving the new isosurfaces.
 * @param imageUid UID of the image receiving the new isosurfaces.
 * @param component Image component index receiving the new isosurfaces.
 * @param nextSurfaceIndex One-based display index for the first generated surface.
 * @param selectedSurfaceUid Selection updated to the last generated surface.
 * @param imageToSelectedSurfaceUid Per-image surface selection map.
 * @param storeFuture Callback reserved for asynchronous mesh-generation task ownership.
 * @param addTaskToIsosurfaceGpuMeshGenerationQueue Callback reserved for queueing GPU mesh generation.
 * @return True when isosurfaces were added.
 */
bool renderAddSurfacesDialog(
  AppData& appData,
  const Image* image,
  const uuids::uuid& imageUid,
  uint32_t component,
  size_t nextSurfaceIndex,
  std::optional<uuids::uuid>& selectedSurfaceUid,
  std::unordered_map<uuids::uuid, uuids::uuid>& imageToSelectedSurfaceUid,
  std::function<void(const uuids::uuid& taskUid, std::future<AsyncTaskDetails> future)> storeFuture,
  std::function<void(const uuids::uuid& taskUid)> addTaskToIsosurfaceGpuMeshGenerationQueue)
{
  if (!image) {
    return false;
  }

  if (ImGui::BeginPopupModal(k_addSurfacesPopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    auto& state = addSurfacesDialogStateByImage[imageUid];
    auto& params = state.range;
    if (params.count == 0) {
      params.count = 2;
      ui::updateIsosurfaceRangeSpacing(params);
    }

    const auto& stats = image->settings().componentStatistics(component);
    const double valueMin = static_cast<double>(stats.onlineStats.min);
    const double valueMax = static_cast<double>(stats.onlineStats.max);

    ImGui::TextUnformatted("Add isosurfaces over a scalar range.");
    ImGui::Spacing();

    ImGui::PushItemWidth(160.0f);

    bool startChanged = false;
    bool endChanged = false;
    if (ImGui::InputDouble("Start", &params.start, 0.0, 0.0, appData.guiData().m_imageValuePrecisionFormat.c_str())) {
      startChanged = true;
    }

    if (ImGui::InputDouble("End", &params.end, 0.0, 0.0, appData.guiData().m_imageValuePrecisionFormat.c_str())) {
      endChanged = true;
    }

    if (startChanged || endChanged) {
      params.start = std::clamp(params.start, valueMin, valueMax);
      params.end = std::clamp(params.end, valueMin, valueMax);

      if (startChanged && params.start > params.end) {
        params.end = params.start;
      }
      else if (endChanged && params.end < params.start) {
        params.start = params.end;
      }

      ui::updateIsosurfaceRangeSpacing(params);
    }

    if (ImGui::InputDouble("Spacing", &params.spacing, 0.0, 0.0, appData.guiData().m_imageValuePrecisionFormat.c_str()))
    {
      ui::updateIsosurfaceRangeCount(params);
    }

    static constexpr std::uint32_t k_countStep = 1;
    static constexpr std::uint32_t k_countStepFast = 10;
    if (ImGui::InputScalar("Count", ImGuiDataType_U32, &params.count, &k_countStep, &k_countStepFast)) {
      ui::updateIsosurfaceRangeSpacing(params);
    }

    ImGui::PopItemWidth();

    ImGui::Spacing();
    ImGui::Checkbox("Color range", &state.colorRange);
    if (state.colorRange) {
      static const ImGuiColorEditFlags k_colorPickerFlags =
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_DisplayRGB |
        ImGuiColorEditFlags_InputRGB;

      if (ImGui::ColorEdit3("Start color", glm::value_ptr(state.startColor), k_colorPickerFlags)) {
        state.startColor = glm::clamp(state.startColor, glm::vec3{0.0f}, glm::vec3{1.0f});
      }

      if (ImGui::ColorEdit3("End color", glm::value_ptr(state.endColor), k_colorPickerFlags)) {
        state.endColor = glm::clamp(state.endColor, glm::vec3{0.0f}, glm::vec3{1.0f});
      }
    }

    ImGui::Spacing();
    const auto values = ui::isosurfaceRangeValues(params);
    ImGui::Text("Will add %zu isosurface%s.", values.size(), values.size() == 1 ? "" : "s");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Button("OK")) {
      std::optional<uuids::uuid> lastAddedUid;
      size_t surfaceIndex = nextSurfaceIndex;
      for (std::size_t valueIndex = 0; valueIndex < values.size(); ++valueIndex) {
        const double value = values[valueIndex];
        const double t =
          (values.size() <= 1) ? 0.0 : static_cast<double>(valueIndex) / static_cast<double>(values.size() - 1);
        const glm::vec3 color = state.colorRange ? ui::interpolateHsvColor(state.startColor, state.endColor, t)
                                                 : defaultIsosurfaceColor(image->settings().borderColor());

        lastAddedUid = addSurfaceAtValue(
          appData,
          image,
          imageUid,
          component,
          surfaceIndex,
          value,
          color,
          storeFuture,
          addTaskToIsosurfaceGpuMeshGenerationQueue);
        ++surfaceIndex;
      }

      if (lastAddedUid) {
        selectedSurfaceUid = *lastAddedUid;
        imageToSelectedSurfaceUid[imageUid] = *lastAddedUid;
      }

      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
      return lastAddedUid.has_value();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  return false;
}

/**
 * @brief Initialize and open the batch isosurface creation dialog.
 *
 * @param imageUid UID of the image receiving the new isosurfaces.
 * @param image Image receiving the new isosurfaces.
 * @param component Image component index receiving the new isosurfaces.
 */
void openAddSurfacesDialog(const uuids::uuid& imageUid, const Image& image, uint32_t component)
{
  static constexpr std::uint32_t k_defaultRangeCount = 5;

  const auto& stats = image.settings().componentStatistics(component);
  auto& state = addSurfacesDialogStateByImage[imageUid];
  auto& params = state.range;
  params.start = static_cast<double>(stats.onlineStats.min);
  params.end = static_cast<double>(stats.onlineStats.max);
  params.count = k_defaultRangeCount;
  ui::updateIsosurfaceRangeSpacing(params);
  state.startColor = defaultIsosurfaceColor(image.settings().borderColor());
  state.endColor = k_defaultIsosurfaceRangeEndColor;

  ImGui::OpenPopup(k_addSurfacesPopupName);
}

static const ImGuiTableFlags sk_isosurfaceTableFlags =
  ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable |
  ImGuiTableFlags_SortMulti | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBody |
  ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

/**
 * @brief Isosurface table column identifiers used for ImGui sorting.
 */
enum TableColumnId : uint32_t
{
  /// "Name" column shows several things: visibility checkbox, surface name, surface color picker
  Name = 0,

  /// "Value" column shows isosurface value input selector
  Value = 1
};

static const ImGuiTableColumnFlags sk_nameColumnFlags = ImGuiTableColumnFlags_DefaultSort |
                                                        ImGuiTableColumnFlags_PreferSortDescending |
                                                        ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide;

static const ImGuiTableColumnFlags sk_isoValueColumnFlags =
  ImGuiTableColumnFlags_PreferSortDescending | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide;

//        ( sk_isosurfaceTableFlags & ImGuiTableFlags_NoHostExtendX ) ? 0 :
//        ImGuiTableColumnFlags_WidthStretch;

/**
 * @brief Represents a table row for one isosurface
 */
struct IsosurfaceTableItem
{
  IsosurfaceTableItem(const uuids::uuid& surfaceUid, Isosurface* surface) : m_surfaceUid(surfaceUid), m_surface(surface)
  {
  }

  uuids::uuid m_surfaceUid; //!< UID of the surface
  Isosurface* m_surface;    //!< Reference to the surface
};

enum class IsosurfaceTableItemContentsType
{
  Selectable,       //!< Only the visible selectable item reacts to selection.
  SelectableSpanRow //!< The selectable item spans the full row.
};

/**
 * @brief Compare two isosurface table rows using the current ImGui table sort specification.
 *
 * @param a First item to compare.
 * @param b Second item to compare.
 * @param sortSpecs Active ImGui table sort specification.
 * @return True when @p a should be ordered before @p b.
 */
bool compareWithSortSpecs(
  const IsosurfaceTableItem& a,
  const IsosurfaceTableItem& b,
  const ImGuiTableSortSpecs& sortSpecs)
{
  for (int i = 0; i < sortSpecs.SpecsCount; ++i) {
    const ImGuiTableColumnSortSpecs& spec = sortSpecs.Specs[i];

    double delta = 0.0;

    switch (spec.ColumnUserID) {
      case TableColumnId::Name: {
        delta = static_cast<double>(a.m_surface->name.compare(b.m_surface->name));
        break;
      }
      case TableColumnId::Value: {
        delta = a.m_surface->value - b.m_surface->value;
        break;
      }
      default: {
        IM_ASSERT(0);
        break;
      }
    }

    if (delta > 0.0) {
      return (spec.SortDirection == ImGuiSortDirection_Ascending) ? true : false;
    }
    else if (delta <= 0.0) {
      return (spec.SortDirection == ImGuiSortDirection_Ascending) ? false : true;
    }
  }

  return (a.m_surface->value > b.m_surface->value);
}

/**
 * @brief Add a default isosurface for one image component.
 *
 * @param appData Application data store that owns images and isosurfaces.
 * @param image Image that provides component statistics for the default isovalue.
 * @param imageUid UID of the image receiving the new isosurface.
 * @param component Image component index receiving the new isosurface.
 * @param index One-based display index used to build the default surface name.
 * @param storeFuture Callback reserved for asynchronous mesh-generation task ownership.
 * @param addTaskToIsosurfaceGpuMeshGenerationQueue Callback reserved for queueing GPU mesh generation.
 * @return UID of the created isosurface, or std::nullopt when creation fails.
 */
std::optional<uuids::uuid> addNewSurface(
  AppData& appData,
  const Image* image,
  const uuids::uuid& imageUid,
  uint32_t component,
  size_t index,
  std::function<void(const uuids::uuid& taskUid, std::future<AsyncTaskDetails> future)> storeFuture,
  std::function<void(const uuids::uuid& taskUid)> addTaskToIsosurfaceGpuMeshGenerationQueue)
{
  static constexpr uint32_t k_defaultIsovalueQuantile = 75;

  if (!image) {
    return std::nullopt;
  }

  const auto& stats = image->settings().componentStatistics(component);
  return addSurfaceAtValue(
    appData,
    image,
    imageUid,
    component,
    index,
    static_cast<double>(stats.quantiles[k_defaultIsovalueQuantile]),
    defaultIsosurfaceColor(image->settings().borderColor()),
    storeFuture,
    addTaskToIsosurfaceGpuMeshGenerationQueue);
}

} // namespace

void renderIsosurfacesHeader(
  AppData& appData,
  const uuids::uuid& imageUid,
  std::size_t imageIndex,
  bool isActiveImage,
  bool hasFollowingHeader,
  std::function<void(const uuids::uuid& taskUid, std::future<AsyncTaskDetails> future)> storeFuture,
  std::function<void(const uuids::uuid& taskUid)> addTaskToIsosurfaceGpuMeshGenerationQueue)
{
  static const ImGuiColorEditFlags sk_colorNoAlphaEditFlags =
    ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHex |
    ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_InputRGB;

  static const ImGuiColorEditFlags sk_colorAlphaEditFlags =
    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_DisplayRGB |
    ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf |
    ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_InputRGB;

  static const std::string sk_addSurfaceButtonText = std::string(ICON_FK_FILE_O) + std::string(" Add");
  static const std::string sk_addSurfacesButtonText = std::string(ICON_FK_FILE_TEXT_O) + std::string(" Add range...");
  static const std::string sk_removeSurfaceButtonText = std::string(ICON_FK_TRASH_O) + std::string(" Remove");
  static const std::string sk_saveSurfacesButtonText = std::string(ICON_FK_FLOPPY_O) + std::string(" Save...");

  //    static const char* sk_saveSurfaceDialogTitle( "Save Isosurface Mesh" );
  //    static const std::vector< const char* > sk_saveSurfaceDialogFilters{};

  static const float sk_textBaseHeight = ImGui::GetTextLineHeightWithSpacing();

  static const IsosurfaceTableItemContentsType sk_contentsType = IsosurfaceTableItemContentsType::SelectableSpanRow;

  static const ImGuiSelectableFlags sk_selectableFlags =
    (IsosurfaceTableItemContentsType::SelectableSpanRow == sk_contentsType)
      ? (ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)
      : ImGuiSelectableFlags_None;

  static constexpr int sk_freezeCols = 1;
  static constexpr int sk_freezeRows = 1;

  static const ImVec2 sk_outerSizeValue(0.0f, sk_textBaseHeight * 12);
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
  if (!image) {
    return;
  }

  ImageSettings& imgSettings = image->settings();

  ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_CollapsingHeader;

  /// @todo This annoyingly pops up the active header each time... not sure why
  if (isActiveImage) {
    headerFlags |= ImGuiTreeNodeFlags_DefaultOpen;
  }

  ImGui::PushID(uuids::to_string(imageUid).c_str()); // imageUid

  // Header is ID'ed only by the image index.
  // ### allows the header name to change without changing its ID.
  const bool isRef = appData.refImageUid() && *appData.refImageUid() == imageUid;
  const std::string headerName =
    std::to_string(imageIndex) + ") " +
    ui::headers::imageDisplayNameWithRole(image->settings().displayName(), isRef, isActiveImage, appData.numImages()) +
    "###" + std::to_string(imageIndex);

  const auto headerColors = computeHeaderBgAndTextColors(image->settings().borderColor());
  ImGui::PushStyleColor(ImGuiCol_Header, headerColors.first);
  ImGui::PushStyleColor(ImGuiCol_Text, headerColors.second);

  const bool requestedHeader =
    appData.guiData().m_requestedIsosurfacesImageUid && *appData.guiData().m_requestedIsosurfacesImageUid == imageUid;
  if (
    requestedHeader ||
    (isActiveImage && (appData.guiData().m_requestAddIsosurface || appData.guiData().m_requestAddIsosurfaceRange)))
  {
    ImGui::SetNextItemOpen(true, ImGuiCond_Always);
  }
  const bool open =
    ui::headers::activeImageCollapsingHeader(appData.guiData(), isActiveImage, headerName.c_str(), headerFlags);
  if (requestedHeader) {
    appData.guiData().m_requestedIsosurfacesImageUid = std::nullopt;
  }

  ImGui::PopStyleColor(2); // ImGuiCol_Header, ImGuiCol_Text

  if (!open) {
    ImGui::PopID(); // imageUid
    return;
  }

  ImGui::Spacing();

  // By default, adjust image component 0:
  static uint32_t componentToAdjust = 0;

  const bool showComponentSelection = image->header().numComponentsPerPixel() > 1;

  if (showComponentSelection) {
    if (ImGui::BeginCombo("Image component", std::to_string(componentToAdjust).c_str())) {
      for (uint32_t comp = 0; comp < image->header().numComponentsPerPixel(); ++comp) {
        const bool isSelected = (componentToAdjust == comp);
        if (ImGui::Selectable(std::to_string(comp).c_str(), isSelected)) {
          componentToAdjust = comp;
        }

        if (isSelected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    ImGui::SameLine();
    helpMarker("Select the image component for which to adjust isosurfaces");

    ImGui::Separator();
    ImGui::Spacing();
  }

  const auto isosurfaceUids = appData.isosurfaceUids(imageUid, componentToAdjust);
  bool addSurface = false;
  bool addSurfaces = false;
  if (isActiveImage && appData.guiData().m_requestAddIsosurface) {
    addSurface = true;
    appData.guiData().m_requestAddIsosurface = false;
  }
  if (isActiveImage && appData.guiData().m_requestAddIsosurfaceRange) {
    addSurfaces = true;
    appData.guiData().m_requestAddIsosurfaceRange = false;
  }

  if (isosurfaceUids.empty()) {
    ImGui::TextDisabled("This image has no isosurfaces.");

    ImGui::Spacing();
    addSurface = ImGui::Button(sk_addSurfaceButtonText.c_str()) || addSurface;
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Add new isosurface");
    }

    ImGui::SameLine();
    addSurfaces = ImGui::Button(sk_addSurfacesButtonText.c_str()) || addSurfaces;
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Add multiple isosurfaces");
    }

    if (addSurface) {
      if (
        const auto uid = addNewSurface(
          appData,
          image,
          imageUid,
          componentToAdjust,
          1,
          storeFuture,
          addTaskToIsosurfaceGpuMeshGenerationQueue))
      {
        selectedSurfaceUid = *uid;
        imageToSelectedSurfaceUid[imageUid] = *uid;
      }
      if (hasFollowingHeader) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
      }
      ImGui::PopID(); // imageUid
      return;
    }

    if (addSurfaces) {
      openAddSurfacesDialog(imageUid, *image, componentToAdjust);
    }

    if (renderAddSurfacesDialog(
          appData,
          image,
          imageUid,
          componentToAdjust,
          1,
          selectedSurfaceUid,
          imageToSelectedSurfaceUid,
          storeFuture,
          addTaskToIsosurfaceGpuMeshGenerationQueue))
    {
      if (hasFollowingHeader) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
      }
      ImGui::PopID(); // imageUid
      return;
    }

    if (hasFollowingHeader) {
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
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
  if (std::end(imageToSelectedSurfaceUid) == it) {
    selectedSurfaceUid = std::nullopt;
  }
  else {
    selectedSurfaceUid = it->second;
  }

  for (const auto& uid : appData.isosurfaceUids(imageUid, componentToAdjust)) {
    Isosurface* surface = appData.isosurface(imageUid, componentToAdjust, uid);
    if (!surface) {
      spdlog::error("Isosurface {} is null: it is being removed", uid);
      appData.removeIsosurface(imageUid, componentToAdjust, uid);
      continue;
    }

    tableItems.push_back(IsosurfaceTableItem(uid, surface));

    // The selected UID is valid if there is a surface with this UID
    validSelectedUid = validSelectedUid | (selectedSurfaceUid && *selectedSurfaceUid == uid);
  }

  if (selectedSurfaceUid && !validSelectedUid) {
    // Selected UID was invalid, so remove it
    spdlog::warn("Invalid isosurface UID {} selected", *selectedSurfaceUid);
    selectedSurfaceUid = std::nullopt;
    imageToSelectedSurfaceUid.erase(imageUid);
  }

  const float innerWidthToUse = (sk_isosurfaceTableFlags & ImGuiTableFlags_ScrollX) ? sk_innerWidthWithScroll : 0.0f;

  const ImVec2 outerSize = sk_outerSizeEnabled ? sk_outerSizeValue : ImVec2(0, 0);

  if (ImGui::BeginTable("isosurfaceSettingsTable", 2, sk_isosurfaceTableFlags, outerSize, innerWidthToUse)) {
    // Declare columns:
    ImGui::TableSetupColumn("Surface", sk_nameColumnFlags, 150.0f, TableColumnId::Name);
    ImGui::TableSetupColumn("Isovalue", sk_isoValueColumnFlags, 150.0f, TableColumnId::Value);

    ImGui::TableSetupScrollFreeze(sk_freezeCols, sk_freezeRows);

    // Sort the table items if sort specifications have been changed
    if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
      // Force the sort to always happen:
      if (sortSpecs->SpecsDirty | true) {
        itemsNeedSort = true;
      }

      if (itemsNeedSort && tableItems.size() > 1) {
        std::sort(
          std::begin(tableItems),
          std::end(tableItems),
          [sortSpecs](const IsosurfaceTableItem& a, const IsosurfaceTableItem& b) {
            return compareWithSortSpecs(a, b, *sortSpecs);
          });

        sortSpecs->SpecsDirty = false;
      }
    }

    itemsNeedSort = false;

    if (sk_showHeaders) {
      ImGui::TableHeadersRow();
    }

    ImGui::PushButtonRepeat(true);

    // Always selected at least one item (the first one, by default):
    if (!selectedSurfaceUid) {
      selectedSurfaceUid = tableItems.front().m_surfaceUid;
      imageToSelectedSurfaceUid[imageUid] = tableItems.front().m_surfaceUid;
    }

    for (IsosurfaceTableItem& item : tableItems) {
      ImGui::PushID(uuids::to_string(item.m_surfaceUid).c_str()); // item.surfaceUid

      ImGui::TableNextRow(ImGuiTableRowFlags_None, sk_minRowHeight);

      // Column with visibility checkbox, color picker, and name field:
      ImGui::TableSetColumnIndex(TableColumnId::Name);

      ImGui::Checkbox("##visible", &(item.m_surface->visible));
      ImGui::SameLine();

      // Non-pm-RGBA:
      static constexpr bool premult = false;
      glm::vec4 color = getIsosurfaceColor(appData, *(item.m_surface), imgSettings, componentToAdjust, premult);

      // Disable editing the surface color when the image colormap is used:
      const bool disableEdit = imgSettings.applyImageColormapToIsosurfaces();

      const ImGuiColorEditFlags disableEditFlag = (disableEdit) ? ImGuiColorEditFlags_NoPicker : 0;

      if (ImGui::ColorEdit4("##color", glm::value_ptr(color), sk_colorAlphaEditFlags | disableEditFlag)) {
        if (!disableEdit) {
          item.m_surface->color = glm::vec3{color};
          item.m_surface->opacity = color.a;
        }
      }

      ImGui::SameLine();

      const bool itemIsSelected = (selectedSurfaceUid && *selectedSurfaceUid == item.m_surfaceUid);
      if (ImGui::Selectable(
            item.m_surface->name.c_str(),
            itemIsSelected,
            sk_selectableFlags,
            ImVec2(0, sk_minRowHeight)))
      {
        selectedSurfaceUid = item.m_surfaceUid;
        imageToSelectedSurfaceUid[imageUid] = item.m_surfaceUid;
      }

      // Column with isosurface value:
      if (ImGui::TableSetColumnIndex(TableColumnId::Value)) {
        static constexpr double sk_step = 0.1;
        static constexpr double sk_stepFast = 10.0;

        const auto& stats = image->settings().componentStatistics(componentToAdjust);

        //                const double k_step = ( valueMax - valueMin ) / 2000.0;
        //                const double k_stepFast = sk_step / 100.0;

        ImGui::PushItemWidth(-1);

        double value = item.m_surface->value;

        if (ImGui::InputDouble(
              "##isovalue",
              &value,
              sk_step,
              sk_stepFast,
              appData.guiData().m_imageValuePrecisionFormat.c_str()))
        {
          if (stats.onlineStats.min <= value && value <= stats.onlineStats.max) {
            item.m_surface->value = value;
          }

          // To avoid triggering a sort while holding the button;
          // only trigger it when the button has been released
          if (ImGui::IsItemDeactivated()) {
            itemsNeedSort = true;
          }
        }

        ImGui::PopItemWidth();
      }

      ImGui::PopID(); // item.surfaceUid
    }
    ImGui::PopButtonRepeat();

    ImGui::EndTable();
  }

  ImGui::Spacing();
  addSurface = ImGui::Button(sk_addSurfaceButtonText.c_str()) || addSurface;
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Add new isosurface");
  }

  ImGui::SameLine();
  addSurfaces = ImGui::Button(sk_addSurfacesButtonText.c_str()) || addSurfaces;
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Add multiple isosurfaces");
  }

  if (addSurface) {
    if (
      const auto uid = addNewSurface(
        appData,
        image,
        imageUid,
        componentToAdjust,
        tableItems.size() + 1,
        storeFuture,
        addTaskToIsosurfaceGpuMeshGenerationQueue))
    {
      selectedSurfaceUid = *uid;
      imageToSelectedSurfaceUid[imageUid] = *uid;
      ImGui::PopStyleColor();
      ImGui::PopID(); // imageUid
      return;
    }
  }

  if (addSurfaces) {
    openAddSurfacesDialog(imageUid, *image, componentToAdjust);
  }

  if (renderAddSurfacesDialog(
        appData,
        image,
        imageUid,
        componentToAdjust,
        tableItems.size() + 1,
        selectedSurfaceUid,
        imageToSelectedSurfaceUid,
        storeFuture,
        addTaskToIsosurfaceGpuMeshGenerationQueue))
  {
    ImGui::PopStyleColor();
    ImGui::PopID(); // imageUid
    return;
  }

  if (selectedSurfaceUid) {
    ImGui::SameLine();
    const bool removeSurface = ImGui::Button(sk_removeSurfaceButtonText.c_str());
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Remove isosurface");
    }

    if (removeSurface) {
      if (appData.removeIsosurface(imageUid, componentToAdjust, *selectedSurfaceUid)) {
        spdlog::info("Removed isosurface {}", *selectedSurfaceUid);
        selectedSurfaceUid = std::nullopt;
        imageToSelectedSurfaceUid.erase(imageUid);
        ImGui::PopStyleColor();
        ImGui::PopID(); // imageUid
        return;
      }
    }

    ImGui::SameLine();
    const bool saveSurface = ImGui::Button(sk_saveSurfacesButtonText.c_str());
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Save isosurface...");
    }

    if (saveSurface) {
      /// @todo Save
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    Isosurface* surface = appData.isosurface(imageUid, componentToAdjust, *selectedSurfaceUid);

    // Open Surface Properties on first appearance
    ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);

    if (ImGui::TreeNode("Properties")) {
      ImGui::InputText("Name", &surface->name);
      ImGui::SameLine();
      helpMarker("Edit the name of the surface");

      const double valueMin =
        static_cast<double>(image->settings().componentStatistics(componentToAdjust).onlineStats.min);
      const double valueMax =
        static_cast<double>(image->settings().componentStatistics(componentToAdjust).onlineStats.max);

      if (mySliderF64(
            "Isovalue",
            &(surface->value),
            valueMin,
            valueMax,
            appData.guiData().m_imageValuePrecisionFormat.c_str()))
      {
        // updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Surface iso-value");

      ImGui::Spacing();
      ImGui::Checkbox("Visible", &surface->visible);
      ImGui::SameLine();
      helpMarker("Show/hide the surface");

      static constexpr bool premult = false;
      glm::vec4 color = getIsosurfaceColor(appData, *surface, imgSettings, componentToAdjust, premult);
      glm::vec3 color3{color};

      // Disable editing the surface color when the image colormap is used:
      const bool disableEdit = imgSettings.applyImageColormapToIsosurfaces();
      const ImGuiColorEditFlags disableEditFlag = (disableEdit) ? ImGuiColorEditFlags_NoPicker : 0;

      if (ImGui::ColorEdit3("Color", glm::value_ptr(color3), sk_colorNoAlphaEditFlags | disableEditFlag)) {
        if (!disableEdit) {
          surface->color = color3;
        }
      }
      ImGui::SameLine();
      helpMarker("Surface color");

      mySliderF32("Opacity", &surface->opacity, 0.0f, 1.0f);
      ImGui::SameLine();
      helpMarker("Surface (3D) and contour (2D) opacity");

      mySliderF32("Fill", &surface->fillOpacity, 0.0f, 1.0f);
      ImGui::SameLine();
      helpMarker("Fill opacity in 2D views");

      if (ImGui::TreeNode("Rim Lighting")) {
        ImGui::Checkbox("Enable", &surface->rimLightingEnabled);
        ImGui::SameLine();
        helpMarker("Enable view-angle rim opacity modulation and glow for this surface in 3D raycasting");

        if (!surface->rimLightingEnabled) {
          ImGui::BeginDisabled();
        }
        if (mySliderF32("Opacity", &surface->rimOpacityStrength, 0.0f, 1.0f, "%0.2f")) {
          surface->rimOpacityStrength = std::clamp(surface->rimOpacityStrength, 0.0f, 1.0f);
        }
        ImGui::SameLine();
        helpMarker("Modulate surface opacity by view angle so silhouettes remain more visible in 3D raycasting");

        if (mySliderF32("Glow", &surface->rimEmissionStrength, 0.0f, 2.0f, "%0.2f")) {
          surface->rimEmissionStrength = std::max(surface->rimEmissionStrength, 0.0f);
        }
        ImGui::SameLine();
        helpMarker("Add view-angle rim light at silhouettes in 3D raycasting");

        if (mySliderF32("Falloff", &surface->rimPower, 0.25f, 8.0f, "%0.2f")) {
          surface->rimPower = std::max(surface->rimPower, 0.25f);
        }
        ImGui::SameLine();
        helpMarker("Controls rim width; higher values make a narrower rim");
        if (!surface->rimLightingEnabled) {
          ImGui::EndDisabled();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TreePop();
      }

      ImGui::TreePop();
    }

    if (ImGui::TreeNode("Image Settings")) {
      ImGui::TextDisabled("Settings for all image isosurfaces:");
      ImGui::Spacing();

      bool hideAll = !imgSettings.isosurfacesVisible();
      if (ImGui::Checkbox("Hide all", &hideAll)) {
        imgSettings.setIsosurfacesVisible(!hideAll);
      }
      ImGui::SameLine();
      helpMarker("Hide all isosurfaces");

      if (imgSettings.isosurfacesVisible()) {
        bool showIn2d = imgSettings.showIsocontoursIn2D();
        if (ImGui::Checkbox("Show isocontours in 2D", &showIn2d)) {
          imgSettings.setShowIsoscontoursIn2D(showIn2d);
        }
        ImGui::SameLine();
        helpMarker("Show isocontours in 2D image planes");

        bool applyColormap = imgSettings.applyImageColormapToIsosurfaces();
        if (ImGui::Checkbox("Color using image colormap", &applyColormap)) {
          imgSettings.setApplyImageColormapToIsosurfaces(applyColormap);
        }
        ImGui::SameLine();
        helpMarker("Color isosurfaces using the image colormap");

        float opacityMod = imgSettings.isosurfaceOpacityModulator();
        if (mySliderF32("Global opacity", &opacityMod, 0.0f, 1.0f, "%0.2f")) {
          imgSettings.setIsosurfaceOpacityModulator(opacityMod);
        }
        ImGui::SameLine();
        helpMarker("Global opacity modulator for all image isosurfaces");

        if (imgSettings.showIsocontoursIn2D()) {
          float width = static_cast<float>(imgSettings.isoContourLineWidthIn2D());
          // if (ImGui::DragFloat("Iso-line width", &width, 0.001f, 0.001f, 10.000f, "%0.3f \%",
          // ImGuiSliderFlags_AlwaysClamp)) {
          if (mySliderF32("Isocontour width", &width, 1.0f, 10.0f, "%0.1f %%")) {
            imgSettings.setIsosurfaceWidthIn2d(static_cast<double>(width));
          }
          ImGui::SameLine();
          helpMarker("Width of isocontours in 2D views");
        }

        ImGui::Spacing();
        bool useDistMap = imgSettings.useDistanceMapForRaycasting();
        if (ImGui::Checkbox("Raycast using distance map", &useDistMap)) {
          imgSettings.setUseDistanceMapForRaycasting(useDistMap);
        }
        ImGui::SameLine();
        helpMarker("Accelerate raycasting using distance map to foreground mask");

        if (imgSettings.useDistanceMapForRaycasting()) {
          bool distMapChanged = false;
          const double valueMin =
            static_cast<double>(image->settings().componentStatistics(componentToAdjust).onlineStats.min);
          const double valueMax =
            static_cast<double>(image->settings().componentStatistics(componentToAdjust).onlineStats.max);

          double threshLow = imgSettings.foregroundThresholds(componentToAdjust).first;
          double threshHigh = imgSettings.foregroundThresholds(componentToAdjust).second;

          if (mySliderF64(
                "Low thresh.",
                &threshLow,
                valueMin,
                valueMax,
                appData.guiData().m_imageValuePrecisionFormat.c_str()))
          {
            if (threshLow <= threshHigh) {
              imgSettings.setForegroundThresholdLow(threshLow);
              distMapChanged = true;
            }
          }

          if (mySliderF64(
                "High thresh.",
                &threshHigh,
                valueMin,
                valueMax,
                appData.guiData().m_imageValuePrecisionFormat.c_str()))
          {
            if (threshLow <= threshHigh) {
              imgSettings.setForegroundThresholdHigh(threshHigh);
              distMapChanged = true;
            }
          }

          ImGui::SameLine();
          helpMarker(
            "Distance map is computed to foreground mask of image, which is defined by lower and "
            "upper thresholds");

          /// @todo If the thresholds changed, then create a new distance map (\c
          /// createDistanceMaps) and texture (\c createDistanceMapTextures)
          if (distMapChanged) {
            /// @todo create button "Regenerate distance map"
          }
        }
      }

      ImGui::TreePop();
    }
  }

  if (hasFollowingHeader) {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
  }
  ImGui::PopStyleColor();
  ImGui::PopID(); /** PopID surfaceUid **/
}
