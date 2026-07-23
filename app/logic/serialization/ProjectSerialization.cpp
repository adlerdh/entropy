#include "logic/serialization/ProjectSerialization.h"
#include "common/Exception.hpp"
#include "logic/annotation/SerializeAnnot.h"
#include "logic/serialization/JsonSerializers.h"
#include "layout/LayoutSpecJson.h"

#include <safeclib/strerrorlen_s.h>

#include <spdlog/fmt/std.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// #if defined(_LIBCPP_VERSION) && (_LIBCPP_VERSION >= 1000)
#if !defined(_MSC_VER)
#define HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR 1
#else
#define HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR 0
#endif

using json = nlohmann::json;

namespace fs = std::filesystem;

namespace
{
constexpr int k_projectFormatMajorVersion = 1;
constexpr int k_projectFormatMinorVersion = 0;

struct JsonVersion
{
  int major = 0;
  int minor = 0;
};

json versionToJson(const int major, const int minor)
{
  return json{{"major", major}, {"minor", minor}};
}

JsonVersion versionFromJson(const json& value)
{
  if (!value.is_object()) {
    throwDebug("JSON version must be an object");
  }

  return JsonVersion{.major = value.at("major").get<int>(), .minor = value.at("minor").get<int>()};
}

json surfaceVec3ToJson(const glm::vec3& value)
{
  return json::array({value.x, value.y, value.z});
}

std::optional<glm::vec3> surfaceVec3FromJson(const json& value)
{
  if (!value.is_array() || value.size() != 3) {
    return std::nullopt;
  }
  return glm::vec3{value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
}

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
} // namespace

void to_json(json& j, const SurfaceMaterial& material)
{
  const SurfaceMaterial defaults;
  j = json::object();
  addIfChanged(j, "ambient", material.ambient, defaults.ambient);
  addIfChanged(j, "diffuse", material.diffuse, defaults.diffuse);
  addIfChanged(j, "specular", material.specular, defaults.specular);
  addIfChanged(j, "shininess", material.shininess, defaults.shininess);
}

void from_json(const json& j, SurfaceMaterial& material)
{
  if (const auto value = j.find("ambient"); value != j.end() && value->is_number()) {
    material.ambient = value->get<float>();
  }
  if (const auto value = j.find("diffuse"); value != j.end() && value->is_number()) {
    material.diffuse = value->get<float>();
  }
  if (const auto value = j.find("specular"); value != j.end() && value->is_number()) {
    material.specular = value->get<float>();
  }
  if (const auto value = j.find("shininess"); value != j.end() && value->is_number()) {
    material.shininess = value->get<float>();
  }
}

void to_json(json& j, const Isosurface& surface)
{
  const Isosurface defaults;
  j = json::object();
  addIfChanged(j, "name", surface.name, defaults.name);
  addIfChanged(j, "value", surface.value, defaults.value);
  addIfChanged(j, "color", surfaceVec3ToJson(surface.color), surfaceVec3ToJson(defaults.color));

  json material = surface.material;
  addIfNotEmpty(j, "material", std::move(material));

  addIfChanged(j, "opacity", surface.opacity, defaults.opacity);
  addIfChanged(j, "contourFillOpacity", surface.fillOpacity, defaults.fillOpacity);
  addIfChanged(j, "visible", surface.visible, defaults.visible);
  addIfChanged(j, "showContours2D", surface.showIn2d, defaults.showIn2d);

  json rimLighting = json::object();
  addIfChanged(rimLighting, "enabled", surface.rimLightingEnabled, defaults.rimLightingEnabled);
  addIfChanged(rimLighting, "opacity", surface.rimOpacityStrength, defaults.rimOpacityStrength);
  addIfChanged(rimLighting, "glow", surface.rimEmissionStrength, defaults.rimEmissionStrength);
  addIfChanged(rimLighting, "falloff", surface.rimPower, defaults.rimPower);
  addIfNotEmpty(j, "rimLighting", std::move(rimLighting));
}

void from_json(const json& j, Isosurface& surface)
{
  if (const auto value = j.find("name"); value != j.end() && value->is_string()) {
    surface.name = value->get<std::string>();
  }
  if (const auto value = j.find("value"); value != j.end() && value->is_number()) {
    surface.value = value->get<double>();
  }
  if (const auto color = j.find("color"); color != j.end()) {
    if (const auto parsed = surfaceVec3FromJson(*color)) {
      surface.color = *parsed;
    }
  }
  if (const auto value = j.find("material"); value != j.end() && value->is_object()) {
    surface.material = value->get<SurfaceMaterial>();
  }
  if (const auto value = j.find("opacity"); value != j.end() && value->is_number()) {
    surface.opacity = value->get<float>();
  }
  if (const auto value = j.find("contourFillOpacity"); value != j.end() && value->is_number()) {
    surface.fillOpacity = value->get<float>();
  }
  if (const auto value = j.find("visible"); value != j.end() && value->is_boolean()) {
    surface.visible = value->get<bool>();
  }
  if (const auto value = j.find("showContours2D"); value != j.end() && value->is_boolean()) {
    surface.showIn2d = value->get<bool>();
  }
  if (const auto rim = j.find("rimLighting"); rim != j.end() && rim->is_object()) {
    if (const auto value = rim->find("enabled"); value != rim->end() && value->is_boolean()) {
      surface.rimLightingEnabled = value->get<bool>();
    }
    if (const auto value = rim->find("opacity"); value != rim->end() && value->is_number()) {
      surface.rimOpacityStrength = value->get<float>();
    }
    if (const auto value = rim->find("glow"); value != rim->end() && value->is_number()) {
      surface.rimEmissionStrength = value->get<float>();
    }
    if (const auto value = rim->find("falloff"); value != rim->end() && value->is_number()) {
      surface.rimPower = value->get<float>();
    }
  }
}

namespace
{

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

void applyToImagePaths(
  serialize::EntropyProject& project,
  const fs::path& projectBasePath,
  const std::function<void(serialize::Image& image, const fs::path& projectBasePath)>& func)
{
  func(project.m_referenceImage, projectBasePath);

  for (auto& image : project.m_additionalImages) {
    func(image, projectBasePath);
  }
}

#if !HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR
/**
 * @brief Use \c spdlog to log the errno.
 *
 * @note Several standard library functions indicate errors by writing positive integers to errno.
 * Typically, the value of errno is set to one of the error codes listed in <errno.h> as macro
 * constants beginning with the letter E followed by uppercase letters or digits.
 *
 * @see https://en.cppreference.com/w/c/error/errno
 */
void logStdErrno()
{
  const size_t errmsglen = safeclib::strerrorlen_s(errno) + 1;
  std::unique_ptr<char[]> errmsg(new char[errmsglen]);
  strerror_s(errmsg.get(), errmsglen, errno);

  spdlog::error("Error #{}: {}", errno, errmsg.get());
}
#endif

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

void makePathCanonicalAbsolute(std::optional<fs::path>& path, const fs::path& projectBasePath, const char* description)
{
  if (!path) {
    return;
  }
  if (path->empty()) {
    spdlog::warn("Ignoring empty {} path", description);
    path = std::nullopt;
    return;
  }

  fs::path absolutePath = path->is_absolute() ? *path : (projectBasePath / *path).lexically_normal();
  std::error_code error;
  if (fs::exists(absolutePath, error)) {
    absolutePath = fs::canonical(absolutePath);
  }
  else {
    spdlog::warn("Referenced {} {} does not exist", description, absolutePath);
  }
  *path = absolutePath;
}

void makePathCanonicalAbsolute(fs::path& path, const fs::path& projectBasePath, const char* description)
{
  std::optional<fs::path> optionalPath = path;
  makePathCanonicalAbsolute(optionalPath, projectBasePath, description);
  path = optionalPath.value_or(fs::path{});
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

namespace serialize
{

void to_json(json& j, const serialize::ImageSettings& settings)
{
  const serialize::ImageSettings defaults;

  json componentValues = json::array();
  const std::size_t componentCount = imageComponentSettingsCount(settings);
  for (std::size_t i = 0; i < componentCount; ++i) {
    json component = imageComponentSettingsToJson(settings, i);
    removeDefaultEntries(component, imageComponentDefaultsToJson());
    if (!component.empty()) {
      component["index"] = i;
      componentValues.push_back(std::move(component));
    }
  }

  j = json::object();

  json display = json::object();
  addIfChanged(display, "name", settings.m_displayName, defaults.m_displayName);
  addIfChanged(display, "visible", settings.m_globalVisibility, defaults.m_globalVisibility);
  addIfChanged(display, "opacity", settings.m_globalOpacity, defaults.m_globalOpacity);
  if (settings.m_hasBorderColor) {
    display["borderColor"] = vec3ToJson(settings.m_borderColor);
  }
  addIfNotEmpty(j, "display", std::move(display));

  json transform = json::object();
  addIfChanged(transform, "lockedToReference", settings.m_lockedToReference, defaults.m_lockedToReference);
  addIfChanged(transform, "warpEnabled", settings.m_warpEnabled, defaults.m_warpEnabled);
  addIfChanged(transform, "warpStrength", settings.m_warpStrength, defaults.m_warpStrength);
  addIfChanged(transform, "allowExaggeratedWarp", settings.m_allowExaggeratedWarp, defaults.m_allowExaggeratedWarp);
  addIfNotEmpty(j, "transform", std::move(transform));

  json time = json::object();
  addIfChanged(time, "activePoint", settings.m_activeTimePoint, defaults.m_activeTimePoint);
  addIfChanged(time, "playbackLoop", settings.m_timePlaybackLoop, defaults.m_timePlaybackLoop);
  addIfChanged(time, "playbackPlaying", settings.m_timePlaybackPlaying, defaults.m_timePlaybackPlaying);
  addIfChanged(time, "playbackSpeed", settings.m_timePlaybackSpeed, defaults.m_timePlaybackSpeed);
  addIfNotEmpty(j, "time", std::move(time));

  json components = json::object();
  addIfChanged(components, "active", settings.m_activeComponent, defaults.m_activeComponent);
  if (settings.m_hasComponentRenderMode) {
    components["renderMode"] = enumToName(settings.m_componentRenderMode, k_componentRenderModeNames);
  }
  addIfChanged(
    components,
    "complexPhaseUnit",
    enumToName(settings.m_complexPhaseUnit, k_complexPhaseUnitNames),
    enumToName(defaults.m_complexPhaseUnit, k_complexPhaseUnitNames));
  addIfChanged(
    components,
    "complexPhaseRange",
    enumToName(settings.m_complexPhaseRange, k_complexPhaseRangeNames),
    enumToName(defaults.m_complexPhaseRange, k_complexPhaseRangeNames));
  addIfChanged(components, "ignoreAlpha", settings.m_ignoreAlpha, defaults.m_ignoreAlpha);
  addIfChanged(
    components,
    "colorInterpolationMode",
    enumToName(settings.m_colorInterpolationMode, k_interpolationModeNames),
    enumToName(defaults.m_colorInterpolationMode, k_interpolationModeNames));
  addIfNotEmpty(components, "values", std::move(componentValues));
  addIfNotEmpty(j, "components", std::move(components));

  json vectorRendering = json::object();
  addIfChanged(
    vectorRendering,
    "planarProjectionSignedColors",
    settings.m_vectorPlanarProjectionSignedColors,
    defaults.m_vectorPlanarProjectionSignedColors);
  addIfChanged(
    vectorRendering,
    "logJacobianDeterminant",
    settings.m_vectorLogJacobianDeterminant,
    defaults.m_vectorLogJacobianDeterminant);
  addIfNotEmpty(j, "vectorRendering", std::move(vectorRendering));

  json vectorArrows = json::object();
  addIfChanged(vectorArrows, "visible", settings.m_vectorArrowOverlayVisible, defaults.m_vectorArrowOverlayVisible);
  addIfChanged(vectorArrows, "onImage", settings.m_vectorArrowOverlayOnImage, defaults.m_vectorArrowOverlayOnImage);
  addIfChanged(vectorArrows, "density", settings.m_vectorArrowOverlayDensity, defaults.m_vectorArrowOverlayDensity);
  addIfChanged(
    vectorArrows,
    "voxelSpacing",
    settings.m_vectorArrowOverlayVoxelSpacing,
    defaults.m_vectorArrowOverlayVoxelSpacing);
  addIfChanged(
    vectorArrows,
    "millimeterSpacing",
    settings.m_vectorArrowOverlayMillimeterSpacing,
    defaults.m_vectorArrowOverlayMillimeterSpacing);
  addIfChanged(
    vectorArrows,
    "spacingMode",
    enumToName(settings.m_vectorArrowOverlaySpacingMode, k_vectorArrowOverlaySpacingModeNames),
    enumToName(defaults.m_vectorArrowOverlaySpacingMode, k_vectorArrowOverlaySpacingModeNames));
  addIfChanged(
    vectorArrows,
    "color",
    vec3ToJson(settings.m_vectorArrowOverlayColor),
    vec3ToJson(defaults.m_vectorArrowOverlayColor));
  addIfChanged(
    vectorArrows,
    "useDirectionColor",
    settings.m_vectorArrowOverlayUseDirectionColor,
    defaults.m_vectorArrowOverlayUseDirectionColor);
  addIfChanged(
    vectorArrows,
    "lineThickness",
    settings.m_vectorArrowOverlayLineThickness,
    defaults.m_vectorArrowOverlayLineThickness);
  addIfChanged(vectorArrows, "opacity", settings.m_vectorArrowOverlayOpacity, defaults.m_vectorArrowOverlayOpacity);
  addIfChanged(
    vectorArrows,
    "scaleByMagnitude",
    settings.m_vectorArrowOverlayScaleByMagnitude,
    defaults.m_vectorArrowOverlayScaleByMagnitude);
  addIfChanged(
    vectorArrows,
    "scaleFactor",
    settings.m_vectorArrowOverlayScaleFactor,
    defaults.m_vectorArrowOverlayScaleFactor);
  addIfNotEmpty(j, "vectorArrows", std::move(vectorArrows));

  json warpedGrid = json::object();
  addIfChanged(warpedGrid, "visible", settings.m_vectorWarpedGridVisible, defaults.m_vectorWarpedGridVisible);
  addIfChanged(
    warpedGrid,
    "onImage",
    settings.m_vectorWarpedGridOverlayOnImage,
    defaults.m_vectorWarpedGridOverlayOnImage);
  addIfChanged(
    warpedGrid,
    "convention",
    enumToName(settings.m_vectorWarpedGridConvention, k_vectorWarpedGridConventionNames),
    enumToName(defaults.m_vectorWarpedGridConvention, k_vectorWarpedGridConventionNames));
  addIfChanged(
    warpedGrid,
    "pixelSpacing",
    settings.m_vectorWarpedGridPixelSpacing,
    defaults.m_vectorWarpedGridPixelSpacing);
  addIfChanged(
    warpedGrid,
    "voxelSpacing",
    settings.m_vectorWarpedGridVoxelSpacing,
    defaults.m_vectorWarpedGridVoxelSpacing);
  addIfChanged(
    warpedGrid,
    "millimeterSpacing",
    settings.m_vectorWarpedGridMillimeterSpacing,
    defaults.m_vectorWarpedGridMillimeterSpacing);
  addIfChanged(
    warpedGrid,
    "spacingMode",
    enumToName(settings.m_vectorWarpedGridSpacingMode, k_vectorArrowOverlaySpacingModeNames),
    enumToName(defaults.m_vectorWarpedGridSpacingMode, k_vectorArrowOverlaySpacingModeNames));
  addIfChanged(
    warpedGrid,
    "lineThickness",
    settings.m_vectorWarpedGridLineThickness,
    defaults.m_vectorWarpedGridLineThickness);
  addIfChanged(
    warpedGrid,
    "scaleFactor",
    settings.m_vectorWarpedGridScaleFactor,
    defaults.m_vectorWarpedGridScaleFactor);
  addIfChanged(
    warpedGrid,
    "foregroundColor",
    vec4ToJson(settings.m_vectorWarpedGridForegroundColor),
    vec4ToJson(defaults.m_vectorWarpedGridForegroundColor));
  addIfChanged(
    warpedGrid,
    "backgroundColor",
    vec4ToJson(settings.m_vectorWarpedGridBackgroundColor),
    vec4ToJson(defaults.m_vectorWarpedGridBackgroundColor));
  addIfNotEmpty(j, "warpedGrid", std::move(warpedGrid));

  json edges = json::object();
  addIfChanged(
    edges,
    "method",
    enumToName(settings.m_edgeDetectionMethod, k_edgeDetectionMethodNames),
    enumToName(defaults.m_edgeDetectionMethod, k_edgeDetectionMethodNames));
  addIfChanged(edges, "visible", settings.m_showEdges, defaults.m_showEdges);
  addIfChanged(edges, "hardEdges", settings.m_thresholdEdges, defaults.m_thresholdEdges);
  addIfChanged(edges, "thinPixelEdges", settings.m_thinPixelEdges, defaults.m_thinPixelEdges);
  addIfChanged(edges, "overlay", settings.m_overlayEdges, defaults.m_overlayEdges);
  addIfChanged(edges, "magnitude", settings.m_edgeMagnitude, defaults.m_edgeMagnitude);
  addIfChanged(edges, "pixelScale", settings.m_pixelEdgeScale, defaults.m_pixelEdgeScale);
  addIfChanged(edges, "pixelThreshold", settings.m_pixelEdgeThreshold, defaults.m_pixelEdgeThreshold);
  if (settings.m_hasEdgeColor) {
    edges["color"] = vec3ToJson(settings.m_edgeColor);
  }
  addIfChanged(edges, "opacity", settings.m_edgeOpacity, defaults.m_edgeOpacity);

  if (
    serialize::ProjectEdgeDetectionMethod::Voxel == settings.m_edgeDetectionMethod &&
    settings.m_colormapEdges != defaults.m_colormapEdges)
  {
    edges["useColormap"] = settings.m_colormapEdges;
  }
  addIfNotEmpty(j, "edges", std::move(edges));

  json raycasting = json::object();
  addIfChanged(
    raycasting,
    "useDistanceMap",
    settings.m_useDistanceMapForRaycasting,
    defaults.m_useDistanceMapForRaycasting);
  addIfNotEmpty(j, "raycasting", std::move(raycasting));

  json isosurfaces = json::object();
  addIfChanged(isosurfaces, "visible", settings.m_isosurfacesVisible, defaults.m_isosurfacesVisible);
  addIfChanged(
    isosurfaces,
    "applyImageColormap",
    settings.m_applyImageColormapToIsosurfaces,
    defaults.m_applyImageColormapToIsosurfaces);
  addIfChanged(isosurfaces, "showContours2D", settings.m_showIsocontoursIn2D, defaults.m_showIsocontoursIn2D);
  addIfChanged(
    isosurfaces,
    "contourLineWidth2D",
    settings.m_isocontourLineWidthIn2D,
    defaults.m_isocontourLineWidthIn2D);
  addIfChanged(
    isosurfaces,
    "opacityModulator",
    settings.m_isosurfaceOpacityModulator,
    defaults.m_isosurfaceOpacityModulator);
  addIfNotEmpty(j, "isosurfaces", std::move(isosurfaces));
}

void from_json(const json& j, serialize::ImageSettings& settings)
{
  if (const auto display = j.find("display"); display != j.end() && display->is_object()) {
    if (const auto name = display->find("name"); name != display->end() && name->is_string()) {
      settings.m_displayName = name->get<std::string>();
    }
    if (const auto visible = display->find("visible"); visible != display->end() && visible->is_boolean()) {
      settings.m_globalVisibility = visible->get<bool>();
    }
    if (const auto opacity = display->find("opacity"); opacity != display->end() && opacity->is_number()) {
      settings.m_globalOpacity = opacity->get<double>();
    }
    if (const auto color = display->find("borderColor"); color != display->end() && color->is_array()) {
      settings.m_borderColor = vec3FromJson(*color);
      settings.m_hasBorderColor = true;
    }
  }

  if (const auto transform = j.find("transform"); transform != j.end() && transform->is_object()) {
    if (const auto locked = transform->find("lockedToReference"); locked != transform->end() && locked->is_boolean()) {
      settings.m_lockedToReference = locked->get<bool>();
    }
    if (const auto enabled = transform->find("warpEnabled"); enabled != transform->end() && enabled->is_boolean()) {
      settings.m_warpEnabled = enabled->get<bool>();
    }
    if (const auto strength = transform->find("warpStrength"); strength != transform->end() && strength->is_number()) {
      settings.m_warpStrength = strength->get<float>();
    }
    if (const auto exaggerated = transform->find("allowExaggeratedWarp");
        exaggerated != transform->end() && exaggerated->is_boolean())
    {
      settings.m_allowExaggeratedWarp = exaggerated->get<bool>();
    }
  }

  if (const auto time = j.find("time"); time != j.end() && time->is_object()) {
    if (const auto active = unsignedIntFromJson(time->value("activePoint", json{}))) {
      settings.m_activeTimePoint = *active;
    }
    if (const auto loop = time->find("playbackLoop"); loop != time->end() && loop->is_boolean()) {
      settings.m_timePlaybackLoop = loop->get<bool>();
    }
    if (const auto playing = time->find("playbackPlaying"); playing != time->end() && playing->is_boolean()) {
      settings.m_timePlaybackPlaying = playing->get<bool>();
    }
    if (const auto speed = time->find("playbackSpeed"); speed != time->end() && speed->is_number()) {
      settings.m_timePlaybackSpeed = speed->get<double>();
    }
  }

  if (const auto components = j.find("components"); components != j.end() && components->is_object()) {
    if (const auto active = unsignedIntFromJson(components->value("active", json{}))) {
      settings.m_activeComponent = *active;
    }
    if (
      const auto parsed = enumFromName<serialize::ProjectComponentRenderMode>(
        components->value("renderMode", ""),
        k_componentRenderModeNames))
    {
      settings.m_componentRenderMode = *parsed;
      settings.m_hasComponentRenderMode = true;
    }
    if (
      const auto parsed = enumFromName<serialize::ProjectComplexPhaseUnit>(
        components->value("complexPhaseUnit", ""),
        k_complexPhaseUnitNames))
    {
      settings.m_complexPhaseUnit = *parsed;
    }
    if (
      const auto parsed = enumFromName<serialize::ProjectComplexPhaseRange>(
        components->value("complexPhaseRange", ""),
        k_complexPhaseRangeNames))
    {
      settings.m_complexPhaseRange = *parsed;
    }
    if (const auto ignoreAlpha = components->find("ignoreAlpha");
        ignoreAlpha != components->end() && ignoreAlpha->is_boolean())
    {
      settings.m_ignoreAlpha = ignoreAlpha->get<bool>();
    }
    if (
      const auto parsed =
        enumFromName<InterpolationMode>(components->value("colorInterpolationMode", ""), k_interpolationModeNames))
    {
      settings.m_colorInterpolationMode = *parsed;
    }

    if (const auto values = components->find("values"); values != components->end() && values->is_array()) {
      for (std::size_t componentIndex = 0; componentIndex < values->size(); ++componentIndex) {
        const json& value = values->at(componentIndex);
        if (!value.is_object()) {
          continue;
        }
        const std::size_t storedIndex = unsignedIntFromJson(value.value("index", json{})).value_or(componentIndex);
        if (const auto componentValue = value.find("level");
            componentValue != value.end() && componentValue->is_number())
        {
          setSparseVectorValue(
            settings.m_componentLevels,
            settings.m_componentLevelIndices,
            storedIndex,
            componentValue->get<double>(),
            settings.m_level);
        }
        if (const auto componentValue = value.find("window");
            componentValue != value.end() && componentValue->is_number())
        {
          setSparseVectorValue(
            settings.m_componentWindows,
            settings.m_componentWindowIndices,
            storedIndex,
            componentValue->get<double>(),
            settings.m_window);
        }
        if (const auto componentValue = value.find("thresholdLow");
            componentValue != value.end() && componentValue->is_number())
        {
          setSparseVectorValue(
            settings.m_componentThresholdLows,
            settings.m_componentThresholdLowIndices,
            storedIndex,
            componentValue->get<double>(),
            settings.m_thresholdLow);
        }
        if (const auto componentValue = value.find("thresholdHigh");
            componentValue != value.end() && componentValue->is_number())
        {
          setSparseVectorValue(
            settings.m_componentThresholdHighs,
            settings.m_componentThresholdHighIndices,
            storedIndex,
            componentValue->get<double>(),
            settings.m_thresholdHigh);
        }
        if (const auto componentValue = value.find("visible");
            componentValue != value.end() && componentValue->is_boolean())
        {
          setSparseVectorValue(
            settings.m_componentVisibility,
            settings.m_componentVisibilityIndices,
            storedIndex,
            componentValue->get<bool>(),
            true);
        }
        if (const auto componentValue = value.find("opacity");
            componentValue != value.end() && componentValue->is_number())
        {
          setSparseVectorValue(
            settings.m_componentOpacities,
            settings.m_componentOpacityIndices,
            storedIndex,
            componentValue->get<double>(),
            settings.m_opacity);
        }
        if (const auto componentValue = unsignedIntFromJson(value.value("colorMapIndex", json{}))) {
          setSparseVectorValue(
            settings.m_colorMapIndices,
            settings.m_colorMapIndexIndices,
            storedIndex,
            static_cast<std::size_t>(*componentValue),
            std::size_t{0});
        }
        if (const auto componentValue = value.find("colorMapInverted");
            componentValue != value.end() && componentValue->is_boolean())
        {
          setSparseVectorValue(
            settings.m_colorMapInverted,
            settings.m_colorMapInvertedIndices,
            storedIndex,
            componentValue->get<bool>(),
            false);
        }
        if (const auto componentValue = value.find("colorMapContinuous");
            componentValue != value.end() && componentValue->is_boolean())
        {
          setSparseVectorValue(
            settings.m_colorMapContinuous,
            settings.m_colorMapContinuousIndices,
            storedIndex,
            componentValue->get<bool>(),
            true);
        }
        if (const auto componentValue = unsignedIntFromJson(value.value("colorMapLevels", json{}))) {
          setSparseVectorValue(
            settings.m_colorMapLevels,
            settings.m_colorMapLevelIndices,
            storedIndex,
            static_cast<std::size_t>(*componentValue),
            std::size_t{8});
        }
        if (const auto componentValue = value.find("colorMapHsvModifier");
            componentValue != value.end() && componentValue->is_array())
        {
          setSparseVectorValue(
            settings.m_colorMapHsvModifiers,
            settings.m_colorMapHsvModifierIndices,
            storedIndex,
            vec3FromJson(*componentValue),
            glm::vec3{0.0f, 1.0f, 1.0f});
        }
        if (const auto componentValue = value.find("interpolationMode");
            componentValue != value.end() && componentValue->is_string())
        {
          if (
            const auto parsed =
              enumFromName<InterpolationMode>(componentValue->get<std::string>(), k_interpolationModeNames))
          {
            setSparseVectorValue(
              settings.m_interpolationModes,
              settings.m_interpolationModeIndices,
              storedIndex,
              *parsed,
              InterpolationMode::Linear);
          }
        }
        if (const auto componentValue = value.find("foregroundThresholdLow");
            componentValue != value.end() && componentValue->is_number())
        {
          setSparseVectorValue(
            settings.m_foregroundThresholdLows,
            settings.m_foregroundThresholdLowIndices,
            storedIndex,
            componentValue->get<double>(),
            0.0);
        }
        if (const auto componentValue = value.find("foregroundThresholdHigh");
            componentValue != value.end() && componentValue->is_number())
        {
          setSparseVectorValue(
            settings.m_foregroundThresholdHighs,
            settings.m_foregroundThresholdHighIndices,
            storedIndex,
            componentValue->get<double>(),
            0.0);
        }
      }

      const std::size_t activeComponent = settings.m_activeComponent;
      if (
        const auto componentValue =
          sparseVectorValue(settings.m_componentLevels, settings.m_componentLevelIndices, activeComponent))
      {
        settings.m_level = *componentValue;
      }
      if (
        const auto componentValue =
          sparseVectorValue(settings.m_componentWindows, settings.m_componentWindowIndices, activeComponent))
      {
        settings.m_window = *componentValue;
      }
      if (
        const auto componentValue = sparseVectorValue(
          settings.m_componentThresholdLows,
          settings.m_componentThresholdLowIndices,
          activeComponent))
      {
        settings.m_thresholdLow = *componentValue;
      }
      if (
        const auto componentValue = sparseVectorValue(
          settings.m_componentThresholdHighs,
          settings.m_componentThresholdHighIndices,
          activeComponent))
      {
        settings.m_thresholdHigh = *componentValue;
      }
      if (
        const auto componentValue =
          sparseVectorValue(settings.m_componentOpacities, settings.m_componentOpacityIndices, activeComponent))
      {
        settings.m_opacity = *componentValue;
      }
    }
  }

  if (const auto vectorRendering = j.find("vectorRendering");
      vectorRendering != j.end() && vectorRendering->is_object())
  {
    if (const auto signedColors = vectorRendering->find("planarProjectionSignedColors");
        signedColors != vectorRendering->end() && signedColors->is_boolean())
    {
      settings.m_vectorPlanarProjectionSignedColors = signedColors->get<bool>();
    }
    if (const auto logJacobian = vectorRendering->find("logJacobianDeterminant");
        logJacobian != vectorRendering->end() && logJacobian->is_boolean())
    {
      settings.m_vectorLogJacobianDeterminant = logJacobian->get<bool>();
    }
  }

  if (const auto arrows = j.find("vectorArrows"); arrows != j.end() && arrows->is_object()) {
    if (const auto visible = arrows->find("visible"); visible != arrows->end() && visible->is_boolean()) {
      settings.m_vectorArrowOverlayVisible = visible->get<bool>();
    }
    if (const auto onImage = arrows->find("onImage"); onImage != arrows->end() && onImage->is_boolean()) {
      settings.m_vectorArrowOverlayOnImage = onImage->get<bool>();
    }
    if (const auto density = arrows->find("density"); density != arrows->end() && density->is_number()) {
      settings.m_vectorArrowOverlayDensity = density->get<float>();
    }
    if (const auto spacing = arrows->find("voxelSpacing"); spacing != arrows->end() && spacing->is_number()) {
      settings.m_vectorArrowOverlayVoxelSpacing = spacing->get<float>();
    }
    if (const auto spacing = arrows->find("millimeterSpacing"); spacing != arrows->end() && spacing->is_number()) {
      settings.m_vectorArrowOverlayMillimeterSpacing = spacing->get<float>();
    }
    if (
      const auto parsed = enumFromName<serialize::ProjectVectorArrowOverlaySpacingMode>(
        arrows->value("spacingMode", ""),
        k_vectorArrowOverlaySpacingModeNames))
    {
      settings.m_vectorArrowOverlaySpacingMode = *parsed;
    }
    if (const auto color = arrows->find("color"); color != arrows->end() && color->is_array()) {
      settings.m_vectorArrowOverlayColor = vec3FromJson(*color);
    }
    if (const auto directionColor = arrows->find("useDirectionColor");
        directionColor != arrows->end() && directionColor->is_boolean())
    {
      settings.m_vectorArrowOverlayUseDirectionColor = directionColor->get<bool>();
    }
    if (const auto thickness = arrows->find("lineThickness"); thickness != arrows->end() && thickness->is_number()) {
      settings.m_vectorArrowOverlayLineThickness = thickness->get<float>();
    }
    if (const auto opacity = arrows->find("opacity"); opacity != arrows->end() && opacity->is_number()) {
      settings.m_vectorArrowOverlayOpacity = opacity->get<float>();
    }
    if (const auto scale = arrows->find("scaleByMagnitude"); scale != arrows->end() && scale->is_boolean()) {
      settings.m_vectorArrowOverlayScaleByMagnitude = scale->get<bool>();
    }
    if (const auto scale = arrows->find("scaleFactor"); scale != arrows->end() && scale->is_number()) {
      settings.m_vectorArrowOverlayScaleFactor = scale->get<float>();
    }
  }

  if (const auto grid = j.find("warpedGrid"); grid != j.end() && grid->is_object()) {
    if (const auto visible = grid->find("visible"); visible != grid->end() && visible->is_boolean()) {
      settings.m_vectorWarpedGridVisible = visible->get<bool>();
    }
    if (const auto onImage = grid->find("onImage"); onImage != grid->end() && onImage->is_boolean()) {
      settings.m_vectorWarpedGridOverlayOnImage = onImage->get<bool>();
    }
    if (
      const auto parsed = enumFromName<serialize::ProjectVectorWarpedGridConvention>(
        grid->value("convention", ""),
        k_vectorWarpedGridConventionNames))
    {
      settings.m_vectorWarpedGridConvention = *parsed;
    }
    if (const auto spacing = grid->find("pixelSpacing"); spacing != grid->end() && spacing->is_number()) {
      settings.m_vectorWarpedGridPixelSpacing = spacing->get<float>();
    }
    if (const auto spacing = grid->find("voxelSpacing"); spacing != grid->end() && spacing->is_number()) {
      settings.m_vectorWarpedGridVoxelSpacing = spacing->get<float>();
    }
    if (const auto spacing = grid->find("millimeterSpacing"); spacing != grid->end() && spacing->is_number()) {
      settings.m_vectorWarpedGridMillimeterSpacing = spacing->get<float>();
    }
    if (
      const auto parsed = enumFromName<serialize::ProjectVectorArrowOverlaySpacingMode>(
        grid->value("spacingMode", ""),
        k_vectorArrowOverlaySpacingModeNames))
    {
      settings.m_vectorWarpedGridSpacingMode = *parsed;
    }
    if (const auto thickness = grid->find("lineThickness"); thickness != grid->end() && thickness->is_number()) {
      settings.m_vectorWarpedGridLineThickness = thickness->get<float>();
    }
    if (const auto scale = grid->find("scaleFactor"); scale != grid->end() && scale->is_number()) {
      settings.m_vectorWarpedGridScaleFactor = scale->get<float>();
    }
    if (const auto color = grid->find("foregroundColor"); color != grid->end() && color->is_array()) {
      settings.m_vectorWarpedGridForegroundColor = vec4FromJson(*color);
    }
    if (const auto color = grid->find("backgroundColor"); color != grid->end() && color->is_array()) {
      settings.m_vectorWarpedGridBackgroundColor = vec4FromJson(*color);
    }
  }

  if (const auto edges = j.find("edges"); edges != j.end() && edges->is_object()) {
    if (
      const auto parsed =
        enumFromName<serialize::ProjectEdgeDetectionMethod>(edges->value("method", ""), k_edgeDetectionMethodNames))
    {
      settings.m_edgeDetectionMethod = *parsed;
    }
    if (const auto visible = edges->find("visible"); visible != edges->end() && visible->is_boolean()) {
      settings.m_showEdges = visible->get<bool>();
    }
    if (const auto hard = edges->find("hardEdges"); hard != edges->end() && hard->is_boolean()) {
      settings.m_thresholdEdges = hard->get<bool>();
    }
    if (const auto thin = edges->find("thinPixelEdges"); thin != edges->end() && thin->is_boolean()) {
      settings.m_thinPixelEdges = thin->get<bool>();
    }
    if (const auto overlay = edges->find("overlay"); overlay != edges->end() && overlay->is_boolean()) {
      settings.m_overlayEdges = overlay->get<bool>();
    }
    if (serialize::ProjectEdgeDetectionMethod::Voxel == settings.m_edgeDetectionMethod) {
      if (const auto colormap = edges->find("useColormap"); colormap != edges->end() && colormap->is_boolean()) {
        settings.m_colormapEdges = colormap->get<bool>();
      }
    }
    if (const auto magnitude = edges->find("magnitude"); magnitude != edges->end() && magnitude->is_number()) {
      settings.m_edgeMagnitude = magnitude->get<double>();
    }
    if (const auto scale = edges->find("pixelScale"); scale != edges->end() && scale->is_number()) {
      settings.m_pixelEdgeScale = scale->get<double>();
    }
    if (const auto threshold = edges->find("pixelThreshold"); threshold != edges->end() && threshold->is_number()) {
      settings.m_pixelEdgeThreshold = threshold->get<double>();
    }
    if (const auto color = edges->find("color"); color != edges->end() && color->is_array()) {
      settings.m_edgeColor = vec3FromJson(*color);
      settings.m_hasEdgeColor = true;
    }
    if (const auto opacity = edges->find("opacity"); opacity != edges->end() && opacity->is_number()) {
      settings.m_edgeOpacity = opacity->get<double>();
    }
  }

  if (const auto raycasting = j.find("raycasting"); raycasting != j.end() && raycasting->is_object()) {
    if (const auto useDistanceMap = raycasting->find("useDistanceMap");
        useDistanceMap != raycasting->end() && useDistanceMap->is_boolean())
    {
      settings.m_useDistanceMapForRaycasting = useDistanceMap->get<bool>();
    }
  }

  if (const auto isosurfaces = j.find("isosurfaces"); isosurfaces != j.end() && isosurfaces->is_object()) {
    if (const auto visible = isosurfaces->find("visible"); visible != isosurfaces->end() && visible->is_boolean()) {
      settings.m_isosurfacesVisible = visible->get<bool>();
    }
    if (const auto colormap = isosurfaces->find("applyImageColormap");
        colormap != isosurfaces->end() && colormap->is_boolean())
    {
      settings.m_applyImageColormapToIsosurfaces = colormap->get<bool>();
    }
    if (const auto contours = isosurfaces->find("showContours2D");
        contours != isosurfaces->end() && contours->is_boolean())
    {
      settings.m_showIsocontoursIn2D = contours->get<bool>();
    }
    if (const auto lineWidth = isosurfaces->find("contourLineWidth2D");
        lineWidth != isosurfaces->end() && lineWidth->is_number())
    {
      settings.m_isocontourLineWidthIn2D = lineWidth->get<double>();
    }
    if (const auto opacity = isosurfaces->find("opacityModulator");
        opacity != isosurfaces->end() && opacity->is_number())
    {
      settings.m_isosurfaceOpacityModulator = opacity->get<float>();
    }
  }
}

void to_json(json& j, const serialize::SegSettings& settings)
{
  const serialize::SegSettings defaults;
  j = json::object();

  json display = json::object();
  addIfChanged(display, "name", settings.m_displayName, defaults.m_displayName);
  addIfChanged(display, "visible", settings.m_visible, defaults.m_visible);
  addIfChanged(display, "opacity", settings.m_opacity, defaults.m_opacity);
  addIfNotEmpty(j, "display", std::move(display));

  addIfChanged(j, "labelTableIndex", settings.m_labelTableIndex, defaults.m_labelTableIndex);
  addIfChanged(
    j,
    "interpolationMode",
    enumToName(settings.m_interpolationMode, k_interpolationModeNames),
    enumToName(defaults.m_interpolationMode, k_interpolationModeNames));
}

void from_json(const json& j, serialize::SegSettings& settings)
{
  if (const auto display = j.find("display"); display != j.end() && display->is_object()) {
    if (const auto name = display->find("name"); name != display->end() && name->is_string()) {
      settings.m_displayName = name->get<std::string>();
    }
    if (const auto visible = display->find("visible"); visible != display->end() && visible->is_boolean()) {
      settings.m_visible = visible->get<bool>();
    }
    if (const auto opacity = display->find("opacity"); opacity != display->end() && opacity->is_number()) {
      settings.m_opacity = opacity->get<double>();
    }
  }
  if (const auto index = unsignedIntFromJson(j.value("labelTableIndex", json{}))) {
    settings.m_labelTableIndex = *index;
  }
  if (const auto parsed = enumFromName<InterpolationMode>(j.value("interpolationMode", ""), k_interpolationModeNames)) {
    settings.m_interpolationMode = *parsed;
  }
}

void to_json(json& j, const serialize::Segmentation& seg)
{
  j = json{{"path", pathToString(seg.m_segFileName)}};

  if (seg.m_settings) {
    json settings = *seg.m_settings;
    addIfNotEmpty(j, "settings", std::move(settings));
  }
}

void from_json(const json& j, serialize::Segmentation& seg)
{
  std::string p;
  j.at("path").get_to(p);
  seg.m_segFileName = p;

  if (j.count("settings")) {
    seg.m_settings = j.at("settings").get<serialize::SegSettings>();
  }
}

void to_json(json& j, const serialize::LandmarkPoint& point)
{
  const serialize::LandmarkPoint defaults;
  j = json::object();
  addIfChanged(j, "position", vec3ToJson(point.m_position), vec3ToJson(defaults.m_position));
  if (!point.m_name.empty()) {
    j["name"] = point.m_name;
  }
}

void from_json(const json& j, serialize::LandmarkPoint& point)
{
  if (const auto index = unsignedIntFromJson(j.value("index", json{}))) {
    point.m_index = *index;
  }
  if (const auto position = j.find("position"); position != j.end() && position->is_array()) {
    point.m_position = vec3FromJson(*position);
  }
  if (const auto name = j.find("name"); name != j.end() && name->is_string()) {
    point.m_name = name->get<std::string>();
  }
}

void to_json(json& j, const serialize::LandmarkGroup& landmarks)
{
  const serialize::LandmarkGroup defaults;
  j = json::object();

  if (landmarks.m_csvFileName) {
    j["path"] = pathToString(*landmarks.m_csvFileName);
  }

  addIfChanged(
    j,
    "coordinateSpace",
    enumToName(landmarks.m_coordinateSpace, k_landmarkCoordinateSpaceNames),
    enumToName(defaults.m_coordinateSpace, k_landmarkCoordinateSpaceNames));

  if (!landmarks.m_points.empty()) {
    j["points"] = landmarks.m_points;
  }

  json display = json::object();
  addIfChanged(display, "name", landmarks.m_name, defaults.m_name);
  addIfChanged(display, "visible", landmarks.m_visible, defaults.m_visible);
  addIfChanged(display, "opacity", landmarks.m_opacity, defaults.m_opacity);
  addIfChanged(display, "color", vec3ToJson(landmarks.m_color), vec3ToJson(defaults.m_color));
  addIfChanged(display, "colorOverride", landmarks.m_colorOverride, defaults.m_colorOverride);
  addIfChanged(display, "glyphRadiusFactor", landmarks.m_glyphRadiusFactor, defaults.m_glyphRadiusFactor);

  json labels = json::object();
  addIfChanged(labels, "showIndices", landmarks.m_renderLandmarkIndices, defaults.m_renderLandmarkIndices);
  addIfChanged(labels, "showNames", landmarks.m_renderLandmarkNames, defaults.m_renderLandmarkNames);
  if (landmarks.m_textColor) {
    labels["textColor"] = vec3ToJson(*landmarks.m_textColor);
  }
  addIfNotEmpty(display, "labels", std::move(labels));
  addIfNotEmpty(j, "display", std::move(display));
}

void from_json(const json& j, serialize::LandmarkGroup& landmarks)
{
  if (const auto path = j.find("path"); path != j.end() && path->is_string()) {
    landmarks.m_csvFileName = path->get<std::string>();
  }

  if (
    const auto parsed = enumFromName<serialize::ProjectLandmarkCoordinateSpace>(
      j.value("coordinateSpace", ""),
      k_landmarkCoordinateSpaceNames))
  {
    landmarks.m_coordinateSpace = *parsed;
  }

  if (const auto points = j.find("points"); points != j.end()) {
    points->get_to(landmarks.m_points);
    for (std::size_t i = 0; i < landmarks.m_points.size(); ++i) {
      landmarks.m_points.at(i).m_index = i;
    }
  }

  if (const auto display = j.find("display"); display != j.end() && display->is_object()) {
    if (const auto name = display->find("name"); name != display->end() && name->is_string()) {
      landmarks.m_name = name->get<std::string>();
    }
    if (const auto visible = display->find("visible"); visible != display->end() && visible->is_boolean()) {
      landmarks.m_visible = visible->get<bool>();
    }
    if (const auto opacity = display->find("opacity"); opacity != display->end() && opacity->is_number()) {
      landmarks.m_opacity = opacity->get<float>();
    }
    if (const auto color = display->find("color"); color != display->end()) {
      landmarks.m_color = vec3FromJson(*color);
    }
    if (const auto overrideColor = display->find("colorOverride");
        overrideColor != display->end() && overrideColor->is_boolean())
    {
      landmarks.m_colorOverride = overrideColor->get<bool>();
    }
    if (const auto radiusFactor = display->find("glyphRadiusFactor");
        radiusFactor != display->end() && radiusFactor->is_number())
    {
      landmarks.m_glyphRadiusFactor = radiusFactor->get<float>();
    }

    if (const auto labels = display->find("labels"); labels != display->end() && labels->is_object()) {
      if (const auto textColor = labels->find("textColor"); textColor != labels->end() && !textColor->is_null()) {
        landmarks.m_textColor = vec3FromJson(*textColor);
      }
      if (const auto showIndices = labels->find("showIndices");
          showIndices != labels->end() && showIndices->is_boolean())
      {
        landmarks.m_renderLandmarkIndices = showIndices->get<bool>();
      }
      if (const auto showNames = labels->find("showNames"); showNames != labels->end() && showNames->is_boolean()) {
        landmarks.m_renderLandmarkNames = showNames->get<bool>();
      }
    }
  }
}

void to_json(json& j, const serialize::ImageIsosurface& isosurface)
{
  const serialize::ImageIsosurface defaults;
  j = json::object();
  addIfChanged(j, "component", isosurface.m_component, defaults.m_component);

  json surface = isosurface.m_surface;
  addIfNotEmpty(j, "surface", std::move(surface));
}

void from_json(const json& j, serialize::ImageIsosurface& isosurface)
{
  if (const auto component = unsignedIntFromJson(j.value("component", json{}))) {
    isosurface.m_component = *component;
  }
  if (const auto surface = j.find("surface"); surface != j.end() && surface->is_object()) {
    isosurface.m_surface = surface->get<Isosurface>();
  }
  else {
    isosurface.m_surface = j.get<Isosurface>();
  }
}

void to_json(json& j, const serialize::DicomSource& source)
{
  j = json::object();

  if (!source.m_rootPath.empty()) {
    j["rootPath"] = pathToString(source.m_rootPath);
  }

  if (!source.m_seriesInstanceUid.empty()) {
    j["seriesInstanceUid"] = source.m_seriesInstanceUid;
  }

  if (!source.m_studyInstanceUid.empty()) {
    j["studyInstanceUid"] = source.m_studyInstanceUid;
  }

  if (!source.m_files.empty()) {
    std::vector<std::string> paths;
    paths.reserve(source.m_files.size());
    for (const auto& file : source.m_files) {
      paths.push_back(pathToString(file));
    }
    j["paths"] = paths;
  }
}

void from_json(const json& j, serialize::DicomSource& source)
{
  if (j.count("rootPath")) {
    source.m_rootPath = j.at("rootPath").get<std::string>();
  }
  if (j.count("studyInstanceUid")) {
    source.m_studyInstanceUid = j.at("studyInstanceUid").get<std::string>();
  }
  if (j.count("seriesInstanceUid")) {
    source.m_seriesInstanceUid = j.at("seriesInstanceUid").get<std::string>();
  }
  if (j.count("paths")) {
    const auto paths = j.at("paths").get<std::vector<std::string>>();
    source.m_files.clear();
    source.m_files.reserve(paths.size());
    for (const auto& path : paths) {
      source.m_files.emplace_back(path);
    }
  }
}

void to_json(json& j, const serialize::Image& image)
{
  j = json{{"path", pathToString(image.m_imageFileName)}};

  if (image.m_spatialMetadata) {
    j["spatialMetadata"] = {
      {"spacing", vec3ToJson(image.m_spatialMetadata->spacingMm)},
      {"origin", vec3ToJson(image.m_spatialMetadata->originMm)},
      {"directions", mat3ToJson(image.m_spatialMetadata->directions)}};
  }

  if (image.m_dicomSource) {
    json dicomSource = *image.m_dicomSource;
    addIfNotEmpty(j, "dicomSource", std::move(dicomSource));
  }

  if (image.m_initialAffineFileName || image.m_initialAffineMatrix) {
    j["initialAffine"] = affineToJson(image.m_initialAffineFileName, image.m_initialAffineMatrix);
  }

  if (image.m_inverseWarpFieldPath) {
    j["inverseWarpField"] = pathObjectToJson(*image.m_inverseWarpFieldPath);
  }

  if (image.m_inverseWarpReferenceImagePath) {
    j["inverseWarpReferenceImage"] = pathObjectToJson(*image.m_inverseWarpReferenceImagePath);
  }

  if (image.m_forwardWarpFieldPath) {
    j["forwardWarpField"] = pathObjectToJson(*image.m_forwardWarpFieldPath);
  }

  if (image.m_manualAffineFileName || image.m_manualAffineMatrix) {
    j["manualAffine"] = affineToJson(image.m_manualAffineFileName, image.m_manualAffineMatrix);
  }

  if (image.m_annotationsFileName || !image.m_annotations.empty()) {
    json annotations = json::object();
    if (image.m_annotationsFileName) {
      annotations["path"] = pathToString(*image.m_annotationsFileName);
    }
    if (!image.m_annotations.empty()) {
      annotations["embedded"] = annotationsToJson(image.m_annotations);
    }
    j["annotations"] = std::move(annotations);
  }

  if (!image.m_segmentations.empty()) {
    j["segmentations"] = image.m_segmentations;
  }

  if (!image.m_landmarkGroups.empty()) {
    j["landmarks"] = image.m_landmarkGroups;
  }

  if (!image.m_isosurfaces.empty()) {
    j["isosurfaces"] = image.m_isosurfaces;
  }

  if (image.m_settings) {
    json settings = *image.m_settings;
    addIfNotEmpty(j, "settings", std::move(settings));
  }
}

void from_json(const json& j, serialize::Image& image)
{
  std::string p;
  j.at("path").get_to(p);
  image.m_imageFileName = p;

  if (const auto metadata = j.find("spatialMetadata"); metadata != j.end() && metadata->is_object()) {
    ImageSpatialMetadata spatialMetadata;
    spatialMetadata.spacingMm = vec3FromJson(metadata->at("spacing"));
    spatialMetadata.originMm = vec3FromJson(metadata->at("origin"));
    spatialMetadata.directions = mat3FromJson(metadata->at("directions"));
    image.m_spatialMetadata = spatialMetadata;
  }

  if (j.count("dicomSource")) {
    image.m_dicomSource = j.at("dicomSource").get<serialize::DicomSource>();
  }

  if (j.count("initialAffine")) {
    affineFromJson(j.at("initialAffine"), image.m_initialAffineFileName, image.m_initialAffineMatrix);
  }

  if (j.count("inverseWarpField")) {
    image.m_inverseWarpFieldPath = pathObjectFromJson(j.at("inverseWarpField"), "inverseWarpField");
  }

  if (j.count("inverseWarpReferenceImage")) {
    image.m_inverseWarpReferenceImagePath =
      pathObjectFromJson(j.at("inverseWarpReferenceImage"), "inverseWarpReferenceImage");
  }

  if (j.count("forwardWarpField")) {
    image.m_forwardWarpFieldPath = pathObjectFromJson(j.at("forwardWarpField"), "forwardWarpField");
  }

  if (j.count("manualAffine")) {
    affineFromJson(j.at("manualAffine"), image.m_manualAffineFileName, image.m_manualAffineMatrix);
  }

  if (const auto annotations = j.find("annotations"); annotations != j.end()) {
    if (!annotations->is_object()) {
      throwDebug("Image annotations entry must be an object");
    }
    if (const auto path = annotations->find("path"); path != annotations->end() && path->is_string()) {
      image.m_annotationsFileName = path->get<std::string>();
    }
    if (const auto embedded = annotations->find("embedded"); embedded != annotations->end()) {
      image.m_annotations = annotationsFromJson(*embedded);
    }
  }

  if (j.count("segmentations")) {
    j.at("segmentations").get_to(image.m_segmentations);
  }

  if (j.count("landmarks")) {
    j.at("landmarks").get_to(image.m_landmarkGroups);
  }

  if (j.count("isosurfaces")) {
    j.at("isosurfaces").get_to(image.m_isosurfaces);
  }

  if (j.count("settings")) {
    image.m_settings = j.at("settings").get<serialize::ImageSettings>();
  }
}

void to_json(json& j, const ProjectInterfaceSettings& settings)
{
  const ProjectInterfaceSettings defaults;
  j = json::object();
  addIfChanged(j, "synchronizeTimeSeries", settings.m_synchronizeTimeSeries, defaults.m_synchronizeTimeSeries);
}

void from_json(const json& j, ProjectInterfaceSettings& settings)
{
  if (const auto syncTime = j.find("synchronizeTimeSeries"); syncTime != j.end() && syncTime->is_boolean()) {
    settings.m_synchronizeTimeSeries = syncTime->get<bool>();
  }
}

void to_json(json& j, const ProjectViewSettings& settings)
{
  const ProjectViewSettings defaults;
  j = json::object();

  json imageBorders = json::object();
  addIfChanged(imageBorders, "visible", settings.m_showImageBorders, defaults.m_showImageBorders);
  addIfChanged(
    imageBorders,
    "visibleInLightboxes",
    settings.m_showImageBordersInLightboxViews,
    defaults.m_showImageBordersInLightboxViews);
  addIfNotEmpty(j, "imageBorders", std::move(imageBorders));

  json crosshairs = json::object();
  addIfChanged(crosshairs, "visible", settings.m_showCrosshairs, defaults.m_showCrosshairs);
  addIfChanged(
    crosshairs,
    "visibleInLightboxes",
    settings.m_showCrosshairsInLightboxViews,
    defaults.m_showCrosshairsInLightboxViews);
  addIfChanged(
    crosshairs,
    "snapping",
    enumToName(settings.m_crosshairsSnapping, k_crosshairsSnappingNames),
    enumToName(defaults.m_crosshairsSnapping, k_crosshairsSnappingNames));
  addIfNotEmpty(j, "crosshairs", std::move(crosshairs));

  json anatomicalLabels = json::object();
  addIfChanged(anatomicalLabels, "visible", settings.m_showAnatomicalLabels, defaults.m_showAnatomicalLabels);
  addIfChanged(
    anatomicalLabels,
    "visibleInLightboxes",
    settings.m_showAnatomicalLabelsInLightboxViews,
    defaults.m_showAnatomicalLabelsInLightboxViews);
  addIfChanged(
    anatomicalLabels,
    "type",
    enumToName(settings.m_anatomicalLabelType, k_anatomicalLabelNames),
    enumToName(defaults.m_anatomicalLabelType, k_anatomicalLabelNames));
  addIfChanged(
    anatomicalLabels,
    "lockDirectionsToReferenceImage",
    settings.m_lockAnatomicalDirectionsToReferenceImage,
    defaults.m_lockAnatomicalDirectionsToReferenceImage);
  addIfNotEmpty(j, "anatomicalLabels", std::move(anatomicalLabels));

  json scaleBars = json::object();
  addIfChanged(scaleBars, "visible", settings.m_showScaleBars, defaults.m_showScaleBars);
  addIfChanged(
    scaleBars,
    "visibleInLightboxes",
    settings.m_showScaleBarsInLightboxViews,
    defaults.m_showScaleBarsInLightboxViews);
  addIfNotEmpty(j, "scaleBars", std::move(scaleBars));
}

void from_json(const json& j, ProjectViewSettings& settings)
{
  if (const auto imageBorders = j.find("imageBorders"); imageBorders != j.end() && imageBorders->is_object()) {
    if (const auto value = imageBorders->find("visible"); value != imageBorders->end() && value->is_boolean()) {
      settings.m_showImageBorders = value->get<bool>();
    }
    if (const auto value = imageBorders->find("visibleInLightboxes");
        value != imageBorders->end() && value->is_boolean())
    {
      settings.m_showImageBordersInLightboxViews = value->get<bool>();
    }
  }
  if (const auto crosshairs = j.find("crosshairs"); crosshairs != j.end() && crosshairs->is_object()) {
    if (const auto value = crosshairs->find("visible"); value != crosshairs->end() && value->is_boolean()) {
      settings.m_showCrosshairs = value->get<bool>();
    }
    if (const auto value = crosshairs->find("visibleInLightboxes"); value != crosshairs->end() && value->is_boolean()) {
      settings.m_showCrosshairsInLightboxViews = value->get<bool>();
    }
    if (
      const auto parsed =
        enumFromName<CrosshairsSnapping>(crosshairs->value("snapping", ""), k_crosshairsSnappingNames))
    {
      settings.m_crosshairsSnapping = *parsed;
    }
  }
  if (const auto anatomicalLabels = j.find("anatomicalLabels");
      anatomicalLabels != j.end() && anatomicalLabels->is_object())
  {
    if (const auto value = anatomicalLabels->find("visible"); value != anatomicalLabels->end() && value->is_boolean()) {
      settings.m_showAnatomicalLabels = value->get<bool>();
    }
    if (const auto value = anatomicalLabels->find("visibleInLightboxes");
        value != anatomicalLabels->end() && value->is_boolean())
    {
      settings.m_showAnatomicalLabelsInLightboxViews = value->get<bool>();
    }
    if (
      const auto parsed =
        enumFromName<AnatomicalLabelType>(anatomicalLabels->value("type", ""), k_anatomicalLabelNames))
    {
      settings.m_anatomicalLabelType = *parsed;
      if (AnatomicalLabelType::Disabled == settings.m_anatomicalLabelType) {
        settings.m_showAnatomicalLabels = false;
        settings.m_anatomicalLabelType = AnatomicalLabelType::Human;
      }
    }
    if (const auto value = anatomicalLabels->find("lockDirectionsToReferenceImage");
        value != anatomicalLabels->end() && value->is_boolean())
    {
      settings.m_lockAnatomicalDirectionsToReferenceImage = value->get<bool>();
    }
  }
  if (const auto scaleBars = j.find("scaleBars"); scaleBars != j.end() && scaleBars->is_object()) {
    if (const auto value = scaleBars->find("visible"); value != scaleBars->end() && value->is_boolean()) {
      settings.m_showScaleBars = value->get<bool>();
    }
    if (const auto value = scaleBars->find("visibleInLightboxes"); value != scaleBars->end() && value->is_boolean()) {
      settings.m_showScaleBarsInLightboxViews = value->get<bool>();
    }
  }

  if (!settings.m_showImageBorders) {
    settings.m_showImageBordersInLightboxViews = false;
  }
  if (!settings.m_showCrosshairs) {
    settings.m_showCrosshairsInLightboxViews = false;
  }
  if (!settings.m_showAnatomicalLabels) {
    settings.m_showAnatomicalLabelsInLightboxViews = false;
  }
  if (!settings.m_showScaleBars) {
    settings.m_showScaleBarsInLightboxViews = false;
  }
}

void to_json(json& j, const ProjectMetricSettings& settings)
{
  const ProjectMetricSettings defaults;
  j = json::object();
  addIfChanged(j, "colormapIndex", settings.m_colorMapIndex, defaults.m_colorMapIndex);
  addIfChanged(j, "windowSlopeIntercept", vec2ToJson(settings.m_slopeIntercept), vec2ToJson(defaults.m_slopeIntercept));
  addIfChanged(j, "invertColormap", settings.m_invertColormap, defaults.m_invertColormap);
  addIfChanged(j, "continuousColormap", settings.m_continuousColormap, defaults.m_continuousColormap);
  addIfChanged(j, "colormapLevels", settings.m_colormapLevels, defaults.m_colormapLevels);
}

void from_json(const json& j, ProjectMetricSettings& settings)
{
  if (const auto value = j.find("colormapIndex"); value != j.end() && value->is_number_unsigned()) {
    settings.m_colorMapIndex = value->get<std::size_t>();
  }
  if (const auto value = j.find("windowSlopeIntercept"); value != j.end() && value->is_array() && value->size() == 2) {
    settings.m_slopeIntercept = vec2FromJson(*value);
  }
  if (const auto value = j.find("invertColormap"); value != j.end() && value->is_boolean()) {
    settings.m_invertColormap = value->get<bool>();
  }
  if (const auto value = j.find("continuousColormap"); value != j.end() && value->is_boolean()) {
    settings.m_continuousColormap = value->get<bool>();
  }
  if (const auto value = j.find("colormapLevels"); value != j.end() && value->is_number_integer()) {
    settings.m_colormapLevels = std::max(value->get<int>(), 2);
  }
}

void to_json(json& j, const ProjectDifferenceMetricSettings& settings)
{
  const ProjectDifferenceMetricSettings defaults;
  j = json::object();
  addIfChanged(j, "squared", settings.m_squared, defaults.m_squared);

  json metric = settings.m_metric;
  addIfNotEmpty(j, "metric", std::move(metric));
}

void from_json(const json& j, ProjectDifferenceMetricSettings& settings)
{
  if (const auto value = j.find("squared"); value != j.end() && value->is_boolean()) {
    settings.m_squared = value->get<bool>();
  }
  if (const auto metric = j.find("metric"); metric != j.end() && metric->is_object()) {
    metric->get_to(settings.m_metric);
  }
}

void to_json(json& j, const ProjectLocalNccMetricSettings& settings)
{
  const ProjectLocalNccMetricSettings defaults;
  j = json::object();

  json metric = settings.m_metric;
  addIfNotEmpty(j, "metric", std::move(metric));
  addIfChanged(
    j,
    "presentation",
    enumToName(settings.m_presentation, k_localNccPresentationNames),
    enumToName(defaults.m_presentation, k_localNccPresentationNames));
  addIfChanged(
    j,
    "negativeCorrelationAsMismatch",
    settings.m_negativeCorrelationAsMismatch,
    defaults.m_negativeCorrelationAsMismatch);
  addIfChanged(j, "patchRadius", settings.m_patchRadius, defaults.m_patchRadius);
  addIfChanged(j, "sampleSpacing", settings.m_sampleSpacing, defaults.m_sampleSpacing);
  addIfChanged(j, "minimumValidFraction", settings.m_minimumValidFraction, defaults.m_minimumValidFraction);
  addIfChanged(j, "varianceEpsilon", settings.m_varianceEpsilon, defaults.m_varianceEpsilon);
  addIfChanged(
    j,
    "invalidStyle",
    enumToName(settings.m_invalidStyle, k_localMetricInvalidStyleNames),
    enumToName(defaults.m_invalidStyle, k_localMetricInvalidStyleNames));
}

void from_json(const json& j, ProjectLocalNccMetricSettings& settings)
{
  if (const auto metric = j.find("metric"); metric != j.end() && metric->is_object()) {
    metric->get_to(settings.m_metric);
  }
  if (
    const auto parsed =
      enumFromName<ProjectLocalNccPresentation>(j.value("presentation", ""), k_localNccPresentationNames))
  {
    settings.m_presentation = *parsed;
  }
  if (
    const auto parsed =
      enumFromName<ProjectLocalMetricInvalidStyle>(j.value("invalidStyle", ""), k_localMetricInvalidStyleNames))
  {
    settings.m_invalidStyle = *parsed;
  }
  if (const auto value = j.find("negativeCorrelationAsMismatch"); value != j.end() && value->is_boolean()) {
    settings.m_negativeCorrelationAsMismatch = value->get<bool>();
  }
  if (const auto value = j.find("patchRadius"); value != j.end() && value->is_number_integer()) {
    settings.m_patchRadius = std::clamp(value->get<int>(), 1, 5);
  }
  if (const auto value = j.find("sampleSpacing"); value != j.end() && value->is_number()) {
    settings.m_sampleSpacing = std::clamp(value->get<float>(), 0.5f, 4.0f);
  }
  if (const auto value = j.find("minimumValidFraction"); value != j.end() && value->is_number()) {
    settings.m_minimumValidFraction = std::clamp(value->get<float>(), 0.1f, 1.0f);
  }
  if (const auto value = j.find("varianceEpsilon"); value != j.end() && value->is_number()) {
    settings.m_varianceEpsilon = std::max(value->get<float>(), 0.0f);
  }
}

void to_json(json& j, const ProjectLocalLinearResidualMetricSettings& settings)
{
  const ProjectLocalLinearResidualMetricSettings defaults;
  j = json::object();

  json metric = settings.m_metric;
  addIfNotEmpty(j, "metric", std::move(metric));
  addIfChanged(j, "patchRadius", settings.m_patchRadius, defaults.m_patchRadius);
  addIfChanged(j, "sampleSpacing", settings.m_sampleSpacing, defaults.m_sampleSpacing);
  addIfChanged(j, "minimumValidFraction", settings.m_minimumValidFraction, defaults.m_minimumValidFraction);
  addIfChanged(j, "varianceEpsilon", settings.m_varianceEpsilon, defaults.m_varianceEpsilon);
  addIfChanged(
    j,
    "invalidStyle",
    enumToName(settings.m_invalidStyle, k_localMetricInvalidStyleNames),
    enumToName(defaults.m_invalidStyle, k_localMetricInvalidStyleNames));
}

void from_json(const json& j, ProjectLocalLinearResidualMetricSettings& settings)
{
  if (const auto metric = j.find("metric"); metric != j.end() && metric->is_object()) {
    metric->get_to(settings.m_metric);
  }
  if (
    const auto parsed =
      enumFromName<ProjectLocalMetricInvalidStyle>(j.value("invalidStyle", ""), k_localMetricInvalidStyleNames))
  {
    settings.m_invalidStyle = *parsed;
  }
  if (const auto value = j.find("patchRadius"); value != j.end() && value->is_number_integer()) {
    settings.m_patchRadius = std::clamp(value->get<int>(), 1, 5);
  }
  if (const auto value = j.find("sampleSpacing"); value != j.end() && value->is_number()) {
    settings.m_sampleSpacing = std::clamp(value->get<float>(), 0.5f, 4.0f);
  }
  if (const auto value = j.find("minimumValidFraction"); value != j.end() && value->is_number()) {
    settings.m_minimumValidFraction = std::clamp(value->get<float>(), 0.1f, 1.0f);
  }
  if (const auto value = j.find("varianceEpsilon"); value != j.end() && value->is_number()) {
    settings.m_varianceEpsilon = std::max(value->get<float>(), 0.0f);
  }
}

void to_json(json& j, const ProjectComparisonSettings& settings)
{
  const ProjectComparisonSettings defaults;
  j = json::object();

  json difference = settings.m_difference;
  addIfNotEmpty(j, "difference", std::move(difference));

  json localNcc = settings.m_localNcc;
  addIfNotEmpty(j, "localNormalizedCrossCorrelation", std::move(localNcc));

  json localLinearResidual = settings.m_localLinearResidual;
  addIfNotEmpty(j, "localLinearResidual", std::move(localLinearResidual));

  json overlay = json::object();
  addIfChanged(overlay, "magentaCyan", settings.m_overlayMagentaCyan, defaults.m_overlayMagentaCyan);
  addIfNotEmpty(j, "overlay", std::move(overlay));

  json quadrants = json::object();
  addIfChanged(quadrants, "x", static_cast<bool>(settings.m_quadrants.x), static_cast<bool>(defaults.m_quadrants.x));
  addIfChanged(quadrants, "y", static_cast<bool>(settings.m_quadrants.y), static_cast<bool>(defaults.m_quadrants.y));
  addIfNotEmpty(j, "quadrants", std::move(quadrants));

  json checkerboard = json::object();
  addIfChanged(checkerboard, "squares", settings.m_checkerboardSquares, defaults.m_checkerboardSquares);
  addIfNotEmpty(j, "checkerboard", std::move(checkerboard));

  json flashlight = json::object();
  addIfChanged(flashlight, "radiusFraction", settings.m_flashlightRadiusFraction, defaults.m_flashlightRadiusFraction);
  addIfChanged(
    flashlight,
    "overlayMovingImage",
    settings.m_flashlightOverlayMovingImage,
    defaults.m_flashlightOverlayMovingImage);
  addIfNotEmpty(j, "flashlight", std::move(flashlight));
}

void from_json(const json& j, ProjectComparisonSettings& settings)
{
  if (const auto difference = j.find("difference"); difference != j.end() && difference->is_object()) {
    difference->get_to(settings.m_difference);
  }
  if (const auto localNcc = j.find("localNormalizedCrossCorrelation"); localNcc != j.end() && localNcc->is_object()) {
    localNcc->get_to(settings.m_localNcc);
  }
  if (const auto localResidual = j.find("localLinearResidual"); localResidual != j.end() && localResidual->is_object())
  {
    localResidual->get_to(settings.m_localLinearResidual);
  }
  if (const auto overlay = j.find("overlay"); overlay != j.end() && overlay->is_object()) {
    if (const auto value = overlay->find("magentaCyan"); value != overlay->end() && value->is_boolean()) {
      settings.m_overlayMagentaCyan = value->get<bool>();
    }
  }
  if (const auto quadrants = j.find("quadrants"); quadrants != j.end() && quadrants->is_object()) {
    bool x = static_cast<bool>(settings.m_quadrants.x);
    bool y = static_cast<bool>(settings.m_quadrants.y);
    if (const auto value = quadrants->find("x"); value != quadrants->end() && value->is_boolean()) {
      x = value->get<bool>();
    }
    if (const auto value = quadrants->find("y"); value != quadrants->end() && value->is_boolean()) {
      y = value->get<bool>();
    }
    settings.m_quadrants = glm::ivec2{x, y};
  }
  if (const auto checker = j.find("checkerboard"); checker != j.end() && checker->is_object()) {
    if (const auto value = checker->find("squares"); value != checker->end() && value->is_number_integer()) {
      settings.m_checkerboardSquares = std::clamp(value->get<int>(), 2, 2048);
    }
  }
  if (const auto flashlight = j.find("flashlight"); flashlight != j.end() && flashlight->is_object()) {
    if (const auto value = flashlight->find("radiusFraction"); value != flashlight->end() && value->is_number()) {
      settings.m_flashlightRadiusFraction = std::clamp(value->get<float>(), 0.01f, 1.0f);
    }
    if (const auto value = flashlight->find("overlayMovingImage"); value != flashlight->end() && value->is_boolean()) {
      settings.m_flashlightOverlayMovingImage = value->get<bool>();
    }
  }
}

void to_json(json& j, const ProjectRaycastingSettings& settings)
{
  const ProjectRaycastingSettings defaults;
  j = json::object();
  addIfChanged(j, "samplingFactor", settings.m_samplingFactor, defaults.m_samplingFactor);
  addIfChanged(
    j,
    "transparentBackgroundWhenNoHit",
    settings.m_transparentBackgroundWhenNoHit,
    defaults.m_transparentBackgroundWhenNoHit);
  addIfChanged(
    j,
    "showImageBox",
    settings.m_backgroundEdgeBrighteningEnabled,
    defaults.m_backgroundEdgeBrighteningEnabled);
  addIfChanged(j, "showCrosshairsGlyph", settings.m_showCrosshairsIn3D, defaults.m_showCrosshairsIn3D);
  addIfChanged(
    j,
    "crosshairsGlyphDiameterVoxels",
    settings.m_crosshairs3DGlyphDiameterVoxelDiagonals,
    defaults.m_crosshairs3DGlyphDiameterVoxelDiagonals);
  addIfChanged(
    j,
    "showCameraFrustumIn2DViews",
    settings.m_showThreeDCameraFrustumIn2DViews,
    defaults.m_showThreeDCameraFrustumIn2DViews);
  addIfChanged(
    j,
    "reverseRotateAboutEye",
    settings.m_reverseThreeDRotateAboutEye,
    defaults.m_reverseThreeDRotateAboutEye);
  addIfChanged(
    j,
    "cameraFrustumColor",
    vec4ToJson(settings.m_threeDCameraFrustumColor),
    vec4ToJson(defaults.m_threeDCameraFrustumColor));
  addIfChanged(j, "renderFrontFaces", settings.m_renderFrontFaces, defaults.m_renderFrontFaces);
  addIfChanged(j, "renderBackFaces", settings.m_renderBackFaces, defaults.m_renderBackFaces);
  addIfChanged(
    j,
    "segmentationMasking",
    enumToName(settings.m_segmentationMasking, k_raycastSegmentationMaskingNames),
    enumToName(defaults.m_segmentationMasking, k_raycastSegmentationMaskingNames));
}

void from_json(const json& j, ProjectRaycastingSettings& settings)
{
  if (const auto value = j.find("samplingFactor"); value != j.end() && value->is_number()) {
    settings.m_samplingFactor = std::clamp(value->get<float>(), 0.5f, 2.0f);
  }
  if (const auto value = j.find("transparentBackgroundWhenNoHit"); value != j.end() && value->is_boolean()) {
    settings.m_transparentBackgroundWhenNoHit = value->get<bool>();
  }
  if (const auto value = j.find("showImageBox"); value != j.end() && value->is_boolean()) {
    settings.m_backgroundEdgeBrighteningEnabled = value->get<bool>();
  }
  if (const auto value = j.find("showCrosshairsGlyph"); value != j.end() && value->is_boolean()) {
    settings.m_showCrosshairsIn3D = value->get<bool>();
  }
  if (const auto value = j.find("crosshairsGlyphDiameterVoxels"); value != j.end() && value->is_number()) {
    settings.m_crosshairs3DGlyphDiameterVoxelDiagonals = std::clamp(value->get<float>(), 0.1f, 10.0f);
  }
  if (const auto value = j.find("showCameraFrustumIn2DViews"); value != j.end() && value->is_boolean()) {
    settings.m_showThreeDCameraFrustumIn2DViews = value->get<bool>();
  }
  if (const auto value = j.find("reverseRotateAboutEye"); value != j.end() && value->is_boolean()) {
    settings.m_reverseThreeDRotateAboutEye = value->get<bool>();
  }
  if (const auto value = j.find("cameraFrustumColor"); value != j.end() && value->is_array() && value->size() == 4) {
    settings.m_threeDCameraFrustumColor = glm::clamp(vec4FromJson(*value), glm::vec4{0.0f}, glm::vec4{1.0f});
  }
  if (const auto value = j.find("renderFrontFaces"); value != j.end() && value->is_boolean()) {
    settings.m_renderFrontFaces = value->get<bool>();
  }
  if (const auto value = j.find("renderBackFaces"); value != j.end() && value->is_boolean()) {
    settings.m_renderBackFaces = value->get<bool>();
  }
  if (
    const auto parsed = enumFromName<ProjectSegmentationRaycastMasking>(
      j.value("segmentationMasking", ""),
      k_raycastSegmentationMaskingNames))
  {
    settings.m_segmentationMasking = *parsed;
  }
}

void to_json(json& j, const ProjectIntensityProjectionSettings& settings)
{
  const ProjectIntensityProjectionSettings defaults;
  j = json::object();
  addIfChanged(j, "useMaximumImageExtent", settings.m_useMaximumImageExtent, defaults.m_useMaximumImageExtent);
  addIfChanged(j, "slabThicknessMm", settings.m_slabThicknessMm, defaults.m_slabThicknessMm);
  addIfChanged(j, "xrayEnergyKeV", settings.m_xrayEnergyKeV, defaults.m_xrayEnergyKeV);
  addIfChanged(j, "xrayWindow", settings.m_xrayWindow, defaults.m_xrayWindow);
  addIfChanged(j, "xrayLevel", settings.m_xrayLevel, defaults.m_xrayLevel);
}

void from_json(const json& j, ProjectIntensityProjectionSettings& settings)
{
  if (const auto value = j.find("useMaximumImageExtent"); value != j.end() && value->is_boolean()) {
    settings.m_useMaximumImageExtent = value->get<bool>();
  }
  if (const auto value = j.find("slabThicknessMm"); value != j.end() && value->is_number()) {
    settings.m_slabThicknessMm = std::max(value->get<float>(), 0.0f);
  }
  if (const auto value = j.find("xrayEnergyKeV"); value != j.end() && value->is_number()) {
    settings.m_xrayEnergyKeV = value->get<float>();
  }
  if (const auto value = j.find("xrayWindow"); value != j.end() && value->is_number()) {
    settings.m_xrayWindow = std::clamp(value->get<float>(), 1.0e-3f, 1.0f);
  }
  if (const auto value = j.find("xrayLevel"); value != j.end() && value->is_number()) {
    settings.m_xrayLevel = std::clamp(value->get<float>(), 0.0f, 1.0f);
  }
}

void to_json(json& j, const ProjectSegmentationDisplaySettings& settings)
{
  const ProjectSegmentationDisplaySettings defaults;
  j = json::object();
  addIfChanged(
    j,
    "modulateOpacityWithImageOpacity",
    settings.m_modulateOpacityWithImageOpacity,
    defaults.m_modulateOpacityWithImageOpacity);
  addIfChanged(
    j,
    "outlineStyle",
    enumToName(settings.m_outlineStyle, k_segmentationOutlineNames),
    enumToName(defaults.m_outlineStyle, k_segmentationOutlineNames));
  addIfChanged(j, "interiorOpacity", settings.m_interiorOpacity, defaults.m_interiorOpacity);
  addIfChanged(j, "erosionFactor", settings.m_erosionFactor, defaults.m_erosionFactor);
}

void from_json(const json& j, ProjectSegmentationDisplaySettings& settings)
{
  if (const auto value = j.find("modulateOpacityWithImageOpacity"); value != j.end() && value->is_boolean()) {
    settings.m_modulateOpacityWithImageOpacity = value->get<bool>();
  }
  if (
    const auto parsed = enumFromName<SegmentationOutlineStyle>(j.value("outlineStyle", ""), k_segmentationOutlineNames))
  {
    settings.m_outlineStyle = *parsed;
  }
  if (const auto value = j.find("interiorOpacity"); value != j.end() && value->is_number()) {
    settings.m_interiorOpacity = std::clamp(value->get<float>(), 0.0f, 1.0f);
  }
  if (const auto value = j.find("erosionFactor"); value != j.end() && value->is_number()) {
    settings.m_erosionFactor = std::clamp(value->get<float>(), 0.5f, 1.0f);
  }
}

void to_json(json& j, const ProjectIsosurfaceDisplaySettings& settings)
{
  const ProjectIsosurfaceDisplaySettings defaults;
  j = json::object();
  addIfChanged(
    j,
    "floatingPointInterpolationPolicy",
    enumToName(settings.m_floatingPointInterpolationPolicy, k_floatingPointInterpolationPolicyNames),
    enumToName(defaults.m_floatingPointInterpolationPolicy, k_floatingPointInterpolationPolicyNames));
  addIfChanged(
    j,
    "modulateOpacityWithImageOpacity",
    settings.m_modulateOpacityWithImageOpacity,
    defaults.m_modulateOpacityWithImageOpacity);
}

void from_json(const json& j, ProjectIsosurfaceDisplaySettings& settings)
{
  if (const auto policy = j.find("floatingPointInterpolationPolicy"); policy != j.end() && policy->is_string()) {
    if (
      const auto parsed = enumFromName<FloatingPointLinearInterpolationPolicy>(
        policy->get<std::string>(),
        k_floatingPointInterpolationPolicyNames))
    {
      settings.m_floatingPointInterpolationPolicy = *parsed;
    }
  }
  if (const auto value = j.find("modulateOpacityWithImageOpacity"); value != j.end() && value->is_boolean()) {
    settings.m_modulateOpacityWithImageOpacity = value->get<bool>();
  }
}

void to_json(json& j, const ProjectAnnotationDisplaySettings& settings)
{
  const ProjectAnnotationDisplaySettings defaults;
  j = json::object();

  json rendering = json::object();
  addIfChanged(rendering, "annotationsOnTop", settings.m_annotationsOnTop, defaults.m_annotationsOnTop);
  addIfChanged(rendering, "landmarksOnTop", settings.m_landmarksOnTop, defaults.m_landmarksOnTop);
  addIfChanged(
    rendering,
    "hideAnnotationVertices",
    settings.m_hideAnnotationVertices,
    defaults.m_hideAnnotationVertices);
  addIfNotEmpty(j, "rendering", std::move(rendering));
}

void from_json(const json& j, ProjectAnnotationDisplaySettings& settings)
{
  const auto rendering = j.find("rendering");
  if (rendering == j.end() || !rendering->is_object()) {
    return;
  }

  if (const auto value = rendering->find("annotationsOnTop"); value != rendering->end() && value->is_boolean()) {
    settings.m_annotationsOnTop = value->get<bool>();
  }
  if (const auto value = rendering->find("landmarksOnTop"); value != rendering->end() && value->is_boolean()) {
    settings.m_landmarksOnTop = value->get<bool>();
  }
  if (const auto value = rendering->find("hideAnnotationVertices"); value != rendering->end() && value->is_boolean()) {
    settings.m_hideAnnotationVertices = value->get<bool>();
  }
}

void to_json(json& j, const RegistrationResult& result)
{
  j = json::object();

  if (!result.m_backend.empty()) {
    j["backend"] = result.m_backend;
  }

  optionalPathObjectToJson(j, "fixedImage", result.m_fixedImage);
  optionalPathObjectToJson(j, "movingImage", result.m_movingImage);
  optionalPathObjectToJson(j, "manifest", result.m_manifestFileName);
  optionalPathObjectToJson(j, "warpedImage", result.m_warpedImage);
  optionalPathObjectToJson(j, "inverseWarpField", result.m_inverseWarpField);
  optionalPathObjectToJson(j, "forwardWarpField", result.m_forwardWarpField);
  optionalPathObjectToJson(j, "affine", result.m_affineTransform);
  if (!result.m_warpedSegmentations.empty()) {
    j["warpedSegmentations"] = pathObjectVectorToJson(result.m_warpedSegmentations);
  }
  if (!result.m_transformedSurfaces.empty()) {
    j["transformedSurfaces"] = pathObjectVectorToJson(result.m_transformedSurfaces);
  }
  if (!result.m_transformedLandmarks.empty()) {
    j["transformedLandmarks"] = pathObjectVectorToJson(result.m_transformedLandmarks);
  }
  if (!result.m_warnings.empty()) {
    j["warnings"] = result.m_warnings;
  }
}

void from_json(const json& j, RegistrationResult& result)
{
  if (const auto value = j.find("backend"); value != j.end() && value->is_string()) {
    result.m_backend = value->get<std::string>();
  }
  optionalPathObjectFromJson(j, "fixedImage", result.m_fixedImage);
  optionalPathObjectFromJson(j, "movingImage", result.m_movingImage);
  optionalPathObjectFromJson(j, "manifest", result.m_manifestFileName);
  optionalPathObjectFromJson(j, "warpedImage", result.m_warpedImage);
  optionalPathObjectFromJson(j, "inverseWarpField", result.m_inverseWarpField);
  optionalPathObjectFromJson(j, "forwardWarpField", result.m_forwardWarpField);
  optionalPathObjectFromJson(j, "affine", result.m_affineTransform);
  if (const auto value = j.find("warpedSegmentations"); value != j.end()) {
    result.m_warpedSegmentations = pathObjectVectorFromJson(*value, "warpedSegmentations");
  }
  if (const auto value = j.find("transformedSurfaces"); value != j.end()) {
    result.m_transformedSurfaces = pathObjectVectorFromJson(*value, "transformedSurfaces");
  }
  if (const auto value = j.find("transformedLandmarks"); value != j.end()) {
    result.m_transformedLandmarks = pathObjectVectorFromJson(*value, "transformedLandmarks");
  }
  if (const auto value = j.find("warnings"); value != j.end() && value->is_array()) {
    result.m_warnings = value->get<std::vector<std::string>>();
  }
}

void to_json(json& j, const DefaultLayoutOverride& override)
{
  j = json{{"index", override.m_index}, {"layout", override.m_layout}};
}

void from_json(const json& j, DefaultLayoutOverride& override)
{
  override.m_index = j.at("index").get<std::size_t>();
  override.m_layout = j.at("layout").get<layout::LayoutSpec>();
}

void to_json(json& j, const EntropyProject& project)
{
  std::vector<Image> images;
  images.reserve(project.m_additionalImages.size() + 1);
  images.push_back(project.m_referenceImage);
  images.insert(images.end(), project.m_additionalImages.begin(), project.m_additionalImages.end());

  j = json{{"version", versionToJson(k_projectFormatMajorVersion, k_projectFormatMinorVersion)}, {"images", images}};
  if (
    project.m_layoutsFileName || !project.m_layouts.empty() || !project.m_removedDefaultLayoutIndices.empty() ||
    !project.m_modifiedDefaultLayouts.empty() || project.m_currentLayoutIndex)
  {
    json layouts = json::object();
    if (project.m_layoutsFileName) {
      layouts["path"] = pathToString(*project.m_layoutsFileName);
    }
    else if (!project.m_layouts.empty()) {
      layouts["embedded"] = project.m_layouts;
    }
    if (!project.m_removedDefaultLayoutIndices.empty()) {
      layouts["removedDefaults"] = project.m_removedDefaultLayoutIndices;
    }
    if (!project.m_modifiedDefaultLayouts.empty()) {
      layouts["modifiedDefaults"] = project.m_modifiedDefaultLayouts;
    }
    if (project.m_currentLayoutIndex) {
      layouts["current"] = *project.m_currentLayoutIndex;
    }
    j["layouts"] = std::move(layouts);
  }

  json settings = json::object();
  json interface = project.m_interface;
  addIfNotEmpty(settings, "interface", std::move(interface));

  json view = project.m_view;
  addIfNotEmpty(settings, "view", std::move(view));

  json rendering = json::object();
  json comparison = project.m_comparison;
  addIfNotEmpty(rendering, "comparison", std::move(comparison));
  json raycasting = project.m_raycasting;
  addIfNotEmpty(rendering, "volumeRaycasting", std::move(raycasting));
  json intensityProjection = project.m_intensityProjection;
  addIfNotEmpty(rendering, "intensityProjection", std::move(intensityProjection));
  json segmentation = project.m_segmentationDisplay;
  addIfNotEmpty(rendering, "segmentation", std::move(segmentation));
  json isosurfaces = project.m_isosurfaces;
  addIfNotEmpty(rendering, "isosurfaces", std::move(isosurfaces));
  addIfNotEmpty(settings, "rendering", std::move(rendering));

  json annotations = project.m_annotationDisplay;
  addIfNotEmpty(settings, "annotations", std::move(annotations));
  addIfNotEmpty(j, "settings", std::move(settings));

  if (!project.m_registrationResults.empty()) {
    j["registrationResults"] = project.m_registrationResults;
  }
}

void from_json(const json& j, EntropyProject& project)
{
  if (const auto versionValue = j.find("version"); versionValue != j.end()) {
    const JsonVersion version = versionFromJson(*versionValue);
    if (version.major != k_projectFormatMajorVersion || version.minor != k_projectFormatMinorVersion) {
      throwDebug(
        "Unsupported Entropy project JSON version " + std::to_string(version.major) + "." +
        std::to_string(version.minor));
    }
  }

  std::vector<Image> images = j.at("images").get<std::vector<Image>>();
  if (images.empty()) {
    throwDebug("Entropy project JSON must contain at least one image");
  }

  project.m_referenceImage = std::move(images.front());
  project.m_additionalImages.clear();
  project.m_additionalImages.reserve(images.size() - 1);
  for (std::size_t i = 1; i < images.size(); ++i) {
    project.m_additionalImages.push_back(std::move(images.at(i)));
  }
  if (const auto layouts = j.find("layouts"); layouts != j.end()) {
    if (!layouts->is_object()) {
      throwDebug("Project layouts entry must be an object");
    }
    if (const auto path = layouts->find("path"); path != layouts->end() && path->is_string()) {
      project.m_layoutsFileName = path->get<std::string>();
    }
    if (const auto embedded = layouts->find("embedded"); embedded != layouts->end()) {
      embedded->get_to(project.m_layouts);
    }
    if (const auto removedDefaults = layouts->find("removedDefaults"); removedDefaults != layouts->end()) {
      removedDefaults->get_to(project.m_removedDefaultLayoutIndices);
    }
    if (const auto modifiedDefaults = layouts->find("modifiedDefaults"); modifiedDefaults != layouts->end()) {
      modifiedDefaults->get_to(project.m_modifiedDefaultLayouts);
    }
    if (const auto current = layouts->find("current"); current != layouts->end() && !current->is_null()) {
      project.m_currentLayoutIndex = current->get<std::size_t>();
    }
  }
  if (const auto settings = j.find("settings"); settings != j.end() && settings->is_object()) {
    if (const auto interface = settings->find("interface"); interface != settings->end() && interface->is_object()) {
      interface->get_to(project.m_interface);
    }
    if (const auto view = settings->find("view"); view != settings->end() && view->is_object()) {
      view->get_to(project.m_view);
    }
    if (const auto rendering = settings->find("rendering"); rendering != settings->end() && rendering->is_object()) {
      if (const auto comparison = rendering->find("comparison");
          comparison != rendering->end() && comparison->is_object())
      {
        comparison->get_to(project.m_comparison);
      }
      if (const auto raycasting = rendering->find("volumeRaycasting");
          raycasting != rendering->end() && raycasting->is_object())
      {
        raycasting->get_to(project.m_raycasting);
      }
      if (const auto intensityProjection = rendering->find("intensityProjection");
          intensityProjection != rendering->end() && intensityProjection->is_object())
      {
        intensityProjection->get_to(project.m_intensityProjection);
      }
      if (const auto segmentation = rendering->find("segmentation");
          segmentation != rendering->end() && segmentation->is_object())
      {
        segmentation->get_to(project.m_segmentationDisplay);
      }
      if (const auto isosurfaces = rendering->find("isosurfaces");
          isosurfaces != rendering->end() && isosurfaces->is_object())
      {
        isosurfaces->get_to(project.m_isosurfaces);
      }
    }
    if (const auto annotations = settings->find("annotations");
        annotations != settings->end() && annotations->is_object())
    {
      annotations->get_to(project.m_annotationDisplay);
    }
  }
  if (const auto registrationResults = j.find("registrationResults");
      registrationResults != j.end() && registrationResults->is_array())
  {
    registrationResults->get_to(project.m_registrationResults);
  }
}

serialize::EntropyProject createProjectFromInputParams(const InputParams& params)
{
  serialize::EntropyProject project;

  auto addSegmentations = [](serialize::Image& image, const std::vector<fs::path>& segFiles) {
    for (const fs::path& segFile : segFiles) {
      serialize::Segmentation seg;
      seg.m_segFileName = segFile;
      image.m_segmentations.emplace_back(std::move(seg));
    }
  };

  if (!params.imageFiles.empty()) {
    // If images were provided as command-line arguments, use them.

    // Add the reference image, which is at index 0:
    project.m_referenceImage.m_imageFileName = params.imageFiles[0].image;

    // Add the reference segmentations, if provided:
    addSegmentations(project.m_referenceImage, params.imageFiles[0].segmentations);

    // Add the additional images, which begin at index 1:
    for (std::size_t i = 1; i < params.imageFiles.size(); ++i) {
      serialize::Image image;
      image.m_imageFileName = params.imageFiles[i].image;

      // Add the additional image segmentations:
      addSegmentations(image, params.imageFiles[i].segmentations);

      project.m_additionalImages.emplace_back(std::move(image));
    }
  }
  else if (params.projectFile) {
    // A project file was provided as a command-line argument, so open it:
    const bool valid = serialize::open(project, *params.projectFile);

    if (!valid) {
      spdlog::critical("Invalid input in project file {}", *params.projectFile);
      exit(EXIT_FAILURE);
    }
  }
  else {
    spdlog::critical("No project file or image arguments were provided");
    exit(EXIT_FAILURE);
  }

  return project;
}

bool open(EntropyProject& project, const fs::path& fileName)
{
  auto makeOptionalPathCanonicalAbsolute =
    [](std::optional<fs::path>& path, const fs::path& projectBasePath, const char* description) {
      if (!path) {
        return;
      }

      if (path->empty()) {
        spdlog::warn("Ignoring empty {} path", description);
        path = std::nullopt;
        return;
      }

      fs::path absolutePath = path->is_absolute() ? *path : (projectBasePath / *path).lexically_normal();
      std::error_code error;
      if (fs::exists(absolutePath, error)) {
        absolutePath = fs::canonical(absolutePath);
      }
      else {
        spdlog::warn("Referenced {} {} does not exist", description, absolutePath);
      }
      *path = absolutePath;
    };

  auto makeRegistrationResultPathsCanonicalAbsolute =
    [](serialize::RegistrationResult& result, const fs::path& projectBasePath) {
      makePathCanonicalAbsolute(result.m_fixedImage, projectBasePath, "registration fixed image");
      makePathCanonicalAbsolute(result.m_movingImage, projectBasePath, "registration moving image");
      makePathCanonicalAbsolute(result.m_manifestFileName, projectBasePath, "registration manifest");
      makePathCanonicalAbsolute(result.m_warpedImage, projectBasePath, "registered image");
      makePathCanonicalAbsolute(result.m_inverseWarpField, projectBasePath, "registration inverse warp");
      makePathCanonicalAbsolute(result.m_forwardWarpField, projectBasePath, "registration forward warp");
      makePathCanonicalAbsolute(result.m_affineTransform, projectBasePath, "registration affine transform");
      for (fs::path& path : result.m_warpedSegmentations) {
        makePathCanonicalAbsolute(path, projectBasePath, "registered segmentation");
      }
      for (fs::path& path : result.m_transformedSurfaces) {
        makePathCanonicalAbsolute(path, projectBasePath, "transformed registration surface");
      }
      for (fs::path& path : result.m_transformedLandmarks) {
        makePathCanonicalAbsolute(path, projectBasePath, "transformed registration landmarks");
      }
    };

  // Make all paths in the image absolute:
  auto makeCanonicalAbsolute = [](serialize::Image& image, const fs::path& projectBasePath) {
    const fs::path saveCurrentPath = fs::current_path(); // save current path
    fs::current_path(projectBasePath);                   // set current path to project path

    image.m_imageFileName = fs::canonical(image.m_imageFileName);

    if (image.m_dicomSource) {
      if (!image.m_dicomSource->m_rootPath.empty()) {
        image.m_dicomSource->m_rootPath = fs::canonical(image.m_dicomSource->m_rootPath);
      }
      for (auto& file : image.m_dicomSource->m_files) {
        file = fs::canonical(file);
      }
    }

    if (image.m_initialAffineFileName) {
      if (image.m_initialAffineFileName->empty()) {
        spdlog::warn("Ignoring empty affine transform path for image {}", image.m_imageFileName);
        image.m_initialAffineFileName = std::nullopt;
      }
      else {
        image.m_initialAffineFileName = fs::canonical(*image.m_initialAffineFileName);
      }
    }

    if (image.m_manualAffineFileName) {
      if (image.m_manualAffineFileName->empty()) {
        spdlog::warn("Ignoring empty manual affine transform path for image {}", image.m_imageFileName);
        image.m_manualAffineFileName = std::nullopt;
      }
      else {
        image.m_manualAffineFileName = fs::canonical(*image.m_manualAffineFileName);
      }
    }

    if (image.m_inverseWarpFieldPath) {
      if (image.m_inverseWarpFieldPath->empty()) {
        spdlog::warn("Ignoring empty inverse warp path for image {}", image.m_imageFileName);
        image.m_inverseWarpFieldPath = std::nullopt;
      }
      else {
        image.m_inverseWarpFieldPath = fs::canonical(*image.m_inverseWarpFieldPath);
      }
    }

    if (image.m_forwardWarpFieldPath) {
      if (image.m_forwardWarpFieldPath->empty()) {
        spdlog::warn("Ignoring empty forward warp path for image {}", image.m_imageFileName);
        image.m_forwardWarpFieldPath = std::nullopt;
      }
      else {
        image.m_forwardWarpFieldPath = fs::canonical(*image.m_forwardWarpFieldPath);
      }
    }

    if (image.m_inverseWarpReferenceImagePath) {
      if (image.m_inverseWarpReferenceImagePath->empty()) {
        spdlog::warn("Ignoring empty inverse warp reference image path for image {}", image.m_imageFileName);
        image.m_inverseWarpReferenceImagePath = std::nullopt;
      }
      else {
        image.m_inverseWarpReferenceImagePath = fs::canonical(*image.m_inverseWarpReferenceImagePath);
      }
    }

    if (image.m_annotationsFileName) {
      if (image.m_annotationsFileName->empty()) {
        spdlog::warn("Ignoring empty annotations path for image {}", image.m_imageFileName);
        image.m_annotationsFileName = std::nullopt;
      }
      else {
        image.m_annotationsFileName = fs::canonical(*image.m_annotationsFileName);
      }
    }

    for (auto segIt = image.m_segmentations.begin(); segIt != image.m_segmentations.end();) {
      if (segIt->m_segFileName.empty()) {
        spdlog::warn("Ignoring empty segmentation path for image {}", image.m_imageFileName);
        segIt = image.m_segmentations.erase(segIt);
      }
      else {
        segIt->m_segFileName = fs::canonical(segIt->m_segFileName);
        ++segIt;
      }
    }

    for (serialize::LandmarkGroup& landmarks : image.m_landmarkGroups) {
      if (landmarks.m_csvFileName && !landmarks.m_csvFileName->empty()) {
        landmarks.m_csvFileName = fs::canonical(*landmarks.m_csvFileName);
      }
    }

    fs::current_path(saveCurrentPath); // restore current path
  };

  std::ifstream inFile;
  inFile.exceptions(inFile.exceptions() | std::ios::failbit | std::ifstream::badbit);

  try {
    inFile.open(fileName, std::ios_base::in);

    if (!inFile) {
      throw std::system_error(errno, std::system_category(), "Failed to open project file " + fileName.string());
    }

    json j;
    inFile >> j;

    // Image paths in the project file can be specified relative to the project location,
    // so we need to make all image paths absolute.
    project = j.get<EntropyProject>();
    spdlog::debug("Parsed project JSON:\n{}", j.dump(2));

    fs::path projectBasePath(fileName);
    projectBasePath.remove_filename();

    if (projectBasePath.empty()) {
      projectBasePath = fs::current_path();
      spdlog::warn("Project base path is empty; using current path ({})", projectBasePath);
    }

    projectBasePath = fs::canonical(projectBasePath);
    spdlog::debug("Base path for the project file is {}", projectBasePath);

    applyToImagePaths(project, projectBasePath, makeCanonicalAbsolute);
    makeOptionalPathCanonicalAbsolute(project.m_layoutsFileName, projectBasePath, "layouts file");
    for (serialize::RegistrationResult& result : project.m_registrationResults) {
      makeRegistrationResultPathsCanonicalAbsolute(result, projectBasePath);
    }

    const json jAbs = project;
    spdlog::debug("Parsed project JSON (with absolute paths):\n{}", jAbs.dump(2));
    spdlog::info("Loaded project from file {}", fileName);
    return true;
  }
  catch (const std::ios_base::failure& e) {
#if HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR
    // e.code() is only available if the lib actually follows ISO §27.5.3.1.1
    // and derives ios_base::failure from system_error
    spdlog::error("Error #{}: {}", e.code().value(), e.code().message());

    if (std::make_error_condition(std::io_errc::stream) == e.code()) {
      spdlog::error("Stream error opening file");
    }
    else {
      spdlog::error("Unknown failure opening file");
    }
#else
    logStdErrno();
#endif

    spdlog::error("Failure opening project from JSON file {}: {}", fileName, e.what());
    return false;
  }
  catch (const std::exception& e) {
    spdlog::error("Error opening project from JSON file {}: {}", fileName, e.what());
    return false;
  }
}

bool save(const EntropyProject& project, const fs::path& fileName)
{
  // Make all paths in the image relative to the base path:
  auto makeRelative = [](serialize::Image& image, const fs::path& projectBasePath) {
    image.m_imageFileName = fs::relative(image.m_imageFileName, projectBasePath);

    if (image.m_dicomSource) {
      if (!image.m_dicomSource->m_rootPath.empty()) {
        image.m_dicomSource->m_rootPath = fs::relative(image.m_dicomSource->m_rootPath, projectBasePath);
      }
      for (auto& file : image.m_dicomSource->m_files) {
        file = fs::relative(file, projectBasePath);
      }
    }

    if (image.m_initialAffineFileName) {
      image.m_initialAffineFileName = fs::relative(*image.m_initialAffineFileName, projectBasePath);
    }

    if (image.m_manualAffineFileName) {
      image.m_manualAffineFileName = fs::relative(*image.m_manualAffineFileName, projectBasePath);
    }

    if (image.m_inverseWarpFieldPath) {
      image.m_inverseWarpFieldPath = fs::relative(*image.m_inverseWarpFieldPath, projectBasePath);
    }

    if (image.m_forwardWarpFieldPath) {
      image.m_forwardWarpFieldPath = fs::relative(*image.m_forwardWarpFieldPath, projectBasePath);
    }

    if (image.m_inverseWarpReferenceImagePath) {
      image.m_inverseWarpReferenceImagePath = fs::relative(*image.m_inverseWarpReferenceImagePath, projectBasePath);
    }

    if (image.m_annotationsFileName) {
      image.m_annotationsFileName = fs::relative(*image.m_annotationsFileName, projectBasePath);
    }

    for (serialize::Segmentation& seg : image.m_segmentations) {
      seg.m_segFileName = fs::relative(seg.m_segFileName, projectBasePath);
    }

    for (serialize::LandmarkGroup& lm : image.m_landmarkGroups) {
      if (lm.m_csvFileName && !lm.m_csvFileName->empty()) {
        lm.m_csvFileName = fs::relative(*lm.m_csvFileName, projectBasePath);
      }
    }
  };

  auto makeRelativeIfPresent = [](std::optional<fs::path>& path, const fs::path& projectBasePath) {
    if (path && !path->empty()) {
      path = fs::relative(*path, projectBasePath);
    }
  };

  auto makeRegistrationResultPathsRelative =
    [&](serialize::RegistrationResult& result, const fs::path& projectBasePath) {
      makeRelativeIfPresent(result.m_fixedImage, projectBasePath);
      makeRelativeIfPresent(result.m_movingImage, projectBasePath);
      makeRelativeIfPresent(result.m_manifestFileName, projectBasePath);
      makeRelativeIfPresent(result.m_warpedImage, projectBasePath);
      makeRelativeIfPresent(result.m_inverseWarpField, projectBasePath);
      makeRelativeIfPresent(result.m_forwardWarpField, projectBasePath);
      makeRelativeIfPresent(result.m_affineTransform, projectBasePath);
      for (fs::path& path : result.m_warpedSegmentations) {
        path = fs::relative(path, projectBasePath);
      }
      for (fs::path& path : result.m_transformedSurfaces) {
        path = fs::relative(path, projectBasePath);
      }
      for (fs::path& path : result.m_transformedLandmarks) {
        path = fs::relative(path, projectBasePath);
      }
    };

  try {
    // Create version of project with image paths relative to project filename base path:
    EntropyProject projectRelative = project;

    fs::path projectBasePath(fileName);
    projectBasePath.remove_filename();

    if (projectBasePath.empty()) {
      spdlog::warn("Project base path is empty; using current path");
      projectBasePath = fs::current_path();
    }

    projectBasePath = fs::canonical(projectBasePath);
    spdlog::debug("Base path for the project file is {}", projectBasePath);

    if (projectRelative.m_layoutsFileName) {
      projectRelative.m_layoutsFileName = fs::relative(*projectRelative.m_layoutsFileName, projectBasePath);
    }

    // Make all image paths relative to the project file:
    applyToImagePaths(projectRelative, projectBasePath, makeRelative);
    for (serialize::RegistrationResult& result : projectRelative.m_registrationResults) {
      makeRegistrationResultPathsRelative(result, projectBasePath);
    }
    const json j = projectRelative;

    std::ofstream outFile(fileName);
    outFile << j.dump(2) << '\n';

    spdlog::debug("Saved JSON for project (with relative image paths):\n{}", j.dump(2));
    spdlog::info("Saved project to file {}", fileName);
    return true;
  }
  catch (const std::exception& e) {
    spdlog::error("Error saving project to JSON file: {}", e.what());
    return false;
  }
}

bool openAffineTxFile(glm::dmat4& matrix, const fs::path& fileName)
{
  std::ifstream inFile;
  inFile.exceptions(inFile.exceptions() | std::ifstream::badbit);

  try {
    inFile.open(fileName, std::ios_base::in);

    if (!inFile) {
      throw std::system_error(errno, std::system_category(), "Failed to open input file " + fileName.string());
    }

    std::vector<std::vector<double>> rows;
    std::string temp;

    while (std::getline(inFile, temp)) {
      std::istringstream buffer(temp);

      const std::vector<double> row{(std::istream_iterator<double>(buffer)), std::istream_iterator<double>()};

      if (4 != row.size()) {
        throw std::length_error(
          fmt::format("4x4 affine matrix row {} read with invalid length {}", rows.size() + 1, row.size()));
      }

      rows.push_back(row);
    }

    if (4 != rows.size()) {
      throw std::length_error(fmt::format("4x4 affine matrix read with invalid number of rows ({})", rows.size()));
    }

    for (uint32_t c = 0; c < 4; ++c) {
      for (uint32_t r = 0; r < 4; ++r) {
        matrix[static_cast<int>(c)][static_cast<int>(r)] = rows[r][c];
      }
    }

    return true;
  }
  catch (const std::ios_base::failure& e) {
#if HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR
    // e.code() is only available if the lib actually follows ISO §27.5.3.1.1
    // and derives ios_base::failure from system_error
    spdlog::error("Error #{} on opening file {}: {}", e.code().value(), fileName, e.code().message());

    if (std::make_error_condition(std::io_errc::stream) == e.code()) {
      spdlog::error("Stream error opening file {}", fileName);
    }
    else {
      spdlog::error("Unknown failure opening file {}", fileName);
    }
#else
    logStdErrno();
#endif

    spdlog::error("Failure reading affine transformation from file {}: {}", fileName, e.what());
    return false;
  }
  catch (const std::exception& e) {
    spdlog::error("Invalid 4x4 affine transformation matrix in file {}: {}", fileName, e.what());
    return false;
  }
}

bool saveAffineTxFile(const glm::dmat4& matrix, const fs::path& fileName)
{
  std::ofstream outFile;
  outFile.exceptions(outFile.exceptions() | std::ofstream::badbit | std::ofstream::failbit);

  try {
    outFile.open(fileName, std::ofstream::out);

    if (!outFile) {
      throw std::system_error(errno, std::system_category(), "Failed to open output file " + fileName.string());
    }

    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        outFile << matrix[c][r] << " ";
      }
      outFile << "\n";
    }

    return true;
  }
  catch (const std::ios_base::failure& e) {
#if HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR
    // e.code() is only available if the lib actually follows ISO §27.5.3.1.1
    // and derives ios_base::failure from system_error
    spdlog::error("Error #{} on opening file {}: {}", e.code().value(), fileName, e.code().message());

    if (std::make_error_condition(std::io_errc::stream) == e.code()) {
      spdlog::error("Stream error opening file {}", fileName);
    }
    else {
      spdlog::error("Unknown failure opening file {}", fileName);
    }
#else
    logStdErrno();
#endif

    spdlog::error("Failure writing affine transformation to file {}: {}", fileName, e.what());
    return false;
  }
  catch (const std::exception& e) {
    spdlog::error("Could not write 4x4 affine transformation matrix to file {}: {}", fileName, e.what());
    return false;
  }
}

bool openLandmarkGroupCsvFile(std::map<std::size_t, PointRecord<glm::vec3>>& landmarks, const fs::path& csvFileName)
{
  std::ifstream inFile;
  inFile.exceptions(inFile.exceptions() | std::ifstream::badbit);

  try {
    spdlog::debug("Opening landmarks CSV file {}", csvFileName);
    inFile.open(csvFileName, std::ios_base::in);

    if (!inFile || !inFile.good()) {
      spdlog::error("Error opening landmarks CSV file {}", csvFileName);
      throw std::system_error(errno, std::system_category(), "Failed to open CSV file " + csvFileName.string());
    }

    int lineNum = 1;

    std::string line;
    std::string colName;
    int numCols = 0;

    // Read the first line
    std::getline(inFile, line);
    ++lineNum;

    // SPDLOG_TRACE( "\n\nReading line: {}\n\n\n", line );

    std::istringstream ssHeader(line);

    // Read the column headers into colName (they are not used)
    while (std::getline(ssHeader, colName, ',')) {
      spdlog::debug("Read column name {}", colName);
      ++numCols;
    }

    // The expected columns are (with the last column optional)
    // index ,X ,Y ,Z [,name]
    if (numCols < 4) {
      spdlog::error(
        "Expected at least four columns (id, x, y, z) when reading landmarks CSV file {}, "
        "but only read {} columns",
        csvFileName,
        numCols);
      return false;
    }

    // Is the name column provided?
    const bool nameProvided = (numCols >= 5);

    // Read all lines containing landmark data
    while (std::getline(inFile, line)) {
      // SPDLOG_TRACE( "Reading line: {}", line );

      std::stringstream ssLm(line);

      int landmarkIndex = 0;
      glm::vec3 landmarkPos{0.0f};
      std::optional<std::string> landmarkName;

      int col = 0;
      std::string val;

      while (std::getline(ssLm, val, ',')) {
        // SPDLOG_TRACE( "\tval: {}", val );

        switch (col) {
          case 0: {
            landmarkIndex = std::stoi(val);
            break;
          }
          case 1: {
            landmarkPos.x = std::stof(val);
            break;
          }
          case 2: {
            landmarkPos.y = std::stof(val);
            break;
          }
          case 3: {
            landmarkPos.z = std::stof(val);
            break;
          }
          case 4: {
            if (nameProvided) {
              landmarkName = val;
            }
            break;
          }
          default:
            break; // ignore any more columns
        }

        // If the next token is a comma, ignore it
        if (',' == ssLm.peek()) {
          ssLm.ignore();
        }

        ++col;
      }

      if (nameProvided && (col < numCols - 1)) {
        // The name is optional, so only check col against numCols - 1
        spdlog::error(
          "Line {} of landmarks CSV file {} has {} entries, which is less than the expected {} "
          "entries",
          lineNum,
          csvFileName,
          col,
          numCols - 1);
        return false;
      }
      else if (!nameProvided && (col < numCols)) {
        spdlog::error(
          "Line {} of landmarks CSV file {} has {} entries, which is less than the expected {} "
          "entries",
          lineNum,
          csvFileName,
          col,
          numCols);
        return false;
      }

      //            if ( ! landmarkName )
      //            {
      //                // Assign default name:
      //                std::ostringstream ss;
      //                ss << "LM " << landmarkIndex;
      //                landmarkName = ss.str();
      //            }

      if (landmarkIndex < 0) {
        spdlog::error(
          "Invalid negative landmark index ({}) on line {} of landmarks CSV file {}",
          landmarkIndex,
          lineNum,
          csvFileName);
        return false;
      }

      const auto r = landmarks.try_emplace(static_cast<uint32_t>(landmarkIndex), landmarkPos, *landmarkName);
      if (!r.second) {
        spdlog::warn("Unable to insert landmark '{}', because index {} is already used", *landmarkName, landmarkIndex);
      }

      ++lineNum;
    }

    return true;
  }
  catch (const std::ios_base::failure& e) {
#if HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR
    // e.code() is only available if the lib actually follows ISO §27.5.3.1.1
    // and derives ios_base::failure from system_error
    spdlog::error("Error #{} on opening CSV file {}: {}", e.code().value(), csvFileName, e.code().message());

    if (std::make_error_condition(std::io_errc::stream) == e.code()) {
      spdlog::error("Stream error opening CSV file {}", csvFileName);
    }
    else {
      spdlog::error("Unknown failure opening CSV file {}", csvFileName);
    }
#else
    logStdErrno();
#endif

    spdlog::error("Failure reading landmark CSV file {}: {}", csvFileName, e.what());
    return false;
  }
  catch (const std::exception& e) {
    spdlog::error("Invalid landmark CSV file {}: {}", csvFileName, e.what());
    return false;
  }
}

bool saveLandmarkGroupCsvFile(
  const std::map<std::size_t, PointRecord<glm::vec3>>& landmarks,
  const fs::path& csvFileName)
{
  std::ofstream outFile;
  outFile.exceptions(outFile.exceptions() | std::ofstream::badbit | std::ofstream::failbit);

  try {
    outFile.open(csvFileName, std::ofstream::out);

    if (!outFile) {
      throw std::system_error(errno, std::system_category(), "Failed to open output CSV file " + csvFileName.string());
    }

    static const std::string sk_header("ID,X,Y,Z,Name");

    outFile << sk_header << "\n";

    for (const auto& lm : landmarks) {
      const auto id = lm.first;
      const auto pos = lm.second.getPosition();
      const auto name = lm.second.getName();
      outFile << id << "," << pos.x << "," << pos.y << "," << pos.z << "," << name << "\n";
    }

    return true;
  }
  catch (const std::ios_base::failure& e) {
#if HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR
    // e.code() is only available if the lib actually follows ISO §27.5.3.1.1
    // and derives ios_base::failure from system_error
    spdlog::error("Error #{} on opening CSV file {}: {}", e.code().value(), csvFileName, e.code().message());

    if (std::make_error_condition(std::io_errc::stream) == e.code()) {
      spdlog::error("Stream error opening CSV file {}", csvFileName);
    }
    else {
      spdlog::error("Unknown failure opening CSV file {}", csvFileName);
    }
#else
    logStdErrno();
#endif

    spdlog::error("Failure writing landmarks to CSV file {}: {}", csvFileName, e.what());
    return false;
  }
  catch (const std::exception& e) {
    spdlog::error("Could not write landmarks to CSV file {}: {}", csvFileName, e.what());
    return false;
  }
}

bool openAnnotationsFromJsonFile(std::vector<Annotation>& annots, const fs::path& jsonFileName)
{
  std::ifstream inFile;
  inFile.exceptions(inFile.exceptions() | std::ifstream::badbit);

  try {
    spdlog::debug("Opening annotations JSON file {}", jsonFileName);
    inFile.open(jsonFileName, std::ios_base::in);

    if (!inFile || !inFile.good()) {
      spdlog::error("Error opening annotations JSON file {}", jsonFileName);
      throw std::system_error(errno, std::system_category(), "Failed to open JSON file " + jsonFileName.string());
    }

    json j;
    inFile >> j;

    annots = annotationsFromJson(j);
    spdlog::debug("Parsed {} annotation(s) from JSON:\n{}", annots.size(), j.dump(2));
    return true;
  }
  catch (const std::ios_base::failure& e) {
#if HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR
    // e.code() is only available if the lib actually follows ISO §27.5.3.1.1
    // and derives ios_base::failure from system_error
    spdlog::error(
      "Error #{} on opening annotations JSON file {}: {}",
      e.code().value(),
      jsonFileName,
      e.code().message());

    if (std::make_error_condition(std::io_errc::stream) == e.code()) {
      spdlog::error("Stream error opening annotations JSON file {}", jsonFileName);
    }
    else {
      spdlog::error("Unknown failure opening annotations JSON file {}", jsonFileName);
    }
#else
    logStdErrno();
#endif

    spdlog::error("Failure reading annotations JSON file {}: {}", jsonFileName, e.what());
    return false;
  }
  catch (const std::exception& e) {
    spdlog::error("Invalid annotations JSON file {}: {}", jsonFileName, e.what());
    return false;
  }
}

bool saveToJsonFile(const nlohmann::json& j, const fs::path& jsonFileName)
{
  std::ofstream outFile;
  outFile.exceptions(outFile.exceptions() | std::ofstream::badbit | std::ofstream::failbit);

  try {
    outFile.open(jsonFileName, std::ofstream::out);

    if (!outFile) {
      throw std::system_error(
        errno,
        std::system_category(),
        "Failed to open output JSON file " + jsonFileName.string());
    }

    outFile << j.dump(2);

    spdlog::debug("Saved to JSON file {}:\n{}", jsonFileName, j.dump(2));
    spdlog::info("Saved to JSON file {}", jsonFileName);
    return true;
  }
  catch (const std::ios_base::failure& e) {
#if HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR
    // e.code() is only available if the lib actually follows ISO §27.5.3.1.1
    // and derives ios_base::failure from system_error
    spdlog::error("Error #{} on opening JSON file {}: {}", e.code().value(), jsonFileName, e.code().message());

    if (std::make_error_condition(std::io_errc::stream) == e.code()) {
      spdlog::error("Stream error opening JSON file {}", jsonFileName);
    }
    else {
      spdlog::error("Unknown failure opening JSON file {}", jsonFileName);
    }
#else
    logStdErrno();
#endif

    spdlog::error("Failure writing to JSON file {}: {}", jsonFileName, e.what());
    return false;
  }
  catch (const std::exception& e) {
    spdlog::error("Could not write to JSON file {}: {}", jsonFileName, e.what());
    return false;
  }
}

} // namespace serialize
