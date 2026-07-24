#pragma once

#include "logic/serialization/ProjectSerialization.h"

#include "common/Exception.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using json = nlohmann::json;
namespace fs = std::filesystem;

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

namespace
{
template<typename T>
void addIfChanged(json& j, const char* key, const T& value, const T& defaultValue)
{
  if (value != defaultValue) {
    j[key] = value;
  }
}

void addIfChanged(json& j, const char* key, const json& value, const json& defaultValue)
{
  if (value != defaultValue) {
    j[key] = value;
  }
}

void addIfNotEmpty(json& j, const char* key, json value)
{
  if (!value.empty()) {
    j[key] = std::move(value);
  }
}

void removeDefaultEntries(json& value, const json& defaultValue)
{
  if (!value.is_object() || !defaultValue.is_object()) {
    return;
  }

  for (auto it = value.begin(); it != value.end();) {
    const auto defaultIt = defaultValue.find(it.key());
    if (defaultIt != defaultValue.end() && it.value() == defaultIt.value()) {
      it = value.erase(it);
    }
    else {
      ++it;
    }
  }
}

template<typename Enum>
struct EnumName
{
  Enum value;
  std::string_view name;
};

template<typename Enum>
EnumName(Enum, std::string_view) -> EnumName<Enum>;

template<typename Enum, std::size_t N>
const char* enumToName(Enum value, const std::array<EnumName<Enum>, N>& names)
{
  const auto it =
    std::find_if(names.begin(), names.end(), [value](const EnumName<Enum>& entry) { return entry.value == value; });
  return it != names.end() ? it->name.data() : names.front().name.data();
}

template<typename Enum, std::size_t N>
std::optional<Enum> enumFromName(std::string_view name, const std::array<EnumName<Enum>, N>& names)
{
  const auto it =
    std::find_if(names.begin(), names.end(), [name](const EnumName<Enum>& entry) { return entry.name == name; });
  if (it == names.end()) {
    return std::nullopt;
  }
  return it->value;
}

constexpr std::array k_anatomicalLabelNames{
  EnumName{AnatomicalLabelType::Human, "human"},
  EnumName{AnatomicalLabelType::Cartesian, "cartesian"},
  EnumName{AnatomicalLabelType::Rodent, "rodent"},
  EnumName{AnatomicalLabelType::Disabled, "disabled"}};

constexpr std::array k_crosshairsSnappingNames{
  EnumName{CrosshairsSnapping::Disabled, "disabled"},
  EnumName{CrosshairsSnapping::ReferenceImage, "referenceImage"},
  EnumName{CrosshairsSnapping::ActiveImage, "activeImage"}};

constexpr std::array k_edgeDetectionMethodNames{
  EnumName{serialize::ProjectEdgeDetectionMethod::Pixel, "pixel"},
  EnumName{serialize::ProjectEdgeDetectionMethod::Voxel, "voxel"}};

constexpr std::array k_componentRenderModeNames{
  EnumName{serialize::ProjectComponentRenderMode::SingleComponent, "singleComponent"},
  EnumName{serialize::ProjectComponentRenderMode::Color, "color"},
  EnumName{serialize::ProjectComponentRenderMode::Minimum, "minimum"},
  EnumName{serialize::ProjectComponentRenderMode::Mean, "mean"},
  EnumName{serialize::ProjectComponentRenderMode::Maximum, "maximum"},
  EnumName{serialize::ProjectComponentRenderMode::Magnitude, "magnitude"},
  EnumName{serialize::ProjectComponentRenderMode::ComplexPhase, "complexPhase"},
  EnumName{serialize::ProjectComponentRenderMode::ComplexReal, "complexReal"},
  EnumName{serialize::ProjectComponentRenderMode::ComplexImaginary, "complexImaginary"},
  EnumName{serialize::ProjectComponentRenderMode::VectorDirectionColor, "vectorDirectionColor"},
  EnumName{serialize::ProjectComponentRenderMode::VectorSignedNormalProjection, "vectorSignedNormalProjection"},
  EnumName{serialize::ProjectComponentRenderMode::VectorPlanarProjectionColor, "vectorPlanarProjectionColor"},
  EnumName{serialize::ProjectComponentRenderMode::VectorJacobianDeterminant, "vectorJacobianDeterminant"},
  EnumName{serialize::ProjectComponentRenderMode::VectorGradientMagnitude, "vectorGradientMagnitude"},
  EnumName{serialize::ProjectComponentRenderMode::VectorDivergence, "vectorDivergence"},
  EnumName{serialize::ProjectComponentRenderMode::VectorCurlMagnitude, "vectorCurlMagnitude"},
  EnumName{serialize::ProjectComponentRenderMode::VectorLaplacianMagnitude, "vectorLaplacianMagnitude"}};

constexpr std::array k_complexPhaseUnitNames{
  EnumName{serialize::ProjectComplexPhaseUnit::Radians, "radians"},
  EnumName{serialize::ProjectComplexPhaseUnit::Degrees, "degrees"}};

constexpr std::array k_complexPhaseRangeNames{
  EnumName{serialize::ProjectComplexPhaseRange::Signed, "signed"},
  EnumName{serialize::ProjectComplexPhaseRange::Unsigned, "unsigned"}};

constexpr std::array k_vectorArrowOverlaySpacingModeNames{
  EnumName{serialize::ProjectVectorArrowOverlaySpacingMode::Pixels, "pixels"},
  EnumName{serialize::ProjectVectorArrowOverlaySpacingMode::Voxels, "voxels"},
  EnumName{serialize::ProjectVectorArrowOverlaySpacingMode::Millimeters, "millimeters"}};

constexpr std::array k_vectorWarpedGridConventionNames{
  EnumName{serialize::ProjectVectorWarpedGridConvention::SamplingField, "samplingField"},
  EnumName{serialize::ProjectVectorWarpedGridConvention::ApparentDeformation, "apparentDeformation"}};

constexpr std::array k_localNccPresentationNames{
  EnumName{serialize::ProjectLocalNccPresentation::Dissimilarity, "dissimilarity"},
  EnumName{serialize::ProjectLocalNccPresentation::Correlation, "correlation"}};

constexpr std::array k_localMetricInvalidStyleNames{
  EnumName{serialize::ProjectLocalMetricInvalidStyle::Transparent, "transparent"},
  EnumName{serialize::ProjectLocalMetricInvalidStyle::Gray, "gray"}};

constexpr std::array k_raycastSegmentationMaskingNames{
  EnumName{serialize::ProjectSegmentationRaycastMasking::Disabled, "disabled"},
  EnumName{serialize::ProjectSegmentationRaycastMasking::MaskIn, "maskIn"},
  EnumName{serialize::ProjectSegmentationRaycastMasking::MaskOut, "maskOut"}};

constexpr std::array k_landmarkCoordinateSpaceNames{
  EnumName{serialize::ProjectLandmarkCoordinateSpace::Subject, "subject"},
  EnumName{serialize::ProjectLandmarkCoordinateSpace::Voxel, "voxel"}};

constexpr std::array k_segmentationOutlineNames{
  EnumName{SegmentationOutlineStyle::Disabled, "disabled"},
  EnumName{SegmentationOutlineStyle::ViewPixel, "pixel"},
  EnumName{SegmentationOutlineStyle::ImageVoxel, "voxel"}};

constexpr std::array k_interpolationModeNames{
  EnumName{InterpolationMode::NearestNeighbor, "nearest"},
  EnumName{InterpolationMode::Linear, "linear"},
  EnumName{InterpolationMode::CubicBsplineConvolution, "cubicBSpline"}};

constexpr std::array k_floatingPointInterpolationPolicyNames{
  EnumName{FloatingPointLinearInterpolationPolicy::Automatic, "automatic"},
  EnumName{FloatingPointLinearInterpolationPolicy::FixedFunction, "fixedFunction"},
  EnumName{FloatingPointLinearInterpolationPolicy::FloatingPoint, "floatingPoint"}};

std::optional<std::uint32_t> unsignedIntFromJson(const json& value)
{
  if (value.is_number_unsigned()) {
    return value.get<std::uint32_t>();
  }
  if (value.is_number_integer()) {
    const auto parsed = value.get<std::int64_t>();
    if (parsed >= 0) {
      return static_cast<std::uint32_t>(parsed);
    }
  }
  return std::nullopt;
}

json matrixToJson(const glm::mat4& matrix)
{
  json j = json::array();

  for (int r = 0; r < 4; ++r) {
    json row = json::array();
    for (int c = 0; c < 4; ++c) {
      row.push_back(matrix[c][r]);
    }
    j.push_back(std::move(row));
  }

  return j;
}

json vec2ToJson(const glm::vec2& value)
{
  return json::array({value.x, value.y});
}

json vec3ToJson(const glm::vec3& value)
{
  return json::array({value.x, value.y, value.z});
}

json vec4ToJson(const glm::vec4& value)
{
  return json::array({value.x, value.y, value.z, value.w});
}

glm::vec2 vec2FromJson(const json& j)
{
  return glm::vec2{j.at(0).get<float>(), j.at(1).get<float>()};
}

glm::vec3 vec3FromJson(const json& j)
{
  return glm::vec3{j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>()};
}

glm::vec4 vec4FromJson(const json& j)
{
  return glm::vec4{j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>()};
}

json mat3ToJson(const glm::mat3& matrix)
{
  json j = json::array();
  for (int col = 0; col < 3; ++col) {
    j.push_back(vec3ToJson(matrix[col]));
  }
  return j;
}

glm::mat3 mat3FromJson(const json& j)
{
  glm::mat3 matrix{1.0f};
  for (int col = 0; col < 3; ++col) {
    matrix[col] = vec3FromJson(j.at(static_cast<std::size_t>(col)));
  }
  return matrix;
}

glm::mat4 matrixFromJson(const json& j)
{
  glm::mat4 matrix{1.0f};

  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      matrix[c][r] = j.at(r).at(c).get<float>();
    }
  }

  return matrix;
}

