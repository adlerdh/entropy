#include "ui/headers/Headers.h"
#include "ui/GuiData.h"
#include "ui/headers/HeaderCommon.h"
#include "ui/DicomMetadataTable.h"
#include "ui/Helpers.h"
#include "ui/ImageExport.h"
#include "ui/ImGuiCustomControls.h"
#include "ui/NativeFileDialogs.h"
#include "ui/widgets/Widgets.h"
#include "ui/widgets/ImageHistogram.h"

// data::roundPointToNearestImageVoxelCenter
// data::getAnnotationSubjectPlaneName
#include "logic/app/DataHelper.h"
#include "rendering/TextureSetup.h"

#include "image/DicomSeries.h"
#include "image/Image.h"
#include "image/ImageColorMap.h"
#include "image/ImageHeader.h"
#include "image/ImageSettings.h"
#include "image/ImageTransformations.h"
#include "image/ImageUtility.h"
#include "image/TimePlaybackController.h"

#include "logic/app/Data.h"
#include "logic/states/annotation/AnnotationStateMachine.h"

#include <IconsForkAwesome.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>
#include <imgui-knobs.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <implot/implot.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#undef min
#undef max

namespace fs = std::filesystem;
using namespace entropy::ui::headers;

namespace
{
constexpr double k_minTimePlaybackSpeed = 0.1;
constexpr double k_minTimePlaybackFramesPerSecond = 1.0;
constexpr double k_maxTimePlaybackFramesPerSecond = 120.0;

double minTimePlaybackSpeed(const Image& image)
{
  return std::min(
    k_minTimePlaybackSpeed,
    image.timeAxis().maxPlaybackSpeedForFrameRate(k_minTimePlaybackFramesPerSecond));
}

double maxTimePlaybackSpeed(const Image& image)
{
  return std::max(
    k_minTimePlaybackSpeed,
    image.timeAxis().maxPlaybackSpeedForFrameRate(k_maxTimePlaybackFramesPerSecond));
}

const char* vectorArrowOverlaySpacingModeName(VectorArrowOverlaySpacingMode mode)
{
  switch (mode) {
    case VectorArrowOverlaySpacingMode::Pixels:
      return "Pixels";
    case VectorArrowOverlaySpacingMode::Voxels:
      return "Voxels";
    case VectorArrowOverlaySpacingMode::Millimeters:
      return "Millimeters";
  }

  return "Pixels";
}

const char* vectorWarpedGridConventionName(VectorWarpedGridConvention convention)
{
  switch (convention) {
    case VectorWarpedGridConvention::SamplingField:
      return "Sampling field";
    case VectorWarpedGridConvention::ApparentDeformation:
      return "Apparent deformation";
  }

  return "Sampling field";
}

void disabledWrappedText(const char* text)
{
  ImGui::PushTextWrapPos();
  ImGui::TextDisabled("%s", text);
  ImGui::PopTextWrapPos();
}

void disabledWrappedTextWithInlineStatus(const char* text, const std::string& status)
{
  ImGui::PushTextWrapPos();
  ImGui::TextDisabled("%s", text);
  ImGui::SameLine();
  if (!status.empty()) {
    ImGui::TextDisabled("%s", status.c_str());
  }
  else {
    ImGui::Dummy(ImVec2(ImGui::CalcTextSize("Computing Laplacian magnitude projection...").x, 0.0f));
  }
  ImGui::PopTextWrapPos();
}

bool autoWindowButton(
  const char* label,
  Image& image,
  ImageSettings& settings,
  uint32_t component,
  double low,
  double high)
{
  if (!ImGui::SmallButton(label)) {
    return false;
  }

  settings.setWindowValueLow(component, image.quantileToValue(component, low));
  settings.setWindowValueHigh(component, image.quantileToValue(component, high));
  return true;
}

bool renderAutoWindowButtons(Image& image, ImageSettings& settings, uint32_t component)
{
  ImGui::PushID(&image);
  ImGui::AlignTextToFramePadding();
  ImGui::TextDisabled("Auto window:");
  ImGui::SameLine();

  bool changed = autoWindowButton("0-100%", image, settings, component, 0.0, 1.0);
  ImGui::SameLine();
  changed |= autoWindowButton("1-99%", image, settings, component, 0.01, 0.99);
  ImGui::SameLine();
  changed |= autoWindowButton("2-98%", image, settings, component, 0.02, 0.98);
  ImGui::SameLine();
  helpMarker("Set window/level from image percentiles for the displayed component");

  ImGui::PopID();
  return changed;
}

bool visibilityCheckboxBeforeSlider(const char* id, bool* visible, const char* tooltip)
{
  const float originalSliderWidth = ImGui::CalcItemWidth();
  const bool changed = ImGui::Checkbox(id, visible);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
  const float checkboxSlotWidth = ImGui::GetItemRectSize().x + ImGui::GetStyle().ItemSpacing.x;
  ImGui::SameLine();
  ImGui::PushItemWidth(std::max(1.0f, originalSliderWidth - checkboxSlotWidth));
  return changed;
}

const char* componentRenderModeDescription(
  ComponentRenderMode mode,
  bool vectorFieldImage,
  bool complexValuedImage,
  bool hasAlphaComponent,
  bool logJacobian = false)
{
  switch (mode) {
    case ComponentRenderMode::SingleComponent:
      return vectorFieldImage ? "Selected vector component as a scalar image."
                              : "Selected component as a scalar image.";
    case ComponentRenderMode::Color:
      if (vectorFieldImage) {
        return "Vector components as RGB intensity channels after window/level adjustment.";
      }
      return hasAlphaComponent ? "First four components as RGBA color channels."
                               : "First three components as RGB color channels.";
    case ComponentRenderMode::Minimum:
      return "Minimum value across all components at each voxel.";
    case ComponentRenderMode::Mean:
      return "Mean value across all components at each voxel.";
    case ComponentRenderMode::Maximum:
      return "Maximum value across all components at each voxel.";
    case ComponentRenderMode::Magnitude:
      if (complexValuedImage) {
        return "Complex magnitude.";
      }
      return vectorFieldImage ? "Vector magnitude as a scalar image." : "Component vector length.";
    case ComponentRenderMode::ComplexPhase:
      return "Complex phase.";
    case ComponentRenderMode::ComplexReal:
      return "Real component as a scalar image.";
    case ComponentRenderMode::ComplexImaginary:
      return "Imaginary component as a scalar image.";
    case ComponentRenderMode::VectorDirectionColor:
      return "Direction color in subject/LPS coordinates: red = x, green = y, blue = z; signs ignored.";
    case ComponentRenderMode::VectorSignedNormalProjection:
      return "View-normal projection color: red toward viewer, blue into screen.";
    case ComponentRenderMode::VectorPlanarProjectionColor:
      return "View-plane projection color: red = view right, green = view up.";
    case ComponentRenderMode::VectorJacobianDeterminant:
      return logJacobian ? "Deformation volume change: values > 0 expand, values < 0 contract."
                         : "Deformation volume change: values > 1 expand, values < 1 contract.";
    case ComponentRenderMode::VectorGradientMagnitude:
      return "Magnitude of local spatial changes in the vector field.";
    case ComponentRenderMode::VectorDivergence:
      return "Local net outward or inward flow of the vector field.";
    case ComponentRenderMode::VectorCurlMagnitude:
      return "Local rotational strength of the vector field.";
    case ComponentRenderMode::VectorLaplacianMagnitude:
      return "Magnitude of the component-wise Laplacian.";
  }

  return "";
}

std::string dicomCacheKey(const uuids::uuid& imageUid, const serialize::DicomSource& source)
{
  return uuids::to_string(imageUid) + "|" + source.m_studyInstanceUid + "|" + source.m_seriesInstanceUid;
}

struct HeaderDicomMetadataCache
{
  serialize::DicomSource source;
  std::string displayName;
  std::vector<dicom::MetadataEntry> entries;
  std::size_t numSlices = 0;
  std::string error;
};

HeaderDicomMetadataCache loadHeaderDicomMetadata(const serialize::DicomSource& source)
{
  HeaderDicomMetadataCache cache;
  cache.source = source;
  cache.numSlices = source.m_files.size();

  std::vector<fs::path> inputs;
  if (!source.m_rootPath.empty()) {
    inputs.push_back(source.m_rootPath);
  }
  else {
    inputs.insert(inputs.end(), source.m_files.begin(), source.m_files.end());
  }

  if (inputs.empty()) {
    cache.error = "No DICOM source paths are available.";
    return cache;
  }

  const dicom::DiscoverResult result = dicom::discoverSeries(inputs);
  const auto seriesIt =
    std::find_if(result.series.begin(), result.series.end(), [&source](const dicom::SeriesInfo& series) {
      if (!source.m_seriesInstanceUid.empty() && series.seriesInstanceUid != source.m_seriesInstanceUid) {
        return false;
      }
      if (!source.m_studyInstanceUid.empty() && series.metadata.studyInstanceUid != source.m_studyInstanceUid) {
        return false;
      }
      return true;
    });

  if (seriesIt == result.series.end()) {
    cache.error = "The DICOM series metadata could not be rediscovered from the saved source paths.";
    return cache;
  }

  cache.displayName = seriesIt->displayName;
  cache.entries = seriesIt->metadataSummary;
  cache.numSlices = seriesIt->files.size();
  return cache;
}

void renderImageDicomMetadata(const AppData& appData, const uuids::uuid& imageUid)
{
  const serialize::DicomSource* dicomSource = image_export::dicomSourceForImage(appData, imageUid);
  if (!dicomSource) {
    return;
  }

  if (!ImGui::TreeNode("DICOM Metadata")) {
    return;
  }

  static std::unordered_map<std::string, HeaderDicomMetadataCache> metadataCache;
  const std::string cacheKey = dicomCacheKey(imageUid, *dicomSource);
  auto cacheIt = metadataCache.find(cacheKey);
  if (cacheIt == metadataCache.end()) {
    cacheIt = metadataCache.emplace(cacheKey, loadHeaderDicomMetadata(*dicomSource)).first;
  }

  const HeaderDicomMetadataCache& metadata = cacheIt->second;
  if (!metadata.displayName.empty()) {
    ImGui::TextWrapped("%s", metadata.displayName.c_str());
  }
  if (!metadata.error.empty()) {
    ImGui::TextWrapped("%s", metadata.error.c_str());
  }
  else {
    ImGui::Text("Slices: %zu", metadata.numSlices);
    const ImVec2 tableSize(ImGui::GetContentRegionAvail().x, 360.0f);
    renderDicomMetadataTable("ImageHeaderDicomMetadataTable", metadata.entries, tableSize);
  }
  ImGui::TreePop();
}

template<typename T>
std::string metadataScalarToString(const T& value)
{
  if constexpr (std::is_same_v<T, bool>) {
    return value ? "true" : "false";
  }
  else if constexpr (std::is_same_v<T, char> || std::is_same_v<T, signed char> || std::is_same_v<T, unsigned char>) {
    return std::to_string(static_cast<int>(value));
  }
  else {
    std::ostringstream out;
    out << value;
    return out.str();
  }
}

template<typename T>
std::string metadataVectorToString(const std::vector<T>& values)
{
  std::ostringstream out;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << metadataScalarToString(values.at(i));
  }
  return out.str();
}

template<typename T>
std::string metadataMatrixToString(const std::vector<std::vector<T>>& rows)
{
  std::ostringstream out;
  for (std::size_t r = 0; r < rows.size(); ++r) {
    if (r > 0) {
      out << "; ";
    }
    out << '[' << metadataVectorToString(rows.at(r)) << ']';
  }
  return out.str();
}

std::string metadataValueToString(const MetaDataMap::mapped_type& value)
{
  return std::visit(
    [](const auto& data) -> std::string {
      using Value = std::decay_t<decltype(data)>;
      if constexpr (
        std::is_same_v<Value, std::vector<char>> || std::is_same_v<Value, std::vector<int>> ||
        std::is_same_v<Value, std::vector<float>> || std::is_same_v<Value, std::vector<double>>)
      {
        return metadataVectorToString(data);
      }
      else if constexpr (
        std::is_same_v<Value, std::vector<std::vector<float>>> ||
        std::is_same_v<Value, std::vector<std::vector<double>>>)
      {
        return metadataMatrixToString(data);
      }
      else {
        return metadataScalarToString(data);
      }
    },
    value);
}