json affineToJson(const std::optional<fs::path>& path, const std::optional<glm::mat4>& matrix)
{
  if (matrix) {
    return json{{"matrix", matrixToJson(*matrix)}};
  }

  if (path) {
    return json{{"path", path->generic_string()}};
  }

  return json::object();
}

void affineFromJson(const json& j, std::optional<fs::path>& path, std::optional<glm::mat4>& matrix)
{
  path = std::nullopt;
  matrix = std::nullopt;

  if (!j.is_object()) {
    throwDebug("Affine transform JSON must be an object");
  }

  if (j.contains("matrix")) {
    matrix = matrixFromJson(j.at("matrix"));
    if (j.contains("path")) {
      spdlog::warn("Affine transform JSON contains both matrix and path; using embedded matrix");
    }
    return;
  }

  if (j.contains("path")) {
    path = j.at("path").get<std::string>();
    return;
  }

  throwDebug("Affine transform JSON must contain either a matrix or path key");
}

std::string pathToString(const fs::path& path)
{
  return path.generic_string();
}

json pathObjectToJson(const fs::path& path)
{
  return json{{"path", pathToString(path)}};
}

void optionalPathObjectToJson(json& j, const char* key, const std::optional<fs::path>& path)
{
  if (path) {
    j[key] = pathObjectToJson(*path);
  }
}

fs::path pathObjectFromJson(const json& value, const char* key)
{
  if (!value.is_object()) {
    throwDebug(std::string{key} + " must be an object");
  }

  const auto path = value.find("path");
  if (path == value.end() || !path->is_string()) {
    throwDebug(std::string{key} + " must contain a path string");
  }

  return path->get<std::string>();
}

void optionalPathObjectFromJson(const json& j, const char* key, std::optional<fs::path>& path)
{
  path = std::nullopt;
  if (const auto value = j.find(key); value != j.end()) {
    path = pathObjectFromJson(*value, key);
  }
}

json pathObjectVectorToJson(const std::vector<fs::path>& paths)
{
  json values = json::array();
  for (const fs::path& path : paths) {
    values.push_back(pathObjectToJson(path));
  }
  return values;
}

std::vector<fs::path> pathObjectVectorFromJson(const json& j, const char* key)
{
  std::vector<fs::path> paths;
  if (!j.is_array()) {
    throwDebug(std::string{key} + " must be an array");
  }

  paths.reserve(j.size());
  for (const json& value : j) {
    paths.push_back(pathObjectFromJson(value, key));
  }
  return paths;
}

template<typename T>
std::optional<T> vectorValue(const std::vector<T>& values, const std::size_t index)
{
  if (index >= values.size()) {
    return std::nullopt;
  }
  return values.at(index);
}