std::string metadataTypeName(const MetaDataMap::mapped_type& value)
{
  return std::visit(
    [](const auto& data) -> std::string {
      using Value = std::decay_t<decltype(data)>;
      if constexpr (std::is_same_v<Value, bool>)
        return "bool";
      else if constexpr (std::is_same_v<Value, unsigned char>)
        return "unsigned char";
      else if constexpr (std::is_same_v<Value, char>)
        return "char";
      else if constexpr (std::is_same_v<Value, signed char>)
        return "signed char";
      else if constexpr (std::is_same_v<Value, unsigned short>)
        return "unsigned short";
      else if constexpr (std::is_same_v<Value, short>)
        return "short";
      else if constexpr (std::is_same_v<Value, unsigned int>)
        return "unsigned int";
      else if constexpr (std::is_same_v<Value, int>)
        return "int";
      else if constexpr (std::is_same_v<Value, unsigned long>)
        return "unsigned long";
      else if constexpr (std::is_same_v<Value, long>)
        return "long";
      else if constexpr (std::is_same_v<Value, unsigned long long>)
        return "unsigned long long";
      else if constexpr (std::is_same_v<Value, long long>)
        return "long long";
      else if constexpr (std::is_same_v<Value, float>)
        return "float";
      else if constexpr (std::is_same_v<Value, double>)
        return "double";
      else if constexpr (std::is_same_v<Value, std::string>)
        return "string";
      else if constexpr (std::is_same_v<Value, std::vector<char>>)
        return "char[]";
      else if constexpr (std::is_same_v<Value, std::vector<int>>)
        return "int[]";
      else if constexpr (std::is_same_v<Value, std::vector<float>>)
        return "float[]";
      else if constexpr (std::is_same_v<Value, std::vector<double>>)
        return "double[]";
      else if constexpr (std::is_same_v<Value, std::vector<std::vector<float>>>)
        return "float[][]";
      else if constexpr (std::is_same_v<Value, std::vector<std::vector<double>>>)
        return "double[][]";
      else
        return "unknown";
    },
    value);
}

std::vector<dicom::MetadataEntry> imageMetadataEntries(const MetaDataMap& metadata)
{
  std::vector<dicom::MetadataEntry> entries;
  entries.reserve(metadata.size());
  for (const auto& [key, value] : metadata) {
    entries.push_back(
      dicom::MetadataEntry{.tag = key, .name = metadataTypeName(value), .value = metadataValueToString(value)});
  }
  return entries;
}

void renderImageFileMetadata(const ImageHeader& header)
{
  const MetaDataMap& metadata = header.metaData();
  if (metadata.empty()) {
    return;
  }

  ImGui::SetNextItemOpen(false, ImGuiCond_Appearing);
  if (ImGui::TreeNode("Metadata")) {
    const std::vector<dicom::MetadataEntry> entries = imageMetadataEntries(metadata);
    const ImVec2 tableSize(
      std::max(420.0f, ImGui::GetContentRegionAvail().x),
      std::min(360.0f, std::max(180.0f, 32.0f + 24.0f * static_cast<float>(entries.size()))));
    renderDicomMetadataTable("ImageHeaderFileMetadataTable", entries, tableSize, "Key", "Type", "Value", 0.0f, 0.0f);
    ImGui::TreePop();
    ImGui::Separator();
  }
}

void refreshImageTextureForActiveTimePoint(AppData& appData, const uuids::uuid& imageUid)
{
  appData.renderData().m_imageTextures.erase(imageUid);
  createImageTextures(appData, std::vector<uuids::uuid>{imageUid});
}

void setImageTimePoint(AppData& appData, const uuids::uuid& imageUid, Image& image, uint32_t timePoint)
{
  const uint32_t clamped = image.timeAxis().clamp(timePoint);
  if (image.settings().activeTimePoint() == clamped) {
    return;
  }
  image.settings().setActiveTimePoint(clamped);
  refreshImageTextureForActiveTimePoint(appData, imageUid);
}

void setTimePointWithOptionalSynchronization(
  AppData& appData,
  const uuids::uuid& imageUid,
  Image& image,
  uint32_t timePoint)
{
  setImageTimePoint(appData, imageUid, image, timePoint);
  if (!appData.settings().synchronizeTimeSeries()) {
    return;
  }

  for (const uuids::uuid& otherUid : appData.imageUidsOrdered()) {
    if (otherUid == imageUid) {
      continue;
    }
    Image* other = appData.image(otherUid);
    if (!other || !other->isTimeSeries()) {
      continue;
    }
    setImageTimePoint(appData, otherUid, *other, timePoint);
  }
}

std::string timePointInputFormat(const Image& image, uint32_t timePoint)
{
  std::ostringstream os;
  os << "%u of " << (image.timeAxis().numTimePoints() - 1u);
  if (const auto value = image.timeAxis().value(timePoint)) {
    os << " (" << std::fixed << std::setprecision(2) << *value << " " << image.timeAxis().units() << ")";
  }
  return os.str();
}

std::string timeSeriesSummaryLabel(const Image& image)
{
  std::ostringstream os;
  os << image.timeAxis().numTimePoints() << " frames";
  if (const auto spacing = image.timeAxis().spacing()) {
    os << ", " << *spacing;
    if (!image.timeAxis().units().empty()) {
      os << ' ' << image.timeAxis().units();
    }
    os << " spacing";
  }
  else {
    os << ", variable spacing";
  }
  return os.str();
}

std::string playbackSpeedLabel(const Image& image)
{
  const double speed = image.settings().timePlaybackSpeed();
  const double framePeriod = image.timeAxis().playbackFramePeriodSeconds(speed);
  const double framesPerSecond = playbackFramesPerSecond(framePeriod);
  const char* frameWord = std::abs(framesPerSecond - 1.0) < 1.0e-6 ? "frame" : "frames";
  std::ostringstream os;
  os << "%.1fx (" << std::fixed << std::setprecision(1) << framesPerSecond << ' ' << frameWord << "/sec)";
  return os.str();
}

void renderTimeSeriesHeader(AppData& appData, const uuids::uuid& imageUid, Image& image)
{
  if (!image.isTimeSeries()) {
    return;
  }

  ImageSettings& settings = image.settings();
  const uint32_t oldTimePoint = image.timeAxis().clamp(settings.activeTimePoint());
  uint32_t newTimePoint = oldTimePoint;

  ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
  if (ImGui::TreeNode("Time Series")) {
    ImGui::TextDisabled("%s", timeSeriesSummaryLabel(image).c_str());

    const uint32_t maxTimePoint = image.timeAxis().numTimePoints() - 1u;
    uint32_t timePointInput = oldTimePoint;
    constexpr uint32_t timePointStep = 1u;
    const std::string timePointFormat = timePointInputFormat(image, oldTimePoint);
    if (ImGui::InputScalar(
          "Frame",
          ImGuiDataType_U32,
          &timePointInput,
          &timePointStep,
          nullptr,
          timePointFormat.c_str()))
    {
      newTimePoint = image.timeAxis().clamp(timePointInput);
    }
    ImGui::SameLine();
    helpMarker("Select the displayed time frame");

    bool playing = settings.timePlaybackPlaying();
    const std::string playbackButtonLabel =
      std::string{playing ? ICON_FK_PAUSE : ICON_FK_PLAY} + "##toggleTimePlayback";
    if (ImGui::Button(playbackButtonLabel.c_str())) {
      playing = !playing;
      settings.setTimePlaybackPlaying(playing);
      if (playing) {
        appData.state().setAnimating(true);
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(playing ? "Pause time-series playback" : "Play time series");
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_FAST_BACKWARD "##firstTimePoint")) {
      newTimePoint = 0u;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Go to first time frame");
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_STEP_BACKWARD "##previousTimePoint")) {
      newTimePoint = oldTimePoint == 0u ? (settings.timePlaybackLoop() ? maxTimePoint : 0u) : oldTimePoint - 1u;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Go to previous time frame");
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_STEP_FORWARD "##nextTimePoint")) {
      newTimePoint =
        oldTimePoint == maxTimePoint ? (settings.timePlaybackLoop() ? 0u : maxTimePoint) : oldTimePoint + 1u;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Go to next time frame");
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_FAST_FORWARD "##lastTimePoint")) {
      newTimePoint = maxTimePoint;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Go to last time frame");
    }
    ImGui::SameLine();
    bool loop = settings.timePlaybackLoop();
    if (ImGui::Checkbox("Loop", &loop)) {
      settings.setTimePlaybackLoop(loop);
    }

    const double minPlaybackSpeed = minTimePlaybackSpeed(image);
    const double maxPlaybackSpeed = maxTimePlaybackSpeed(image);
    double playbackSpeed = std::clamp(settings.timePlaybackSpeed(), minPlaybackSpeed, maxPlaybackSpeed);
    if (playbackSpeed != settings.timePlaybackSpeed()) {
      settings.setTimePlaybackSpeed(playbackSpeed);
    }
    const std::string playbackSpeedFormat = playbackSpeedLabel(image);
    if (mySliderF64("Playback speed", &playbackSpeed, minPlaybackSpeed, maxPlaybackSpeed, playbackSpeedFormat.c_str()))
    {
      settings.setTimePlaybackSpeed(playbackSpeed);
    }
    ImGui::SameLine();
    helpMarker(
      "Playback speed multiplier. The slider range is adjusted from the image time spacing to include 1 to 120 "
      "frames/sec playback.");

    ImGui::TreePop();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
  }

  if (newTimePoint != oldTimePoint) {
    setTimePointWithOptionalSynchronization(appData, imageUid, image, newTimePoint);
  }
}

} // namespace