template<typename T>
std::optional<T>
sparseVectorValue(const std::vector<T>& values, const std::set<std::size_t>& presentIndices, const std::size_t index)
{
  const bool present = presentIndices.empty() ? index < values.size() : presentIndices.contains(index);
  if (!present || index >= values.size()) {
    return std::nullopt;
  }
  return values.at(index);
}

std::size_t maxSparseIndexPlusOne(const std::set<std::size_t>& indices)
{
  return indices.empty() ? 0u : *indices.rbegin() + 1u;
}

template<typename T>
void setVectorValue(std::vector<T>& values, const std::size_t index, const T& value)
{
  if (index >= values.size()) {
    values.resize(index + 1);
  }
  values.at(index) = value;
}

template<typename T>
void setVectorValue(std::vector<T>& values, const std::size_t index, const T& value, const T& fillValue)
{
  if (index >= values.size()) {
    values.resize(index + 1, fillValue);
  }
  values.at(index) = value;
}

template<typename T>
void setSparseVectorValue(
  std::vector<T>& values,
  std::set<std::size_t>& presentIndices,
  const std::size_t index,
  const T& value,
  const T& fillValue)
{
  setVectorValue(values, index, value, fillValue);
  presentIndices.insert(index);
}

std::size_t imageComponentSettingsCount(const serialize::ImageSettings& settings)
{
  return std::max(
    {std::size_t{1},
     settings.m_componentLevels.size(),
     settings.m_componentWindows.size(),
     settings.m_componentThresholdLows.size(),
     settings.m_componentThresholdHighs.size(),
     settings.m_componentVisibility.size(),
     settings.m_componentOpacities.size(),
     settings.m_colorMapIndices.size(),
     settings.m_colorMapInverted.size(),
     settings.m_colorMapContinuous.size(),
     settings.m_colorMapLevels.size(),
     settings.m_colorMapHsvModifiers.size(),
     settings.m_interpolationModes.size(),
     settings.m_foregroundThresholdLows.size(),
     settings.m_foregroundThresholdHighs.size(),
     maxSparseIndexPlusOne(settings.m_componentLevelIndices),
     maxSparseIndexPlusOne(settings.m_componentWindowIndices),
     maxSparseIndexPlusOne(settings.m_componentThresholdLowIndices),
     maxSparseIndexPlusOne(settings.m_componentThresholdHighIndices),
     maxSparseIndexPlusOne(settings.m_componentVisibilityIndices),
     maxSparseIndexPlusOne(settings.m_componentOpacityIndices),
     maxSparseIndexPlusOne(settings.m_colorMapIndexIndices),
     maxSparseIndexPlusOne(settings.m_colorMapInvertedIndices),
     maxSparseIndexPlusOne(settings.m_colorMapContinuousIndices),
     maxSparseIndexPlusOne(settings.m_colorMapLevelIndices),
     maxSparseIndexPlusOne(settings.m_colorMapHsvModifierIndices),
     maxSparseIndexPlusOne(settings.m_interpolationModeIndices),
     maxSparseIndexPlusOne(settings.m_foregroundThresholdLowIndices),
     maxSparseIndexPlusOne(settings.m_foregroundThresholdHighIndices)});
}

json imageComponentSettingsToJson(const serialize::ImageSettings& settings, const std::size_t index)
{
  json value;

  if (
    const auto componentValue = sparseVectorValue(settings.m_componentLevels, settings.m_componentLevelIndices, index))
  {
    value["level"] = *componentValue;
  }
  else if (index == 0) {
    value["level"] = settings.m_level;
  }

  if (
    const auto componentValue =
      sparseVectorValue(settings.m_componentWindows, settings.m_componentWindowIndices, index))
  {
    value["window"] = *componentValue;
  }
  else if (index == 0) {
    value["window"] = settings.m_window;
  }

  if (
    const auto componentValue =
      sparseVectorValue(settings.m_componentThresholdLows, settings.m_componentThresholdLowIndices, index))
  {
    value["thresholdLow"] = *componentValue;
  }
  else if (index == 0) {
    value["thresholdLow"] = settings.m_thresholdLow;
  }

  if (
    const auto componentValue =
      sparseVectorValue(settings.m_componentThresholdHighs, settings.m_componentThresholdHighIndices, index))
  {
    value["thresholdHigh"] = *componentValue;
  }
  else if (index == 0) {
    value["thresholdHigh"] = settings.m_thresholdHigh;
  }

  if (
    const auto componentValue =
      sparseVectorValue(settings.m_componentVisibility, settings.m_componentVisibilityIndices, index))
  {
    value["visible"] = *componentValue;
  }

  if (
    const auto componentValue =
      sparseVectorValue(settings.m_componentOpacities, settings.m_componentOpacityIndices, index))
  {
    value["opacity"] = *componentValue;
  }
  else if (index == 0) {
    value["opacity"] = settings.m_opacity;
  }

  if (const auto componentValue = sparseVectorValue(settings.m_colorMapIndices, settings.m_colorMapIndexIndices, index))
  {
    value["colorMapIndex"] = *componentValue;
  }
  if (
    const auto componentValue =
      sparseVectorValue(settings.m_colorMapInverted, settings.m_colorMapInvertedIndices, index))
  {
    value["colorMapInverted"] = *componentValue;
  }
  if (
    const auto componentValue =
      sparseVectorValue(settings.m_colorMapContinuous, settings.m_colorMapContinuousIndices, index))
  {
    value["colorMapContinuous"] = *componentValue;
  }
  if (const auto componentValue = sparseVectorValue(settings.m_colorMapLevels, settings.m_colorMapLevelIndices, index))
  {
    value["colorMapLevels"] = *componentValue;
  }
  if (
    const auto componentValue =
      sparseVectorValue(settings.m_colorMapHsvModifiers, settings.m_colorMapHsvModifierIndices, index))
  {
    value["colorMapHsvModifier"] = vec3ToJson(*componentValue);
  }
  if (
    const auto componentValue =
      sparseVectorValue(settings.m_interpolationModes, settings.m_interpolationModeIndices, index))
  {
    value["interpolationMode"] = enumToName(*componentValue, k_interpolationModeNames);
  }
  if (
    const auto componentValue =
      sparseVectorValue(settings.m_foregroundThresholdLows, settings.m_foregroundThresholdLowIndices, index))
  {
    value["foregroundThresholdLow"] = *componentValue;
  }
  if (
    const auto componentValue =
      sparseVectorValue(settings.m_foregroundThresholdHighs, settings.m_foregroundThresholdHighIndices, index))
  {
    value["foregroundThresholdHigh"] = *componentValue;
  }

  return value;
}

json imageComponentDefaultsToJson()
{
  return json{
    {"level", 0.0},
    {"window", 1.0},
    {"thresholdLow", 0.0},
    {"thresholdHigh", 0.0},
    {"visible", true},
    {"opacity", 1.0},
    {"colorMapIndex", 0},
    {"colorMapInverted", false},
    {"colorMapContinuous", true},
    {"colorMapLevels", 8},
    {"colorMapHsvModifier", vec3ToJson(glm::vec3{0.0f, 1.0f, 1.0f})},
    {"interpolationMode", enumToName(InterpolationMode::Linear, k_interpolationModeNames)},
    {"foregroundThresholdLow", 0.0},
    {"foregroundThresholdHigh", 0.0}};
}

} // namespace

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