void renderImageHeader(
  AppData& appData,
  GuiData& guiData,
  const uuids::uuid& imageUid,
  size_t imageIndex,
  Image* image,
  bool isActiveImage,
  size_t numImages,
  const std::function<void(void)>& updateAllImageUniforms,
  const std::function<void(void)>& updateImageUniforms,
  const std::function<void(void)>& updateImageInterpolationMode,
  const std::function<void(std::size_t cmapIndex)>& /*updateImageColorMapInterpolationMode*/,
  const std::function<size_t(void)>& getNumImageColorMaps,
  const std::function<ImageColorMap*(size_t cmapIndex)>& getImageColorMap,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageBackward,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageForward,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageToBack,
  const std::function<bool(const uuids::uuid& imageUid)>& moveImageToFront,
  const std::function<bool(const uuids::uuid& imageUid, bool locked)>& setLockManualImageTransformation,
  const std::function<void(const uuids::uuid& imageUid, ComponentProjectionMode mode)>& requestComponentProjectionImage,
  const std::function<void(const uuids::uuid& imageUid)>& requestSetReferenceImage,
  const std::function<void(const uuids::uuid& imageUid)>& requestRemoveImage,
  const AllViewsRecenterType& recenterAllViews)
{
  const ImGuiColorEditFlags colorNoAlphaEditFlags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar |
                                                    ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHex |
                                                    ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_InputRGB;

  const ImGuiColorEditFlags colorAlphaEditFlags = ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_DisplayRGB |
                                                  ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_AlphaBar |
                                                  ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_Uint8 |
                                                  ImGuiColorEditFlags_InputRGB;
  ;

  const auto buttonSize = scaledToolbarButtonSize(appData.windowData().getContentScaleRatios());

  const ImVec4* colors = ImGui::GetStyle().Colors;
  const ImVec4 activeColor = colors[ImGuiCol_ButtonActive];
  ImVec4 inactiveColor = colors[ImGuiCol_Button];

  const std::string minValuesFormatString = std::string("Min: ") + appData.guiData().m_imageValuePrecisionFormat;
  const std::string maxValuesFormatString = std::string("Max: ") + appData.guiData().m_imageValuePrecisionFormat;

  const char* minValuesFormat = minValuesFormatString.c_str();
  const char* maxValuesFormat = maxValuesFormatString.c_str();

  const std::string minPercentileFormatString =
    std::string("Min: ") + appData.guiData().m_percentilePrecisionFormat + "%%";
  const std::string maxPercentileFormatString =
    std::string("Max: ") + appData.guiData().m_percentilePrecisionFormat + "%%";

  const char* minPercentilesFormat = minPercentileFormatString.c_str();
  const char* maxPercentilesFormat = maxPercentileFormatString.c_str();
  const float windowPercentileStep = std::pow(10.0f, -1.0f * appData.guiData().m_percentilePrecision);

  const char* valuesFormat = appData.guiData().m_imageValuePrecisionFormat.c_str();
  const char* txFormat = appData.guiData().m_txPrecisionFormat.c_str();

  /// @todo ADD visibility control for gamma
  if (!image) {
    return;
  }

  const auto& imgHeader = image->header();
  auto& imgSettings = image->settings();
  auto& imgTx = image->transformations();

  auto activeSegUid = appData.imageToActiveSegUid(imageUid);
  Image* activeSeg = (activeSegUid) ? appData.seg(*activeSegUid) : nullptr;

  uint32_t activeComp = imgSettings.activeComponent();
  const uint32_t activeTimePoint = image->timeAxis().clamp(imgSettings.activeTimePoint());
  Image* displayImage = image;
  ImageSettings* displaySettings = &imgSettings;
  uint32_t displayComp = activeComp;

  if (const auto projectionMode = componentProjectionForImage(*image)) {
    if (const auto projectionUid = appData.componentProjectionImageUid(imageUid, *projectionMode, activeTimePoint)) {
      if (Image* projectionImage = appData.image(*projectionUid)) {
        displayImage = projectionImage;
        displaySettings = &projectionImage->settings();
        displayComp = 0;
      }
    }
  }

  auto getCurrentImageColormapIndex = [&displaySettings]() {
    return displaySettings->colorMapIndex();
  };

  auto setCurrentImageColormapIndex = [&displaySettings](size_t cmapIndex) {
    displaySettings->setColorMapIndex(cmapIndex);
  };

  ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_CollapsingHeader;

  if (isActiveImage) {
    headerFlags |= ImGuiTreeNodeFlags_DefaultOpen;
  }

  ImGui::PushID(uuids::to_string(imageUid).c_str());

  // Header is ID'ed only by the image index.
  // ### allows the header name to change without changing its ID.

  /// @todo Provide a function shortenedDisplayName that takes an argument indicating
  /// the max number N of characters. It removes the last characters of the name, such that the
  /// total length is N.
  const bool isRef = appData.refImageUid() && *appData.refImageUid() == imageUid;
  const std::string headerName =
    std::to_string(imageIndex) + ") " +
    imageDisplayNameWithRole(imgSettings.displayName(), isRef, isActiveImage, appData.numImages()) + "###" +
    std::to_string(imageIndex);

  const auto headerColors = computeHeaderBgAndTextColors(imgSettings.borderColor());
  ImGui::PushStyleColor(ImGuiCol_Header, headerColors.first);
  ImGui::PushStyleColor(ImGuiCol_Text, headerColors.second);

  const bool clicked = ImGui::CollapsingHeader(headerName.c_str(), headerFlags);

  ImGui::PopStyleColor(2); // ImGuiCol_Header, ImGuiCol_Text

  if (!clicked) {
    ImGui::PopID(); // imageUid
    return;
  }

  ImGui::Spacing();

  // Border color:
  glm::vec3 borderColor{imgSettings.borderColor()};

  if (ImGui::ColorEdit3("##BorderColor", glm::value_ptr(borderColor), colorNoAlphaEditFlags)) {
    imgSettings.setBorderColor(borderColor);
    imgSettings.setEdgeColor(borderColor); // Set edge color to border color
    updateImageUniforms();
  }
  //    ImGui::SameLine(); helpMarker("Image border color");

  // Display name text:
  std::string displayName = imgSettings.displayName();
  ImGui::SameLine();

  if (ImGui::InputText("Name", &displayName)) {
    imgSettings.setDisplayName(displayName);
  }
  ImGui::SameLine();
  helpMarker("Set the image display name and border color");

  if (!isActiveImage) {
    if (ImGui::Button(ICON_FK_TOGGLE_OFF)) {
      if (appData.setActiveImageUid(imageUid)) {
        ImGui::PopID(); // imageUid
        return;
      }
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Make this the active image");
    }
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
    ImGui::Button(ICON_FK_TOGGLE_ON);
    ImGui::PopStyleColor(1); // ImGuiCol_Button

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("This is the active image");
    }
  }

  ImGui::SameLine();

  if (isRef && isActiveImage) {
    ImGui::Text("%s", referenceAndActiveImageMessage);
  }
  else if (isRef) {
    ImGui::Text("%s", referenceImageMessage);
  }
  else if (isActiveImage) {
    ImGui::Text("%s", activeImageMessage);
  }
  else {
    ImGui::Text("%s", nonActiveImageMessage);
  }

  const bool forceLocked = isRef;
  const bool isLocked = (forceLocked || image->transformations().is_worldDef_T_affine_locked());

  ImGui::PushStyleColor(ImGuiCol_Button, (isLocked ? inactiveColor : activeColor));
  if (ImGui::Button((isLocked ? ICON_FK_LOCK : ICON_FK_UNLOCK), buttonSize)) {
    if (!forceLocked) {
      setLockManualImageTransformation(imageUid, !isLocked);
    }
  }
  ImGui::PopStyleColor(1); // ImGuiCol_Button

  if (ImGui::IsItemHovered()) {
    if (forceLocked) {
      ImGui::SetTooltip("Manual image transformation is always locked for the reference image.");
    }
    else if (isLocked) {
      ImGui::SetTooltip("Manual image transformation is locked.\nClick to unlock and allow movement.");
    }
    else {
      ImGui::SetTooltip("Manual image transformation is unlocked.\nClick to lock and prevent movement.");
    }
  }

  ImGui::SameLine();

  if (ImGui::Button(ICON_FK_HAND_O_UP, buttonSize)) {
    glm::vec3 worldPos{imgTx.worldDef_T_subject() * glm::vec4{imgHeader.subjectBBoxCenter(), 1.0f}};

    worldPos = data::snapWorldPointToImageVoxels(appData, worldPos);
    appData.state().setWorldCrosshairsPos(worldPos);
  }

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Move crosshairs to the center of the image");
  }

  ImGui::SameLine();

  if (isRef) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button(ICON_FK_BULLSEYE, buttonSize)) {
    requestSetReferenceImage(imageUid);
  }
  if (isRef) {
    ImGui::EndDisabled();
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip(
      isRef ? "This image is the project reference image" : "Make this image the project reference image");
  }

  ImGui::SameLine();

  if (isRef) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button(ICON_FK_TIMES, buttonSize)) {
    requestRemoveImage(imageUid);
  }
  if (isRef) {
    ImGui::EndDisabled();
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip(isRef ? "The reference image cannot be removed" : "Remove this image from the project");
  }

  if (0 < imageIndex) {
    // Rules for showing the buttons that change image order:
    const bool showDecreaseIndex = true | (1 < imageIndex);
    const bool showIncreaseIndex = true | (imageIndex < numImages - 1);

    if (showDecreaseIndex) {
      if (ImGui::Button(ICON_FK_FAST_BACKWARD)) {
        moveImageToBack(imageUid);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Move image to backmost layer");
      }

      ImGui::SameLine();
      if (ImGui::Button(ICON_FK_BACKWARD)) {
        moveImageBackward(imageUid);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Move image backward in layers (decrease the image order)");
      }
    }

    if (showIncreaseIndex) {
      ImGui::SameLine();
      if (ImGui::Button(ICON_FK_FORWARD)) {
        moveImageForward(imageUid);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Move image forward in layers (increase the image order)");
      }

      ImGui::SameLine();
      if (ImGui::Button(ICON_FK_FAST_FORWARD)) {
        moveImageToFront(imageUid);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Move image to frontmost layer");
      }
    }
  }

  if (image_export::imageHasDicomSource(appData, imageUid)) {
    const bool canExportImage = image->hasPixelData();
    if (!canExportImage) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("Export DICOM Series as Image...")) {
      image_export::exportDicomImage(appData, imageUid);
    }
    if (!canExportImage) {
      ImGui::EndDisabled();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("Export this DICOM series as a 3D medical image");
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  const bool showComponentControls =
    (imgHeader.numComponentsPerPixel() > 1 && Image::MultiComponentBufferType::SeparateImages == image->bufferType());

  renderTimeSeriesHeader(appData, imageUid, *image);

  if (showComponentControls) {
    ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
    const bool complexValuedImage = isComplexValuedImage(*image);
    const bool vectorFieldImage = isVectorFieldImage(*image);
    if (ImGui::TreeNode(
          complexValuedImage ? "Complex Image Settings"
                             : (vectorFieldImage ? "Vector Field Rendering" : "Multi-Component Rendering")))
    {
      auto setComponentMode = [&](ComponentRenderMode mode) {
        if (imgSettings.componentRenderMode() == mode) {
          return;
        }

        imgSettings.setComponentRenderMode(mode);
        if (ComponentRenderMode::ComplexReal == mode) {
          imgSettings.setActiveComponent(0);
          activeComp = imgSettings.activeComponent();
        }
        else if (ComponentRenderMode::ComplexImaginary == mode) {
          imgSettings.setActiveComponent(1);
          activeComp = imgSettings.activeComponent();
        }

        if (const auto projectionMode = componentProjectionForImage(*image)) {
          requestComponentProjectionImage(imageUid, *projectionMode);
        }
        updateImageUniforms();
        updateImageInterpolationMode();
      };

      if (vectorFieldImage) {
        struct VectorModeOption
        {
          const char* label;
          ComponentRenderMode mode;
        };
        constexpr std::array vectorModeOptions{
          VectorModeOption{"Component", ComponentRenderMode::SingleComponent},
          VectorModeOption{"Magnitude", ComponentRenderMode::Magnitude},
          VectorModeOption{"Minimum", ComponentRenderMode::Minimum},
          VectorModeOption{"Maximum", ComponentRenderMode::Maximum},
          VectorModeOption{"Mean", ComponentRenderMode::Mean},
          VectorModeOption{"RGB color", ComponentRenderMode::Color},
          VectorModeOption{"Direction color", ComponentRenderMode::VectorDirectionColor},
          VectorModeOption{"Planar projection color", ComponentRenderMode::VectorPlanarProjectionColor},
          VectorModeOption{"Normal projection color", ComponentRenderMode::VectorSignedNormalProjection},
          VectorModeOption{"Deformation Jacobian determinant", ComponentRenderMode::VectorJacobianDeterminant},
          VectorModeOption{"Gradient magnitude", ComponentRenderMode::VectorGradientMagnitude},
          VectorModeOption{"Divergence", ComponentRenderMode::VectorDivergence},
          VectorModeOption{"Curl magnitude", ComponentRenderMode::VectorCurlMagnitude},
          VectorModeOption{"Laplacian magnitude", ComponentRenderMode::VectorLaplacianMagnitude}};

        const char* currentVectorModeLabel = vectorModeOptions[0].label;
        for (const auto& option : vectorModeOptions) {
          if (option.mode == imgSettings.componentRenderMode()) {
            currentVectorModeLabel = option.label;
            break;
          }
        }

        if (ImGui::BeginCombo("Render", currentVectorModeLabel)) {
          for (const auto& option : vectorModeOptions) {
            const bool selected = option.mode == imgSettings.componentRenderMode();
            if (ImGui::Selectable(option.label, selected)) {
              setComponentMode(option.mode);
            }
            if (selected) {
              ImGui::SetItemDefaultFocus();
            }
          }
          ImGui::EndCombo();
        }

        const auto pendingProjectionMode = componentProjectionForImage(*image);
        const bool projectionPending =
          pendingProjectionMode &&
          !appData.componentProjectionImageUid(imageUid, *pendingProjectionMode, activeTimePoint);
        const std::string projectionStatus =
          projectionPending
            ? fmt::format("Computing {} projection...", componentProjectionModeName(*pendingProjectionMode))
            : std::string{};
        disabledWrappedTextWithInlineStatus(
          componentRenderModeDescription(
            imgSettings.componentRenderMode(),
            true,
            false,
            false,
            imgSettings.vectorLogJacobianDeterminant()),
          projectionStatus);
        if (ComponentRenderMode::VectorPlanarProjectionColor == imgSettings.componentRenderMode()) {
          bool signedColors = imgSettings.vectorPlanarProjectionSignedColors();
          if (ImGui::Checkbox("Signed colors", &signedColors)) {
            imgSettings.setVectorPlanarProjectionSignedColors(signedColors);
            updateImageUniforms();
          }
          ImGui::SameLine();
          helpMarker("Use distinct colors for right/left and up/down in-plane vector directions");
        }
        if (ComponentRenderMode::VectorJacobianDeterminant == imgSettings.componentRenderMode()) {
          bool logJacobian = imgSettings.vectorLogJacobianDeterminant();
          if (ImGui::Checkbox("Log-Jacobian", &logJacobian)) {
            imgSettings.setVectorLogJacobianDeterminant(logJacobian);
            if (const auto projectionMode = componentProjectionForImage(*image)) {
              requestComponentProjectionImage(imageUid, *projectionMode);
            }
            updateImageUniforms();
          }
          ImGui::SameLine();
          helpMarker("Show log(det(F)) instead of det(F), where F is the deformation gradient.");
        }
      }
      else if (complexValuedImage) {
        struct ComplexModeOption
        {
          const char* label;
          ComponentRenderMode mode;
        };
        constexpr std::array complexModeOptions{
          ComplexModeOption{"Magnitude", ComponentRenderMode::Magnitude},
          ComplexModeOption{"Phase", ComponentRenderMode::ComplexPhase},
          ComplexModeOption{"Real", ComponentRenderMode::ComplexReal},
          ComplexModeOption{"Imaginary", ComponentRenderMode::ComplexImaginary}};

        const char* currentComplexModeLabel = complexModeOptions[0].label;
        for (const auto& option : complexModeOptions) {
          if (option.mode == imgSettings.componentRenderMode()) {
            currentComplexModeLabel = option.label;
            break;
          }
        }

        if (ImGui::BeginCombo("Render", currentComplexModeLabel)) {
          for (const auto& option : complexModeOptions) {
            const bool selected = option.mode == imgSettings.componentRenderMode();
            if (ImGui::Selectable(option.label, selected)) {
              setComponentMode(option.mode);
            }
            if (selected) {
              ImGui::SetItemDefaultFocus();
            }
          }
          ImGui::EndCombo();
        }

        disabledWrappedText(componentRenderModeDescription(imgSettings.componentRenderMode(), false, true, false));

        if (ComponentRenderMode::ComplexPhase == imgSettings.componentRenderMode()) {
          ImGui::AlignTextToFramePadding();
          ImGui::TextUnformatted("Phase units:");
          ImGui::SameLine();
          bool radians = ComplexPhaseUnit::Radians == imgSettings.complexPhaseUnit();
          if (ImGui::RadioButton("Radians", radians)) {
            imgSettings.setComplexPhaseUnit(ComplexPhaseUnit::Radians);
            if (const auto projectionMode = componentProjectionForImage(*image)) {
              requestComponentProjectionImage(imageUid, *projectionMode);
            }
            updateImageUniforms();
          }
          ImGui::SameLine();
          bool degrees = ComplexPhaseUnit::Degrees == imgSettings.complexPhaseUnit();
          if (ImGui::RadioButton("Degrees", degrees)) {
            imgSettings.setComplexPhaseUnit(ComplexPhaseUnit::Degrees);
            if (const auto projectionMode = componentProjectionForImage(*image)) {
              requestComponentProjectionImage(imageUid, *projectionMode);
            }
            updateImageUniforms();
          }

          ImGui::AlignTextToFramePadding();
          ImGui::TextUnformatted("Phase range:");
          ImGui::SameLine();
          const bool signedRange = ComplexPhaseRange::Signed == imgSettings.complexPhaseRange();
          const char* signedLabel =
            ComplexPhaseUnit::Degrees == imgSettings.complexPhaseUnit() ? "-180 to 180 deg" : "-pi to pi";
          if (ImGui::RadioButton(signedLabel, signedRange)) {
            imgSettings.setComplexPhaseRange(ComplexPhaseRange::Signed);
            if (const auto projectionMode = componentProjectionForImage(*image)) {
              requestComponentProjectionImage(imageUid, *projectionMode);
            }
            updateImageUniforms();
          }
          ImGui::SameLine();
          const char* unsignedLabel =
            ComplexPhaseUnit::Degrees == imgSettings.complexPhaseUnit() ? "0 to 360 deg" : "0 to 2pi";
          if (ImGui::RadioButton(unsignedLabel, !signedRange)) {
            imgSettings.setComplexPhaseRange(ComplexPhaseRange::Unsigned);
            if (const auto projectionMode = componentProjectionForImage(*image)) {
              requestComponentProjectionImage(imageUid, *projectionMode);
            }
            updateImageUniforms();
          }
        }
      }
      else {
        struct MultiComponentModeOption
        {
          const char* label;
          ComponentRenderMode mode;
        };
        std::vector<MultiComponentModeOption> componentModeOptions{
          MultiComponentModeOption{"Single component", ComponentRenderMode::SingleComponent}};
        const bool canDisplayAsColor =
          (3 == imgHeader.numComponentsPerPixel() || 4 == imgHeader.numComponentsPerPixel());
        if (canDisplayAsColor) {
          const bool hasAlphaComponent = 4 == imgHeader.numComponentsPerPixel();
          componentModeOptions.push_back(
            MultiComponentModeOption{hasAlphaComponent ? "RGBA color" : "RGB color", ComponentRenderMode::Color});
        }
        componentModeOptions.push_back(MultiComponentModeOption{"Minimum", ComponentRenderMode::Minimum});
        componentModeOptions.push_back(MultiComponentModeOption{"Maximum", ComponentRenderMode::Maximum});
        componentModeOptions.push_back(MultiComponentModeOption{"Mean", ComponentRenderMode::Mean});
        componentModeOptions.push_back(MultiComponentModeOption{"Magnitude", ComponentRenderMode::Magnitude});

        const char* currentComponentModeLabel = componentModeOptions.front().label;
        for (const auto& option : componentModeOptions) {
          if (option.mode == imgSettings.componentRenderMode()) {
            currentComponentModeLabel = option.label;
            break;
          }
        }

        if (ImGui::BeginCombo("Render", currentComponentModeLabel)) {
          for (const auto& option : componentModeOptions) {
            const bool selected = option.mode == imgSettings.componentRenderMode();
            if (ImGui::Selectable(option.label, selected)) {
              setComponentMode(option.mode);
            }
            if (selected) {
              ImGui::SetItemDefaultFocus();
            }
          }
          ImGui::EndCombo();
        }

        disabledWrappedText(componentRenderModeDescription(
          imgSettings.componentRenderMode(),
          false,
          false,
          4 == imgHeader.numComponentsPerPixel()));
      }

      if (
        !vectorFieldImage && !complexValuedImage && imgSettings.displayImageAsColor() &&
        4 == imgHeader.numComponentsPerPixel())
      {
        bool ignoreAlpha = imgSettings.ignoreAlpha();

        if (ImGui::Checkbox("Ignore alpha component", &ignoreAlpha)) {
          imgSettings.setIgnoreAlpha(ignoreAlpha);
          updateImageUniforms();
        }
        ImGui::SameLine();
        helpMarker("Ignore alpha component of RGBA image");
      }

      const bool showPerComponentControls =
        ComponentRenderMode::SingleComponent == imgSettings.componentRenderMode() || imgSettings.displayImageAsColor();
      if (showPerComponentControls) {
        const uint32_t componentMax = imgHeader.numComponentsPerPixel() - 1;
        if (vectorFieldImage) {
          uint32_t componentInput = activeComp;
          constexpr uint32_t componentStep = 1;
          const std::string currentComponentLabel = vectorFieldComponentLabel(activeComp, componentMax);
          const std::string componentPrefix = std::to_string(activeComp);
          std::string componentFormat = "%u";
          if (currentComponentLabel.starts_with(componentPrefix)) {
            componentFormat += currentComponentLabel.substr(componentPrefix.size());
          }
          if (ImGui::InputScalar(
                "Component",
                ImGuiDataType_U32,
                &componentInput,
                &componentStep,
                nullptr,
                componentFormat.c_str()))
          {
            const uint32_t clampedComponent = std::min(componentInput, componentMax);
            if (clampedComponent != activeComp) {
              imgSettings.setActiveComponent(clampedComponent);
              activeComp = imgSettings.activeComponent();
            }
            updateImageUniforms();
          }
        }
        else if (complexValuedImage) {
          const std::string currentComponentLabel = complexComponentLabel(activeComp, componentMax);
          if (ImGui::BeginCombo("Component", currentComponentLabel.c_str())) {
            for (uint32_t component = 0; component <= componentMax; ++component) {
              const std::string label = complexComponentLabel(component, componentMax);
              const bool selected = component == activeComp;
              if (ImGui::Selectable(label.c_str(), selected)) {
                imgSettings.setActiveComponent(component);
                activeComp = imgSettings.activeComponent();
                updateImageUniforms();
              }
              if (selected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }
        }
        else {
          uint32_t componentInput = activeComp;
          constexpr uint32_t componentStep = 1;
          const std::string componentFormat = "%u of " + std::to_string(componentMax);
          if (ImGui::InputScalar(
                "Component",
                ImGuiDataType_U32,
                &componentInput,
                &componentStep,
                nullptr,
                componentFormat.c_str()))
          {
            const uint32_t clampedComponent = std::min(componentInput, componentMax);
            if (clampedComponent != activeComp) {
              imgSettings.setActiveComponent(clampedComponent);
              activeComp = imgSettings.activeComponent();
              updateImageUniforms();
            }
          }
        }

        ImGui::SameLine();
        helpMarker("Select the image component to display and adjust");

        if (imgSettings.displayImageAsColor()) {
          bool visible = imgSettings.visibility();
          if (visibilityCheckboxBeforeSlider(
                "##componentVisible",
                &visible,
                "Show/hide the selected image component on all views"))
          {
            imgSettings.setVisibility(visible);
            updateImageUniforms();
          }
          double imageOpacity = imgSettings.opacity();
          ImGui::BeginDisabled(!visible);
          if (mySliderF64("Component opacity", &imageOpacity, 0.0, 1.0)) {
            imgSettings.setOpacity(imageOpacity);
            updateImageUniforms();
          }
          ImGui::EndDisabled();
          ImGui::PopItemWidth();
          ImGui::SameLine();
          helpMarker("Selected image component opacity");
        }
      }
      else if (!vectorFieldImage) {
        if (const auto projectionMode = componentProjectionForImage(*image);
            projectionMode && !appData.componentProjectionImageUid(imageUid, *projectionMode, activeTimePoint))
        {
          ImGui::TextDisabled("Computing %s projection...", componentProjectionModeName(*projectionMode).c_str());
        }
      }

      if (vectorFieldImage) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        auto setVectorOverlayOnImage = [&](bool overlayOnImage) {
          imgSettings.setVectorArrowOverlayOnImage(overlayOnImage);
          imgSettings.setVectorWarpedGridOverlayOnImage(overlayOnImage);
        };

        bool vectorOverlayOnImage = imgSettings.vectorArrowOverlayVisible()
                                      ? imgSettings.vectorArrowOverlayOnImage()
                                      : imgSettings.vectorWarpedGridOverlayOnImage();
        if (
          imgSettings.vectorArrowOverlayOnImage() != vectorOverlayOnImage ||
          imgSettings.vectorWarpedGridOverlayOnImage() != vectorOverlayOnImage)
        {
          setVectorOverlayOnImage(vectorOverlayOnImage);
        }

        bool showArrowOverlay = imgSettings.vectorArrowOverlayVisible();
        if (ImGui::Checkbox("Vector field arrows", &showArrowOverlay)) {
          imgSettings.setVectorArrowOverlayVisible(showArrowOverlay);
        }
        ImGui::SameLine();
        helpMarker("Draw sampled arrows over the current slice to show the vector field direction");

        if (
          imgSettings.vectorArrowOverlayVisible() && ImGui::TreeNodeEx("Arrow options", ImGuiTreeNodeFlags_DefaultOpen))
        {
          bool overlayOnImage = vectorOverlayOnImage;
          if (ImGui::Checkbox("Overlay on image", &overlayOnImage)) {
            vectorOverlayOnImage = overlayOnImage;
            setVectorOverlayOnImage(vectorOverlayOnImage);
          }
          ImGui::SameLine();
          helpMarker("Draw the image normally and place vector arrows on top. When off, only the arrows are drawn.");

          bool useDirectionColor = imgSettings.vectorArrowOverlayUseDirectionColor();
          if (ImGui::Checkbox("Color by direction", &useDirectionColor)) {
            imgSettings.setVectorArrowOverlayUseDirectionColor(useDirectionColor);
          }
          ImGui::SameLine();
          helpMarker("Color arrows by vector direction using the absolute x, y, and z components as RGB.");

          bool scaleByMagnitude = imgSettings.vectorArrowOverlayScaleByMagnitude();
          if (ImGui::Checkbox("Scale arrows by magnitude", &scaleByMagnitude)) {
            imgSettings.setVectorArrowOverlayScaleByMagnitude(scaleByMagnitude);
          }
          ImGui::SameLine();
          helpMarker(
            "Scale each arrow length by the vector magnitude. When off, arrows show direction with uniform length.");

          float scaleFactor = imgSettings.vectorArrowOverlayScaleFactor();
          if (ImGui::SliderFloat("Scale factor", &scaleFactor, 0.01f, 10.0f, "%.2f x", ImGuiSliderFlags_Logarithmic)) {
            imgSettings.setVectorArrowOverlayScaleFactor(scaleFactor);
          }
          ImGui::SameLine();
          helpMarker("Scale factor is dimensionless: 1.0 draws the physical displacement length as stored.");

          const VectorArrowOverlaySpacingMode spacingMode = imgSettings.vectorArrowOverlaySpacingMode();
          if (VectorArrowOverlaySpacingMode::Pixels == spacingMode) {
            float spacing = imgSettings.vectorArrowOverlayDensity();
            if (ImGui::SliderFloat("Spacing", &spacing, 8.0f, 128.0f, "%.0f px")) {
              imgSettings.setVectorArrowOverlayDensity(spacing);
            }
          }
          else if (VectorArrowOverlaySpacingMode::Voxels == spacingMode) {
            float spacing = imgSettings.vectorArrowOverlayVoxelSpacing();
            if (ImGui::SliderFloat("Spacing", &spacing, 0.1f, 10.0f, "%.2f vox", ImGuiSliderFlags_Logarithmic)) {
              imgSettings.setVectorArrowOverlayVoxelSpacing(spacing);
            }
          }
          else {
            float spacing = imgSettings.vectorArrowOverlayMillimeterSpacing();
            if (ImGui::SliderFloat("Spacing", &spacing, 0.1f, 100.0f, "%.2f mm")) {
              imgSettings.setVectorArrowOverlayMillimeterSpacing(spacing);
            }
          }

          VectorArrowOverlaySpacingMode selectedSpacingMode = spacingMode;
          if (ImGui::BeginCombo("Spacing units", vectorArrowOverlaySpacingModeName(selectedSpacingMode))) {
            constexpr std::array spacingModes{
              VectorArrowOverlaySpacingMode::Pixels,
              VectorArrowOverlaySpacingMode::Voxels,
              VectorArrowOverlaySpacingMode::Millimeters};
            for (const VectorArrowOverlaySpacingMode mode : spacingModes) {
              const bool selected = mode == selectedSpacingMode;
              if (ImGui::Selectable(vectorArrowOverlaySpacingModeName(mode), selected)) {
                selectedSpacingMode = mode;
                imgSettings.setVectorArrowOverlaySpacingMode(mode);
              }
              if (selected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }

          float lineThickness = imgSettings.vectorArrowOverlayLineThickness();
          if (ImGui::SliderFloat("Line thickness", &lineThickness, 0.25f, 4.0f, "%.2f px")) {
            imgSettings.setVectorArrowOverlayLineThickness(lineThickness);
          }

          if (!imgSettings.vectorArrowOverlayUseDirectionColor()) {
            glm::vec4 arrowColor{imgSettings.vectorArrowOverlayColor(), imgSettings.vectorArrowOverlayOpacity()};
            if (ImGui::ColorEdit4("Color", glm::value_ptr(arrowColor))) {
              imgSettings.setVectorArrowOverlayColor(glm::vec3{arrowColor});
              imgSettings.setVectorArrowOverlayOpacity(arrowColor.a);
            }
            ImGui::SameLine();
            helpMarker("RGBA color used for vector arrows.");
          }

          ImGui::TreePop();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        bool showWarpedGrid = imgSettings.vectorWarpedGridVisible();
        if (ImGui::Checkbox("Vector field warped grid", &showWarpedGrid)) {
          imgSettings.setVectorWarpedGridVisible(showWarpedGrid);
        }
        ImGui::SameLine();
        helpMarker("Draw a grid warped by the vector field on the current slice.");

        if (imgSettings.vectorWarpedGridVisible() && ImGui::TreeNodeEx("Grid options", ImGuiTreeNodeFlags_DefaultOpen))
        {
          bool overlayOnImage = vectorOverlayOnImage;
          if (ImGui::Checkbox("Overlay on image##warpedGrid", &overlayOnImage)) {
            vectorOverlayOnImage = overlayOnImage;
            setVectorOverlayOnImage(vectorOverlayOnImage);
          }
          ImGui::SameLine();
          helpMarker("Draw the image normally and place the warped grid on top. When off, only the grid is drawn.");

          VectorWarpedGridConvention convention = imgSettings.vectorWarpedGridConvention();
          if (ImGui::BeginCombo("Convention", vectorWarpedGridConventionName(convention))) {
            constexpr std::array conventions{
              VectorWarpedGridConvention::SamplingField,
              VectorWarpedGridConvention::ApparentDeformation};
            for (const VectorWarpedGridConvention option : conventions) {
              const bool selected = option == convention;
              if (ImGui::Selectable(vectorWarpedGridConventionName(option), selected)) {
                convention = option;
                imgSettings.setVectorWarpedGridConvention(option);
              }
              if (selected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }
          ImGui::SameLine();
          helpMarker(
            "Choose whether the stored vectors are shown as a sampling field or as an approximate apparent "
            "deformation.");
          if (VectorWarpedGridConvention::ApparentDeformation == imgSettings.vectorWarpedGridConvention()) {
            ImGui::TextDisabled("Uses an approximate inverse by reversing the displacement.");
          }

          float gridScaleFactor = imgSettings.vectorWarpedGridScaleFactor();
          if (ImGui::SliderFloat("Warp scale", &gridScaleFactor, 0.01f, 10.0f, "%.2f x", ImGuiSliderFlags_Logarithmic))
          {
            imgSettings.setVectorWarpedGridScaleFactor(gridScaleFactor);
          }
          ImGui::SameLine();
          helpMarker("Dimensionless multiplier applied to the displacement before warping the grid.");

          const VectorArrowOverlaySpacingMode gridSpacingMode = imgSettings.vectorWarpedGridSpacingMode();
          if (VectorArrowOverlaySpacingMode::Pixels == gridSpacingMode) {
            float spacing = imgSettings.vectorWarpedGridPixelSpacing();
            if (ImGui::SliderFloat("Spacing", &spacing, 4.0f, 256.0f, "%.0f px")) {
              imgSettings.setVectorWarpedGridPixelSpacing(spacing);
            }
          }
          else if (VectorArrowOverlaySpacingMode::Voxels == gridSpacingMode) {
            float spacing = imgSettings.vectorWarpedGridVoxelSpacing();
            if (ImGui::SliderFloat("Spacing", &spacing, 0.1f, 100.0f, "%.2f vox", ImGuiSliderFlags_Logarithmic)) {
              imgSettings.setVectorWarpedGridVoxelSpacing(spacing);
            }
          }
          else {
            float spacing = imgSettings.vectorWarpedGridMillimeterSpacing();
            if (ImGui::SliderFloat("Spacing", &spacing, 0.1f, 100.0f, "%.2f mm")) {
              imgSettings.setVectorWarpedGridMillimeterSpacing(spacing);
            }
          }
          ImGui::SameLine();
          helpMarker("Distance between neighboring grid lines, measured in the selected spacing units.");

          VectorArrowOverlaySpacingMode selectedGridSpacingMode = gridSpacingMode;
          if (ImGui::BeginCombo("Spacing units", vectorArrowOverlaySpacingModeName(selectedGridSpacingMode))) {
            constexpr std::array spacingModes{
              VectorArrowOverlaySpacingMode::Pixels,
              VectorArrowOverlaySpacingMode::Voxels,
              VectorArrowOverlaySpacingMode::Millimeters};
            for (const VectorArrowOverlaySpacingMode mode : spacingModes) {
              const bool selected = mode == selectedGridSpacingMode;
              if (ImGui::Selectable(vectorArrowOverlaySpacingModeName(mode), selected)) {
                selectedGridSpacingMode = mode;
                imgSettings.setVectorWarpedGridSpacingMode(mode);
              }
              if (selected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }
          ImGui::SameLine();
          helpMarker(
            "Choose whether grid spacing is specified in screen pixels, image voxels, or physical millimeters.");

          float gridLineThickness = imgSettings.vectorWarpedGridLineThickness();
          if (ImGui::SliderFloat("Line thickness", &gridLineThickness, 0.25f, 8.0f, "%.2f px")) {
            imgSettings.setVectorWarpedGridLineThickness(gridLineThickness);
          }
          ImGui::SameLine();
          helpMarker("Line thickness in screen pixels.");

          glm::vec4 gridForeground = imgSettings.vectorWarpedGridForegroundColor();
          if (ImGui::ColorEdit4("Foreground", glm::value_ptr(gridForeground))) {
            imgSettings.setVectorWarpedGridForegroundColor(gridForeground);
          }
          ImGui::SameLine();
          helpMarker("RGBA color used for warped grid lines.");

          glm::vec4 gridBackground = imgSettings.vectorWarpedGridBackgroundColor();
          if (ImGui::ColorEdit4("Background", glm::value_ptr(gridBackground))) {
            imgSettings.setVectorWarpedGridBackgroundColor(gridBackground);
          }
          ImGui::SameLine();
          helpMarker("RGBA color used between grid lines. Set alpha to zero for a transparent background.");

          ImGui::TreePop();
        }
      }

      ImGui::TreePop();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
  }

  // Open View Properties on first appearance
  ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
  if (ImGui::TreeNode("View Properties")) {
    if (showComponentControls) {
      // Global image opacity slider:
      bool globalVisibility = imgSettings.globalVisibility();
      if (visibilityCheckboxBeforeSlider(
            "##globalImageVisible",
            &globalVisibility,
            "Show/hide all image components on all views (W)"))
      {
        imgSettings.setGlobalVisibility(globalVisibility);
        updateImageUniforms();
      }
      double globalImageOpacity = imgSettings.globalOpacity();
      ImGui::BeginDisabled(!globalVisibility);
      if (mySliderF64("Image opacity", &globalImageOpacity, 0.0, 1.0)) {
        imgSettings.setGlobalOpacity(globalImageOpacity);
        updateImageUniforms();
      }
      ImGui::EndDisabled();
      ImGui::PopItemWidth();
      ImGui::SameLine();
      helpMarker("Image layer opacity");

      if (activeSeg) {
        bool segVisible = activeSeg->settings().visibility();
        if (visibilityCheckboxBeforeSlider(
              "##segmentationVisibleGlobal",
              &segVisible,
              "Show/hide the segmentation on all views (S)"))
        {
          activeSeg->settings().setVisibility(segVisible);
          updateAllImageUniforms();
        }
        double segOpacity = activeSeg->settings().opacity();
        ImGui::BeginDisabled(!segVisible);
        if (mySliderF64("Seg. opacity", &segOpacity, 0.0, 1.0)) {
          activeSeg->settings().setOpacity(segOpacity);
          updateAllImageUniforms();
        }
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        ImGui::SameLine();
        helpMarker("Segmentation layer opacity");
      }

      ImGui::Dummy(ImVec2(0.0f, 1.0f));
    }

    static const std::string imageOpacityText("Image opacity");

    if (!showComponentControls) {
      // Image opacity slider:
      bool visible = imgSettings.visibility();
      if (visibilityCheckboxBeforeSlider("##imageVisible", &visible, "Show/hide the image on all views")) {
        imgSettings.setVisibility(visible);
        updateImageUniforms();
      }
      double imageOpacity = imgSettings.opacity();
      ImGui::BeginDisabled(!visible);
      if (mySliderF64(imageOpacityText.c_str(), &imageOpacity, 0.0, 1.0)) {
        imgSettings.setOpacity(imageOpacity);
        updateImageUniforms();
      }
      ImGui::EndDisabled();
      ImGui::PopItemWidth();
      ImGui::SameLine();
      helpMarker("Image layer opacity");

      // Segmentation opacity slider:
      if (activeSeg) {
        bool segVisible = activeSeg->settings().visibility();
        if (visibilityCheckboxBeforeSlider(
              "##segmentationVisible",
              &segVisible,
              "Show/hide the segmentation on all views (S)"))
        {
          activeSeg->settings().setVisibility(segVisible);
          updateAllImageUniforms();
        }
        double segOpacity = activeSeg->settings().opacity();
        ImGui::BeginDisabled(!segVisible);
        if (mySliderF64("Seg. opacity", &segOpacity, 0.0, 1.0)) {
          activeSeg->settings().setOpacity(segOpacity);
          updateAllImageUniforms();
        }
        ImGui::EndDisabled();
        ImGui::PopItemWidth();
        ImGui::SameLine();
        helpMarker("Segmentation layer opacity");
      }

      ImGui::Dummy(ImVec2(0.0f, 1.0f));
    }

    ImageSettings& viewSettings = *displaySettings;
    Image& viewImage = *displayImage;
    const ImageHeader& viewHeader = viewImage.header();
    const uint32_t viewComp = displayComp;

    const bool imageHasFloatComponents =
      (ComponentType::Float32 == viewHeader.memoryComponentType() ||
       ComponentType::Float64 == viewHeader.memoryComponentType());

    if (imageHasFloatComponents) {
      // Threshold range:
      const float threshMin = static_cast<float>(viewSettings.thresholdRange(viewComp).first);
      const float threshMax = static_cast<float>(viewSettings.thresholdRange(viewComp).second);

      // Speed of range slider is based on the range
      const float speed = static_cast<float>(threshMax - threshMin) / 1000.0f;

      // Window/level sliders:
      const float windowWidthMin = static_cast<float>(viewSettings.minMaxWindowWidthRange(viewComp).first);
      const float windowWidthMax = static_cast<float>(viewSettings.minMaxWindowWidthRange(viewComp).second);

      const float windowCenterMin = static_cast<float>(viewSettings.minMaxWindowCenterRange(viewComp).first);
      const float windowCenterMax = static_cast<float>(viewSettings.minMaxWindowCenterRange(viewComp).second);

      const float windowMin = static_cast<float>(viewSettings.minMaxWindowRange(viewComp).first);
      const float windowMax = static_cast<float>(viewSettings.minMaxWindowRange(viewComp).second);

      float windowLow = static_cast<float>(viewSettings.windowValuesLowHigh(viewComp).first);
      float windowHigh = static_cast<float>(viewSettings.windowValuesLowHigh(viewComp).second);

      double windowWidth = viewSettings.windowWidth(viewComp);
      double windowCenter = viewSettings.windowCenter(viewComp);

      ImGui::Text("Windowing:");

      if (mySliderF64("Width", &windowWidth, windowWidthMin, windowWidthMax, valuesFormat)) {
        viewSettings.setWindowWidth(viewComp, windowWidth);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Window width");

      if (mySliderF64("Level", &windowCenter, windowCenterMin, windowCenterMax, valuesFormat)) {
        viewSettings.setWindowCenter(viewComp, windowCenter);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Window level (center)");

      if (ImGui::DragFloatRange2(
            "Values",
            &windowLow,
            &windowHigh,
            speed,
            windowMin,
            windowMax,
            minValuesFormat,
            maxValuesFormat,
            ImGuiSliderFlags_AlwaysClamp))
      {
        viewSettings.setWindowValueLow(viewComp, windowLow);
        viewSettings.setWindowValueHigh(viewComp, windowHigh);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Set the minimum and maximum values of the window range");

      const QuantileOfValue qLow = viewImage.valueToQuantile(viewComp, windowLow);
      const QuantileOfValue qHigh = viewImage.valueToQuantile(viewComp, windowHigh);

      constexpr float windowPercentileMin = 0.0f;
      constexpr float windowPercentileMax = 100.0f;

      const float windowPercentileLowCurrent = 100.0f * static_cast<float>(qLow.lowerQuantile);
      const float windowPercentileHighCurrent = 100.0f * static_cast<float>(qHigh.upperQuantile);

      float windowPercentileLowAttempted = windowPercentileLowCurrent;
      float windowPercentileHighAttempted = windowPercentileHighCurrent;

      if (ImGui::DragFloatRange2(
            "Percentiles",
            &windowPercentileLowAttempted,
            &windowPercentileHighAttempted,
            windowPercentileStep,
            windowPercentileMin,
            windowPercentileMax,
            minPercentilesFormat,
            maxPercentilesFormat,
            ImGuiSliderFlags_AlwaysClamp))
      {
        if (windowPercentileLowCurrent != windowPercentileLowAttempted) {
          const double windowPercentileLowBumped = bumpQuantile(
            viewImage,
            viewComp,
            windowPercentileLowCurrent / 100.0,
            windowPercentileLowAttempted / 100.0,
            windowLow,
            viewSettings.usingExactQuantiles());

          const double newWindowLow = viewImage.quantileToValue(viewComp, windowPercentileLowBumped);
          viewSettings.setWindowValueLow(viewComp, newWindowLow);
          updateImageUniforms();
        }

        if (windowPercentileHighCurrent != windowPercentileHighAttempted) {
          const double windowPercentileHighBumped = bumpQuantile(
            viewImage,
            viewComp,
            windowPercentileHighCurrent / 100.0,
            windowPercentileHighAttempted / 100.0,
            windowHigh,
            viewSettings.usingExactQuantiles());

          const double newWindowHigh = viewImage.quantileToValue(viewComp, windowPercentileHighBumped);
          viewSettings.setWindowValueHigh(viewComp, newWindowHigh);
          updateImageUniforms();
        }
      }
      ImGui::SameLine();
      helpMarker("Set the minimum and maximum percentiles of the window range");

      float threshLow = static_cast<float>(viewSettings.thresholds(viewComp).first);
      float threshHigh = static_cast<float>(viewSettings.thresholds(viewComp).second);

      if (ImGui::DragFloatRange2(
            "Thresholds",
            &threshLow,
            &threshHigh,
            speed,
            threshMin,
            threshMax,
            minValuesFormat,
            maxValuesFormat,
            ImGuiSliderFlags_AlwaysClamp))
      {
        viewSettings.setThresholdLow(viewComp, static_cast<double>(threshLow));
        viewSettings.setThresholdHigh(viewComp, static_cast<double>(threshHigh));
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Lower and upper image thresholds");
    }
    else {
      // Use a speed of 1 for integer images:
      constexpr float speed = 1.0f;

      // Window/level sliders:
      const int32_t windowWidthMin =
        static_cast<int32_t>(std::floor(viewSettings.minMaxWindowWidthRange(viewComp).first));
      const int32_t windowWidthMax =
        static_cast<int32_t>(std::ceil(viewSettings.minMaxWindowWidthRange(viewComp).second));

      const int32_t windowCenterMin =
        static_cast<int32_t>(std::floor(viewSettings.minMaxWindowCenterRange(viewComp).first));
      const int32_t windowCenterMax =
        static_cast<int32_t>(std::ceil(viewSettings.minMaxWindowCenterRange(viewComp).second));

      const int32_t windowMin = static_cast<int32_t>(std::floor(viewSettings.minMaxWindowRange(viewComp).first));
      const int32_t windowMax = static_cast<int32_t>(std::ceil(viewSettings.minMaxWindowRange(viewComp).second));

      int32_t windowLow = static_cast<int32_t>(viewSettings.windowValuesLowHigh(viewComp).first);
      int32_t windowHigh = static_cast<int32_t>(viewSettings.windowValuesLowHigh(viewComp).second);

      int64_t windowWidth = static_cast<int64_t>(viewSettings.windowWidth(viewComp));
      int64_t windowCenter = static_cast<int64_t>(viewSettings.windowCenter(viewComp));

      ImGui::Text("Windowing:");

      if (mySliderS64("Width", &windowWidth, windowWidthMin, windowWidthMax)) {
        viewSettings.setWindowWidth(viewComp, static_cast<double>(windowWidth));
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Window width");

      if (mySliderS64("Level", &windowCenter, windowCenterMin, windowCenterMax)) {
        viewSettings.setWindowCenter(viewComp, static_cast<double>(windowCenter));
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Window level (center)");

      if (ImGui::DragIntRange2(
            "Values",
            &windowLow,
            &windowHigh,
            speed,
            windowMin,
            windowMax,
            "Min: %d",
            "Max: %d",
            ImGuiSliderFlags_AlwaysClamp))
      {
        viewSettings.setWindowValueLow(viewComp, windowLow);
        viewSettings.setWindowValueHigh(viewComp, windowHigh);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Set the minimum and maximum of the window range");

      const QuantileOfValue qLow = viewImage.valueToQuantile(viewComp, static_cast<int64_t>(windowLow));
      const QuantileOfValue qHigh = viewImage.valueToQuantile(viewComp, static_cast<int64_t>(windowHigh));

      constexpr float windowPercentileMin = 0.0f;
      constexpr float windowPercentileMax = 100.0f;

      const float windowPercentileLowCurrent = 100.0f * static_cast<float>(qLow.lowerQuantile);
      const float windowPercentileHighCurrent = 100.0f * static_cast<float>(qHigh.upperQuantile);

      float windowPercentileLowAttempted = windowPercentileLowCurrent;
      float windowPercentileHighAttempted = windowPercentileHighCurrent;

      if (ImGui::DragFloatRange2(
            "Percentiles",
            &windowPercentileLowAttempted,
            &windowPercentileHighAttempted,
            windowPercentileStep,
            windowPercentileMin,
            windowPercentileMax,
            minPercentilesFormat,
            maxPercentilesFormat,
            ImGuiSliderFlags_AlwaysClamp))
      {
        if (windowPercentileLowCurrent != windowPercentileLowAttempted) {
          const double windowPercentileLowBumped = bumpQuantile(
            viewImage,
            viewComp,
            windowPercentileLowCurrent / 100.0,
            windowPercentileLowAttempted / 100.0,
            windowLow,
            viewSettings.usingExactQuantiles());

          const double newWindowLow = viewImage.quantileToValue(viewComp, windowPercentileLowBumped);

          viewSettings.setWindowValueLow(viewComp, newWindowLow);
          updateImageUniforms();
        }

        if (windowPercentileHighCurrent != windowPercentileHighAttempted) {
          const double windowPercentileHighBumped = bumpQuantile(
            viewImage,
            viewComp,
            windowPercentileHighCurrent / 100.0,
            windowPercentileHighAttempted / 100.0,
            windowHigh,
            viewSettings.usingExactQuantiles());

          const double newWindowHigh = viewImage.quantileToValue(viewComp, windowPercentileHighBumped);
          viewSettings.setWindowValueHigh(viewComp, newWindowHigh);
          updateImageUniforms();
        }
      }
      ImGui::SameLine();
      helpMarker("Set the minimum and maximum percentiles of the window range");

      // Threshold range:
      const int32_t threshMin = static_cast<int32_t>(viewSettings.thresholdRange(viewComp).first);
      const int32_t threshMax = static_cast<int32_t>(viewSettings.thresholdRange(viewComp).second);

      int32_t threshLow = static_cast<int32_t>(viewSettings.thresholds(viewComp).first);
      int32_t threshHigh = static_cast<int32_t>(viewSettings.thresholds(viewComp).second);

      /// Speed of range slider is based on the image range
      if (ImGui::DragIntRange2(
            "Thresholds",
            &threshLow,
            &threshHigh,
            speed,
            threshMin,
            threshMax,
            "Min: %d",
            "Max: %d",
            ImGuiSliderFlags_AlwaysClamp))
      {
        viewSettings.setThresholdLow(viewComp, static_cast<double>(threshLow));
        viewSettings.setThresholdHigh(viewComp, static_cast<double>(threshHigh));
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Lower and upper image thresholds");
    }
    if (renderAutoWindowButtons(viewImage, viewSettings, viewComp)) {
      updateImageUniforms();
    }
    ImGui::Spacing();

    auto getImageInterpMode = [&viewSettings]() {
      return (viewSettings.displayImageAsColor()) ? viewSettings.colorInterpolationMode()
                                                  : viewSettings.interpolationMode();
    };

    auto setImageInterpMode = [&viewSettings](const InterpolationMode& mode) {
      (viewSettings.displayImageAsColor()) ? viewSettings.setColorInterpolationMode(mode)
                                           : viewSettings.setInterpolationMode(mode);
    };

    if (ImGui::BeginCombo("Sampling", typeString(getImageInterpMode()).c_str())) {
      for (const auto& mode : AllInterpolationModes) {
        if (ImGui::Selectable(typeString(mode).c_str(), (mode == getImageInterpMode()))) {
          setImageInterpMode(mode);
          updateImageInterpolationMode();

          if (mode == getImageInterpMode()) {
            ImGui::SetItemDefaultFocus();
          }
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    helpMarker("Select the image resampling strategy");

    std::size_t cmapIndex = getCurrentImageColormapIndex();

    ImageColorMap* cmap = getImageColorMap(cmapIndex);

    if (cmap && !viewSettings.displayImageAsColor()) {
      bool* showImageColormapWindow = &(guiData.m_showImageColormapWindow[imageUid]);

      glm::vec3 hsvMods = viewSettings.colorMapHsvModFactors();
      glm::ivec3 hsvModsInt = glm::ivec3{360.0f * hsvMods[0], 100.0f * hsvMods[1], 100.0f * hsvMods[2]};

      // Colormap preview:
      const float contentWidth = ImGui::CalcItemWidth(); // ImGui::GetContentRegionAvail().x;
      const float height = (ImGui::GetFontSize());

      char label[128];
      snprintf(label, 128, "%s##cmap_%zu", cmap->name().c_str(), imageIndex);

      const bool doQuantize =
        (!viewSettings.colorMapContinuous() && (ImageColorMap::InterpolationMode::Linear == cmap->interpolationMode()));

      //            ImGui::Dummy(ImVec2(0.0f, 2.0f));
      ImGui::Spacing();

      // ImGui::Text("Color map:");

      *showImageColormapWindow |= ImGui::paletteButton(
        label,
        cmap->data_RGBA_asVector(),
        viewSettings.isColorMapInverted(),
        doQuantize,
        static_cast<int>(viewSettings.colorMapQuantizationLevels()),
        hsvMods,
        ImVec2(contentWidth, height));

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", cmap->description().c_str());
      }

      ImGui::SetNextItemOpen(false, ImGuiCond_Appearing);

      if (ImGui::TreeNode("Color map settings")) {
        // Image colormap dialog:
        *showImageColormapWindow |= ImGui::Button("Select color map");

        bool invertedCmap = viewSettings.isColorMapInverted();

        if (ImGui::Checkbox("Inverted", &invertedCmap)) {
          viewSettings.setColorMapInverted(invertedCmap);
          updateImageUniforms();
        }
        ImGui::SameLine();
        helpMarker("Invert the image color map");

        // If the color map has nearest-neighbor interpolation mode,
        // then we are forced to use the discrete setting:
        //                const bool forcedDiscrete = (ImageColorMap::InterpolationMode::Nearest ==
        //                cmap->interpolationMode());

        bool colorMapContinuous = viewSettings.colorMapContinuous();

        ImGui::SameLine();
        if (ImGui::RadioButton("Continuous", colorMapContinuous /*&& ! forcedDiscrete*/)) {
          colorMapContinuous = true;
          viewSettings.setColorMapContinuous(colorMapContinuous);
          updateImageUniforms();
        }

        ImGui::SameLine();
        if (ImGui::RadioButton("Discrete", !colorMapContinuous /*|| forcedDiscrete*/)) {
          colorMapContinuous = false;
          viewSettings.setColorMapContinuous(colorMapContinuous);
          updateImageUniforms();
        }

        ImGui::SameLine();
        helpMarker("Render color map as either continuous or discrete");

        if (!colorMapContinuous) {
          int numColorMapLevels = static_cast<int>(viewSettings.colorMapQuantizationLevels());

          ImGui::InputInt("Color levels", &numColorMapLevels);
          {
            numColorMapLevels = std::min(std::max(numColorMapLevels, 2), 256);
            viewSettings.setColorMapQuantizationLevels(static_cast<uint32_t>(numColorMapLevels));
            updateImageUniforms();
          }
          ImGui::SameLine();
          helpMarker("Number of image color map quantization levels");
        }

        static constexpr int hue_min = 0;
        static constexpr int hue_max = 360;

        static constexpr int sat_min = 0;
        static constexpr int sat_max = 100;

        static constexpr int val_min = 0;
        static constexpr int val_max = 100;

        ImGui::Spacing();
        ImGui::Text("HSV color adjustment:");
        /*
                    if (ImGuiKnobs::KnobInt(
                            "Hue", glm::value_ptr(hsvModsInt), hue_min, hue_max, 1, "%i%",
                            ImGuiKnobVariant_Stepped, 0,
                            ImGuiKnobFlags_ValueTooltip | ImGuiKnobFlags_DragHorizontal, 12))
                    {
                        imgSettings.setColorMapHueModFactor(hsvModsInt[0] / 360.0f);
                        updateImageUniforms();
                    }

                    if (ImGui::SliderScalarN("Sat. & value", ImGuiDataType_S32, &(hsvModsInt[1]), 2,
           &sv_min, &sv_max))
                    {
                        // imgSettings.setColormapHsvModfactors(glm::vec3{hsvMods} / 360.0f);
                        imgSettings.setColorMapSatModFactor(hsvModsInt[1] / 100.0f);
                        imgSettings.setColorMapValModFactor(hsvModsInt[2] / 100.0f);
                        updateImageUniforms();
                    }

                    ImGui::SameLine(); helpMarker("Apply saturation and value adjustments to the
           color map");
*/

        const int* hsv_mins[3] = {&hue_min, &sat_min, &val_min};
        const int* hsv_maxs[3] = {&hue_max, &sat_max, &val_max};

        const std::string h_format("%d deg");
        const std::string s_format("%d");
        const std::string v_format("%d");

        const char* hsv_formats[3] = {h_format.c_str(), s_format.c_str(), v_format.c_str()};

        if (ImGui::SliderScalarN_multiComp(
              "HSV",
              ImGuiDataType_S32,
              glm::value_ptr(hsvModsInt),
              3,
              reinterpret_cast<const void**>(hsv_mins),
              reinterpret_cast<const void**>(hsv_maxs),
              hsv_formats,
              0))
        {
          viewSettings.setColorMapHueModFactor(hsvModsInt[0] / 360.0f);
          viewSettings.setColorMapSatModFactor(hsvModsInt[1] / 100.0f);
          viewSettings.setColorMapValModFactor(hsvModsInt[2] / 100.0f);
          updateImageUniforms();
        }

        ImGui::TreePop();
      }

      auto getImageColorMapInverted = [&viewSettings]() {
        return viewSettings.isColorMapInverted();
      };

      auto getImageColorMapContinuous = [&viewSettings]() {
        return viewSettings.colorMapContinuous();
      };

      auto getImageColorMapLevels = [&viewSettings]() {
        return static_cast<int>(viewSettings.colorMapQuantizationLevels());
      };

      renderPaletteWindow(
        std::string(
          "Select colormap for image '" + imgSettings.displayName() + "' (component " + std::to_string(displayComp) +
          ")")
          .c_str(),
        showImageColormapWindow,
        getNumImageColorMaps,
        getImageColorMap,
        getCurrentImageColormapIndex,
        setCurrentImageColormapIndex,
        getImageColorMapInverted,
        getImageColorMapContinuous,
        getImageColorMapLevels,
        hsvMods,
        updateImageUniforms);
    }

    // Edge settings
    ImGui::Spacing();
    ImGui::Spacing();

    bool showEdges = viewSettings.showAnyEdges();
    if (ImGui::Checkbox("Show edges", &showEdges)) {
      viewSettings.setShowAnyEdges(showEdges);
      updateImageUniforms();
    }
    ImGui::SameLine();
    helpMarker("Show/hide image edges (E). Edge settings control whether edges are computed in voxel or pixel space.");

    ImGui::SetNextItemOpen(showEdges, ImGuiCond_Appearing);
    if (showEdges && ImGui::TreeNode("Edge settings")) {
      EdgeDetectionMethod edgeMethod = viewSettings.edgeDetectionMethod();
      int edgeMethodIndex = EdgeDetectionMethod::Voxel == edgeMethod ? 0 : 1;
      if (ImGui::RadioButton("Voxel-space", edgeMethodIndex == 0)) {
        edgeMethodIndex = 0;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("Pixel-space", edgeMethodIndex == 1)) {
        edgeMethodIndex = 1;
      }
      const EdgeDetectionMethod selectedEdgeMethod =
        edgeMethodIndex == 0 ? EdgeDetectionMethod::Voxel : EdgeDetectionMethod::Pixel;
      if (selectedEdgeMethod != edgeMethod) {
        viewSettings.setEdgeDetectionMethod(selectedEdgeMethod);
        edgeMethod = selectedEdgeMethod;
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker(
        "Voxel-space edges are computed from image samples. Pixel-space edges are computed after the image is sampled "
        "into the view.");

      const bool useVoxelEdges = EdgeDetectionMethod::Voxel == edgeMethod;
      const bool usePixelEdges = EdgeDetectionMethod::Pixel == edgeMethod;

      if (useVoxelEdges && InterpolationMode::NearestNeighbor == viewSettings.interpolationMode()) {
        ImGui::Text("Note: Linear or cubic interpolation are recommended when showing edges.");
      }

      bool hardEdges = useVoxelEdges ? viewSettings.thresholdEdges() : viewSettings.thresholdPixelEdges();
      if (ImGui::Checkbox("Hard edges", &hardEdges)) {
        if (useVoxelEdges) {
          viewSettings.setThresholdEdges(hardEdges);
        }
        else {
          viewSettings.setThresholdPixelEdges(hardEdges);
        }
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Apply thresholding to edge magnitude.");

      if (usePixelEdges) {
        bool thinPixelEdges = viewSettings.thinPixelEdges();
        if (ImGui::Checkbox("Thin edges", &thinPixelEdges)) {
          viewSettings.setThinPixelEdges(thinPixelEdges);
          updateImageUniforms();
        }
        ImGui::SameLine();
        helpMarker("Keep only local edge-magnitude maxima along the screen-space gradient direction.");
      }

      bool overlayEdges = useVoxelEdges ? viewSettings.overlayEdges() : viewSettings.overlayPixelEdges();
      if (ImGui::Checkbox("Overlay edges on image", &overlayEdges)) {
        if (useVoxelEdges) {
          viewSettings.setOverlayEdges(overlayEdges);
        }
        else {
          viewSettings.setOverlayPixelEdges(overlayEdges);
        }
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Overlay edges on top of the image.");

      if (useVoxelEdges) {
        if (overlayEdges || hardEdges) {
          if (viewSettings.colormapEdges()) {
            viewSettings.setColormapEdges(false);
            updateImageUniforms();
          }
        }
        bool colormapEdges = viewSettings.colormapEdges();
        if (overlayEdges || hardEdges) {
          ImGui::BeginDisabled();
          ImGui::Checkbox("Apply colormap to edges", &colormapEdges);
          ImGui::EndDisabled();
        }
        else if (ImGui::Checkbox("Apply colormap to edges", &colormapEdges)) {
          viewSettings.setColormapEdges(colormapEdges);
          updateImageUniforms();
        }
        ImGui::SameLine();
        helpMarker("Apply the image colormap to voxel-space edge magnitudes.");
      }
      else if (viewSettings.colormapEdges()) {
        viewSettings.setColormapEdges(false);
        updateImageUniforms();
      }

      if (usePixelEdges) {
        double edgeScale = viewSettings.pixelEdgeScale();
        if (mySliderF64("Scale", &edgeScale, 0.01, 10.00)) {
          viewSettings.setPixelEdgeScale(edgeScale);
          updateImageUniforms();
        }
        ImGui::SameLine();
        helpMarker("Scale applied to screen-space edge magnitude.");
      }
      else if (!hardEdges) {
        double edgeScale = 1.0 - viewSettings.edgeMagnitude();
        if (mySliderF64("Scale", &edgeScale, 0.01, 1.00)) {
          viewSettings.setEdgeMagnitude(1.0 - edgeScale);
          updateImageUniforms();
        }
        ImGui::SameLine();
        helpMarker("Scale applied to voxel-space edge magnitude.");
      }

      if (hardEdges) {
        if (usePixelEdges) {
          double edgeThreshold = viewSettings.pixelEdgeThreshold();
          if (mySliderF64("Threshold", &edgeThreshold, 0.0, 1.0)) {
            viewSettings.setPixelEdgeThreshold(edgeThreshold);
            updateImageUniforms();
          }
        }
        else {
          double edgeThreshold = viewSettings.edgeMagnitude();
          if (mySliderF64("Threshold", &edgeThreshold, 0.01, 1.00)) {
            viewSettings.setEdgeMagnitude(edgeThreshold);
            updateImageUniforms();
          }
        }
        ImGui::SameLine();
        helpMarker("Magnitude threshold above which hard edges are shown.");
      }

      if (!(useVoxelEdges && viewSettings.colormapEdges())) {
        glm::vec4 edgeColor{viewSettings.edgeColor(), viewSettings.edgeOpacity()};
        if (ImGui::ColorEdit4("Edge color", glm::value_ptr(edgeColor), colorAlphaEditFlags)) {
          viewSettings.setEdgeColor(edgeColor);
          viewSettings.setEdgeOpacity(static_cast<double>(edgeColor.a));
          updateImageUniforms();
        }
        ImGui::SameLine();
        helpMarker("Edge color and opacity.");
      }

      ImGui::TreePop();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Image Header")) {
    renderImageHeaderInformation(appData, imageUid, *image, updateImageUniforms, recenterAllViews);
    ImGui::TreePop();
    ImGui::Separator();
  }

  renderImageFileMetadata(image->header());

  renderImageDicomMetadata(appData, imageUid);

  if (ImGui::TreeNode("Transformations")) {
    /// @note This code is commented out for now, since additional images are implicitly locked
    /// to the reference image, since the reference image does not get transformed.

    /*
        if (! isRef)
        {
            ImGui::Spacing();

            // Note: We could change this to imgSettings.isLockedToReference(),
            // if at some time in the future we allow transformation of the reference image.

            bool lockTxToReference = true;

            if (ImGui::Checkbox("Lock to reference", &lockTxToReference))
            {
                imgSettings.setLockedToReference(lockTxToReference);
                updateImageUniforms();
            }
            ImGui::SameLine();
            helpMarker("Lock this image's transformation to the reference image, "
                        "so that it moves with the reference (always true)");

            ImGui::Spacing();
            ImGui::Separator();
        }
        */

    ImGui::Spacing();

    // The initial affine and manual affine transformations are disabled for the reference image:
    const bool forceDisableInitialTxs = isRef;

    ImGui::Text("Initial affine transformation:");
    ImGui::SameLine();
    helpMarker("Initial affine transformation matrix (read from file)");

    bool enable_affine_T_subject = imgTx.get_enable_affine_T_subject();
    if (ImGui::Checkbox("Apply##affine_T_subject", &enable_affine_T_subject) && !forceDisableInitialTxs) {
      imgTx.set_enable_affine_T_subject(enable_affine_T_subject);
      updateImageUniforms();
    }
    ImGui::SameLine();

    if (forceDisableInitialTxs) {
      helpMarker(
        "Enable/disable application of the initial affine transformation. "
        "Always disabled for the reference image.");
    }
    else {
      helpMarker("Enable/disable application of the initial affine transformation.");
    }

    if (enable_affine_T_subject && !forceDisableInitialTxs) {
      if (auto fileName = imgTx.get_affine_T_subject_fileName()) {
        std::string fileNameString = fileName->string();
        ImGui::InputText("File", &fileNameString, ImGuiInputTextFlags_ReadOnly);
        ImGui::Spacing();
      }

      glm::mat4 aff_T_sub = glm::transpose(imgTx.get_affine_T_subject());

      ImGui::PushID("initAffineTx");
      ImGui::PushItemWidth(-1);
      ImGui::InputFloat4("##init_col0", glm::value_ptr(aff_T_sub[0]), txFormat, ImGuiInputTextFlags_ReadOnly);
      ImGui::InputFloat4("##init_col1", glm::value_ptr(aff_T_sub[1]), txFormat, ImGuiInputTextFlags_ReadOnly);
      ImGui::InputFloat4("##init_col2", glm::value_ptr(aff_T_sub[2]), txFormat, ImGuiInputTextFlags_ReadOnly);
      ImGui::InputFloat4("##init_col3", glm::value_ptr(aff_T_sub[3]), txFormat, ImGuiInputTextFlags_ReadOnly);
      ImGui::PopItemWidth();
      ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Manual affine transformation:");
    ImGui::SameLine();
    helpMarker("Manual affine transformation from Subject to World space");

    bool enable_worldDef_T_affine = imgTx.get_enable_worldDef_T_affine();
    if (ImGui::Checkbox("Apply##worldDef_T_affine", &enable_worldDef_T_affine) && !forceDisableInitialTxs) {
      imgTx.set_enable_worldDef_T_affine(enable_worldDef_T_affine);
      updateImageUniforms();
    }
    ImGui::SameLine();

    if (forceDisableInitialTxs) {
      helpMarker(
        "Enable/disable application of the manual affine transformation from Subject to "
        "World space. Always disabled for the reference image.");
    }
    else {
      helpMarker(
        "Enable/disable application of the manual affine transformation from Subject to World "
        "space.");
    }

    if (enable_worldDef_T_affine && !forceDisableInitialTxs) {
      glm::quat w_T_s_rotation = imgTx.get_worldDef_T_affine_rotation();
      glm::vec3 w_T_s_scale = imgTx.get_worldDef_T_affine_scale();
      glm::vec3 w_T_s_trans = imgTx.get_worldDef_T_affine_translation();

      float angle = glm::degrees(glm::angle(w_T_s_rotation));
      glm::vec3 axis = glm::normalize(glm::axis(w_T_s_rotation));

      if (ImGui::InputFloat3("Translation", glm::value_ptr(w_T_s_trans), txFormat)) {
        imgTx.set_worldDef_T_affine_translation(w_T_s_trans);
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker("Image translation in x, y, z");

      if (ImGui::InputFloat3("Scale", glm::value_ptr(w_T_s_scale), txFormat)) {
        if (
          glm::epsilonNotEqual(w_T_s_scale.x, 0.0f, glm::epsilon<float>()) &&
          glm::epsilonNotEqual(w_T_s_scale.y, 0.0f, glm::epsilon<float>()) &&
          glm::epsilonNotEqual(w_T_s_scale.z, 0.0f, glm::epsilon<float>()))
        {
          imgTx.set_worldDef_T_affine_scale(w_T_s_scale);
          updateImageUniforms();
        }
      }
      ImGui::SameLine();
      helpMarker("Image scale in x, y, z");

      /// @todo Put in a more friendly rotation widget. For now, disable changing the rotation
      /// @see https://github.com/BrutPitt/imGuIZMO.quat
      /// @see https://github.com/CedricGuillemet/ImGuizmo

      //            ImGui::Text("Rotation");
      if (ImGui::InputFloat("Rotation angle", &angle, 0.01f, 0.1f, txFormat)) {
        //                const float n = glm::length(axis);
        //                if (n < 1e-6f)
        //                {
        //                    const glm::quat newRot = glm::angleAxis(glm::radians(angle),
        //                    glm::normalize(axis)); imgTx.set_worldDef_T_affine_rotation(newRot);
        //                    updateImageUniforms();
        //                }
      }
      ImGui::SameLine();
      helpMarker("Image rotation angle (degrees) [editing disabled]");

      if (ImGui::InputFloat3("Rotation axis", glm::value_ptr(axis), txFormat)) {
        //                const float n = glm::length(axis);
        //                if (n < 1e-6f)
        //                {
        //                    const glm::quat newRot = glm::angleAxis(glm::radians(angle),
        //                    glm::normalize(axis)); imgTx.set_worldDef_T_affine_rotation(newRot);
        //                    updateImageUniforms();
        //                }
      }
      ImGui::SameLine();
      helpMarker("Image rotation axis [editing disabled]");

      //            if (ImGui::InputFloat4("Rotation", glm::value_ptr(w_T_s_rotation), "%.3f"))
      //            {
      //                imgTx.set_worldDef_T_affine_rotation(w_T_s_rotation);
      //                updateImageUniforms();
      //            }
      //            ImGui::SameLine();
      //            HelpMarker("Image rotation defined as a quaternion");

      ImGui::Spacing();
      glm::mat4 world_T_affine = glm::transpose(imgTx.get_worldDef_T_affine());

      ImGui::PushID("subjectToWorld");
      ImGui::PushItemWidth(-1);
      ImGui::Text("Subject-to-World matrix:");
      ImGui::InputFloat4("##sTw_col0", glm::value_ptr(world_T_affine[0]), txFormat, ImGuiInputTextFlags_ReadOnly);
      ImGui::InputFloat4("##sTw_col1", glm::value_ptr(world_T_affine[1]), txFormat, ImGuiInputTextFlags_ReadOnly);
      ImGui::InputFloat4("##sTw_col2", glm::value_ptr(world_T_affine[2]), txFormat, ImGuiInputTextFlags_ReadOnly);
      ImGui::InputFloat4("##sTw_col3", glm::value_ptr(world_T_affine[3]), txFormat, ImGuiInputTextFlags_ReadOnly);
      ImGui::PopItemWidth();
      ImGui::PopID();

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      if (ImGui::Button("Reset manual transformation to identity")) {
        imgTx.reset_worldDef_T_affine();
        updateImageUniforms();
      }
      ImGui::SameLine();
      helpMarker(
        "Reset the manual component of the affine transformation matrix from Subject to World "
        "space");

      // Save manual tx to file:
      static const char* buttonText("Save manual transformation...");
      static const char* dialogTitle("Select Manual Transformation");
      static const auto dialogFilters = native_dialog::transformFilters();

      const auto selectedManualTxFile = ImGui::renderFileButtonDialogAndWindow(buttonText, dialogTitle, dialogFilters);

      ImGui::SameLine();
      helpMarker(
        "Save the manual component of the affine transformation matrix from Subject to World "
        "space");

      if (selectedManualTxFile) {
        const glm::dmat4 worldDef_T_affine{imgTx.get_worldDef_T_affine()};
        if (serialize::saveAffineTxFile(worldDef_T_affine, *selectedManualTxFile)) {
          spdlog::info("Saved manual transformation matrix to file {}", *selectedManualTxFile);
        }
        else {
          spdlog::error("Error saving manual transformation matrix to file {}", *selectedManualTxFile);
        }
      }

      if (imgTx.get_enable_affine_T_subject()) {
        // Save concatenated initial + manual tx to file:
        static const char* saveInitAndManualTxButtonText("Save initial + manual transformation...");
        static const char* saveInitAndManualTxDialogTitle("Select Concatenated Initial and Manual Transformation");

        const auto selectedInitAndManualConcatTxFile = ImGui::renderFileButtonDialogAndWindow(
          saveInitAndManualTxButtonText,
          saveInitAndManualTxDialogTitle,
          dialogFilters);

        ImGui::SameLine();
        helpMarker(
          "Save the concatenated initial and manual affine transformation matrix from Subject to "
          "World space");

        if (selectedInitAndManualConcatTxFile) {
          const glm::dmat4 affine_T_subject{imgTx.get_affine_T_subject()};
          const glm::dmat4 worldDef_T_affine{imgTx.get_worldDef_T_affine()};

          if (serialize::saveAffineTxFile(worldDef_T_affine * affine_T_subject, *selectedInitAndManualConcatTxFile)) {
            spdlog::info(
              "Saved concatenated initial and manual affine transformation matrix to file {}",
              *selectedInitAndManualConcatTxFile);
          }
          else {
            spdlog::error(
              "Error saving concatenated initial and manual affine transformation matrix to file "
              "{}",
              *selectedInitAndManualConcatTxFile);
          }
        }
      }
    }

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::TreePop();
  }

  if (!image->hasPixelData()) {
    ImGui::TextUnformatted("Pixel data is not loaded yet.");
    ImGui::PopID(); // imageUid
    return;
  }

  if (ImGui::TreeNode("Histogram")) {
    if (image->header().numPixels() > std::numeric_limits<int32_t>::max()) {
      spdlog::warn(
        "Number of pixels in image ({}) exceeds maximum supported by image histogram",
        image->header().numPixels());
    }

    const uint32_t comp = activeComp;
    const void* buffer = image->bufferAsVoid(comp);
    const int bufferSize = static_cast<int>(image->header().numPixels());
    const std::string& format = appData.guiData().m_imageValuePrecisionFormat;

    switch (image->header().memoryComponentType()) {
      case ComponentType::Int8: {
        drawImageHistogram(static_cast<const int8_t*>(buffer), bufferSize, imgSettings, format);
        break;
      }
      case ComponentType::UInt8: {
        drawImageHistogram(static_cast<const uint8_t*>(buffer), bufferSize, imgSettings, format);
        break;
      }
      case ComponentType::Int16: {
        drawImageHistogram(static_cast<const int16_t*>(buffer), bufferSize, imgSettings, format);
        break;
      }
      case ComponentType::UInt16: {
        drawImageHistogram(static_cast<const uint16_t*>(buffer), bufferSize, imgSettings, format);
        break;
      }
      case ComponentType::Int32: {
        drawImageHistogram(static_cast<const int32_t*>(buffer), bufferSize, imgSettings, format);
        break;
      }
      case ComponentType::UInt32: {
        drawImageHistogram(static_cast<const uint32_t*>(buffer), bufferSize, imgSettings, format);
        break;
      }
      case ComponentType::Float32: {
        drawImageHistogram(static_cast<const float*>(buffer), bufferSize, imgSettings, format);
        break;
      }
      default: {
        break;
      }
    }

    ImGui::TreePop();
  }

  ImGui::Spacing();

  ImGui::PopID(); // imageUid
}
