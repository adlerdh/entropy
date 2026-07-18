#include "logic/serialization/ProjectSerialization.h"
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
} // namespace

void to_json(json& j, const SurfaceMaterial& material)
{
  j = json{
    {"ambient", material.ambient},
    {"diffuse", material.diffuse},
    {"specular", material.specular},
    {"shininess", material.shininess}};
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
  j = json{
    {"name", surface.name},
    {"value", surface.value},
    {"color", surfaceVec3ToJson(surface.color)},
    {"material", surface.material},
    {"opacity", surface.opacity},
    {"fillOpacity", surface.fillOpacity},
    {"visible", surface.visible},
    {"showIn2d", surface.showIn2d},
    {"rimLightingEnabled", surface.rimLightingEnabled},
    {"rimOpacityStrength", surface.rimOpacityStrength},
    {"rimEmissionStrength", surface.rimEmissionStrength},
    {"rimPower", surface.rimPower}};
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
  if (const auto value = j.find("fillOpacity"); value != j.end() && value->is_number()) {
    surface.fillOpacity = value->get<float>();
  }
  if (const auto value = j.find("visible"); value != j.end() && value->is_boolean()) {
    surface.visible = value->get<bool>();
  }
  if (const auto value = j.find("showIn2d"); value != j.end() && value->is_boolean()) {
    surface.showIn2d = value->get<bool>();
  }
  if (const auto value = j.find("rimLightingEnabled"); value != j.end() && value->is_boolean()) {
    surface.rimLightingEnabled = value->get<bool>();
  }
  if (const auto value = j.find("rimOpacityStrength"); value != j.end() && value->is_number()) {
    surface.rimOpacityStrength = value->get<float>();
  }
  if (const auto value = j.find("rimEmissionStrength"); value != j.end() && value->is_number()) {
    surface.rimEmissionStrength = value->get<float>();
  }
  if (const auto value = j.find("rimPower"); value != j.end() && value->is_number()) {
    surface.rimPower = value->get<float>();
  }
  else if (const auto legacy = j.find("edgeStrength"); legacy != j.end() && legacy->is_number()) {
    const float edgeStrength = legacy->get<float>();
    surface.rimLightingEnabled = edgeStrength > 0.0f;
    surface.rimOpacityStrength = 1.0f;
    surface.rimEmissionStrength = 0.0f;
    surface.rimPower = std::max(edgeStrength, 0.25f);
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

// Convert all project image file names to be relative to the project base path and canonical.
// void makePathsRelative( json& project, const fs::path& basePath )
//{
//     auto makeRelative = [&basePath] ( json& image )
//     {
//         fs::path refImagePath = image.at( "imageFileName" ).get<std::string>();
//         if ( refImagePath.is_relative() )
//         {
//             image.at( "imageFileName" ) = fs::relative( refImagePath, basePath ).string();
//         }

//        if ( image.count( "segFileName" ) > 0 )
//        {
//            fs::path refSegPath = image.at( "segFileName" ).get<std::string>();
//            if ( refSegPath.is_relative() )
//            {
//                image.at( "segFileName" ) = fs::relative( refSegPath, basePath ).string();
//            }
//        }
//    };

//    makeRelative( project.at( "referenceImage" ) );

//    for ( auto& image : project.at( "additionalImages" ) )
//    {
//        makeRelative( image );
//    }
//}

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

std::vector<const char*> interpolationModesToJson(const std::vector<InterpolationMode>& modes)
{
  std::vector<const char*> names;
  names.reserve(modes.size());
  for (const InterpolationMode mode : modes) {
    names.push_back(enumToName(mode, k_interpolationModeNames));
  }
  return names;
}

std::vector<InterpolationMode> interpolationModesFromJson(const json& j)
{
  std::vector<InterpolationMode> modes;
  if (!j.is_array()) {
    return modes;
  }

  modes.reserve(j.size());
  for (const auto& value : j) {
    if (!value.is_string()) {
      continue;
    }
    if (const auto parsed = enumFromName<InterpolationMode>(value.get<std::string>(), k_interpolationModeNames)) {
      modes.push_back(*parsed);
    }
  }
  return modes;
}

std::vector<glm::vec3> vec3ArrayFromJson(const json& j)
{
  std::vector<glm::vec3> values;
  if (!j.is_array()) {
    return values;
  }

  values.reserve(j.size());
  for (const auto& value : j) {
    if (value.is_array() && value.size() == 3) {
      values.push_back(vec3FromJson(value));
    }
  }
  return values;
}

std::string optionalPathToString(const std::optional<fs::path>& path)
{
  return path ? path->generic_string() : std::string{};
}

std::vector<std::string> pathVectorToStrings(const std::vector<fs::path>& paths)
{
  std::vector<std::string> values;
  values.reserve(paths.size());
  for (const fs::path& path : paths) {
    values.push_back(path.generic_string());
  }
  return values;
}

std::vector<fs::path> pathVectorFromJson(const json& j)
{
  std::vector<fs::path> paths;
  if (!j.is_array()) {
    return paths;
  }

  paths.reserve(j.size());
  for (const auto& value : j) {
    if (value.is_string()) {
      paths.emplace_back(value.get<std::string>());
    }
  }
  return paths;
}

void optionalPathFromJson(const json& j, const char* key, std::optional<fs::path>& path)
{
  if (const auto value = j.find(key); value != j.end() && value->is_string()) {
    const std::string parsed = value->get<std::string>();
    if (!parsed.empty()) {
      path = parsed;
    }
  }
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

} // namespace

namespace imageio
{

// Define serialization of InterpolationMode to JSON as strings.
// NLOHMANN_JSON_SERIALIZE_ENUM(
//         ImageSettings::InterpolationMode, {
//             { ImageSettings::InterpolationMode::Linear, "Linear" },
//             { ImageSettings::InterpolationMode::NearestNeighbor, "NearestNeighbor" }
//         } );

} // namespace imageio

namespace serialize
{

void to_json(json& j, const serialize::ImageSettings& settings)
{
  j = json{
    {"displayName", settings.m_displayName},
    {"globalVisibility", settings.m_globalVisibility},
    {"globalOpacity", settings.m_globalOpacity},
    {"borderColor", vec3ToJson(settings.m_borderColor)},
    {"lockedToReference", settings.m_lockedToReference},
    {"warpEnabled", settings.m_warpEnabled},
    {"warpStrength", settings.m_warpStrength},
    {"allowExaggeratedWarp", settings.m_allowExaggeratedWarp},
    {"level", settings.m_level},
    {"window", settings.m_window},
    {"thresholdLow", settings.m_thresholdLow},
    {"thresholdHigh", settings.m_thresholdHigh},
    {"opacity", settings.m_opacity},
    {"activeComponent", settings.m_activeComponent},
    {"activeTimePoint", settings.m_activeTimePoint},
    {"timePlaybackLoop", settings.m_timePlaybackLoop},
    {"timePlaybackPlaying", settings.m_timePlaybackPlaying},
    {"timePlaybackSpeed", settings.m_timePlaybackSpeed},
    {"componentRenderMode", enumToName(settings.m_componentRenderMode, k_componentRenderModeNames)},
    {"complexPhaseUnit", enumToName(settings.m_complexPhaseUnit, k_complexPhaseUnitNames)},
    {"complexPhaseRange", enumToName(settings.m_complexPhaseRange, k_complexPhaseRangeNames)},
    {"vectorArrowOverlayVisible", settings.m_vectorArrowOverlayVisible},
    {"vectorArrowOverlayOnImage", settings.m_vectorArrowOverlayOnImage},
    {"vectorArrowOverlayDensity", settings.m_vectorArrowOverlayDensity},
    {"vectorArrowOverlayVoxelSpacing", settings.m_vectorArrowOverlayVoxelSpacing},
    {"vectorArrowOverlayMillimeterSpacing", settings.m_vectorArrowOverlayMillimeterSpacing},
    {"vectorArrowOverlaySpacingMode",
     enumToName(settings.m_vectorArrowOverlaySpacingMode, k_vectorArrowOverlaySpacingModeNames)},
    {"vectorArrowOverlayColor", vec3ToJson(settings.m_vectorArrowOverlayColor)},
    {"vectorArrowOverlayUseDirectionColor", settings.m_vectorArrowOverlayUseDirectionColor},
    {"vectorArrowOverlayLineThickness", settings.m_vectorArrowOverlayLineThickness},
    {"vectorArrowOverlayOpacity", settings.m_vectorArrowOverlayOpacity},
    {"vectorArrowOverlayScaleByMagnitude", settings.m_vectorArrowOverlayScaleByMagnitude},
    {"vectorArrowOverlayScaleFactor", settings.m_vectorArrowOverlayScaleFactor},
    {"vectorWarpedGridVisible", settings.m_vectorWarpedGridVisible},
    {"vectorWarpedGridOverlayOnImage", settings.m_vectorWarpedGridOverlayOnImage},
    {"vectorWarpedGridConvention",
     enumToName(settings.m_vectorWarpedGridConvention, k_vectorWarpedGridConventionNames)},
    {"vectorWarpedGridPixelSpacing", settings.m_vectorWarpedGridPixelSpacing},
    {"vectorWarpedGridVoxelSpacing", settings.m_vectorWarpedGridVoxelSpacing},
    {"vectorWarpedGridMillimeterSpacing", settings.m_vectorWarpedGridMillimeterSpacing},
    {"vectorWarpedGridSpacingMode",
     enumToName(settings.m_vectorWarpedGridSpacingMode, k_vectorArrowOverlaySpacingModeNames)},
    {"vectorWarpedGridLineThickness", settings.m_vectorWarpedGridLineThickness},
    {"vectorWarpedGridScaleFactor", settings.m_vectorWarpedGridScaleFactor},
    {"vectorWarpedGridForegroundColor", vec4ToJson(settings.m_vectorWarpedGridForegroundColor)},
    {"vectorWarpedGridBackgroundColor", vec4ToJson(settings.m_vectorWarpedGridBackgroundColor)},
    {"vectorPlanarProjectionSignedColors", settings.m_vectorPlanarProjectionSignedColors},
    {"vectorLogJacobianDeterminant", settings.m_vectorLogJacobianDeterminant},
    {"ignoreAlpha", settings.m_ignoreAlpha},
    {"colorInterpolationMode", enumToName(settings.m_colorInterpolationMode, k_interpolationModeNames)},
    {"edgeDetectionMethod", enumToName(settings.m_edgeDetectionMethod, k_edgeDetectionMethodNames)},
    {"showEdges", settings.m_showEdges},
    {"hardEdges", settings.m_thresholdEdges},
    {"thinPixelEdges", settings.m_thinPixelEdges},
    {"overlayEdges", settings.m_overlayEdges},
    {"edgeMagnitude", settings.m_edgeMagnitude},
    {"pixelEdgeScale", settings.m_pixelEdgeScale},
    {"pixelEdgeThreshold", settings.m_pixelEdgeThreshold},
    {"edgeColor", {settings.m_edgeColor.r, settings.m_edgeColor.g, settings.m_edgeColor.b}},
    {"edgeOpacity", settings.m_edgeOpacity},
    {"useDistanceMapForRaycasting", settings.m_useDistanceMapForRaycasting},
    {"isosurfacesVisible", settings.m_isosurfacesVisible},
    {"applyImageColormapToIsosurfaces", settings.m_applyImageColormapToIsosurfaces},
    {"showIsocontoursIn2D", settings.m_showIsocontoursIn2D},
    {"isocontourLineWidthIn2D", settings.m_isocontourLineWidthIn2D},
    {"isosurfaceOpacityModulator", settings.m_isosurfaceOpacityModulator}};

  if (!settings.m_componentLevels.empty()) {
    j["componentLevels"] = settings.m_componentLevels;
  }
  if (!settings.m_componentWindows.empty()) {
    j["componentWindows"] = settings.m_componentWindows;
  }
  if (!settings.m_componentThresholdLows.empty()) {
    j["componentThresholdLows"] = settings.m_componentThresholdLows;
  }
  if (!settings.m_componentThresholdHighs.empty()) {
    j["componentThresholdHighs"] = settings.m_componentThresholdHighs;
  }
  if (!settings.m_componentVisibility.empty()) {
    j["componentVisibility"] = settings.m_componentVisibility;
  }
  if (!settings.m_componentOpacities.empty()) {
    j["componentOpacities"] = settings.m_componentOpacities;
  }
  if (!settings.m_colorMapIndices.empty()) {
    j["colorMapIndices"] = settings.m_colorMapIndices;
  }
  if (!settings.m_colorMapInverted.empty()) {
    j["colorMapInverted"] = settings.m_colorMapInverted;
  }
  if (!settings.m_colorMapContinuous.empty()) {
    j["colorMapContinuous"] = settings.m_colorMapContinuous;
  }
  if (!settings.m_colorMapLevels.empty()) {
    j["colorMapLevels"] = settings.m_colorMapLevels;
  }
  if (!settings.m_colorMapHsvModifiers.empty()) {
    j["colorMapHsvModifiers"] = json::array();
    for (const glm::vec3& value : settings.m_colorMapHsvModifiers) {
      j["colorMapHsvModifiers"].push_back(vec3ToJson(value));
    }
  }
  if (!settings.m_interpolationModes.empty()) {
    j["interpolationModes"] = interpolationModesToJson(settings.m_interpolationModes);
  }
  if (!settings.m_foregroundThresholdLows.empty()) {
    j["foregroundThresholdLows"] = settings.m_foregroundThresholdLows;
  }
  if (!settings.m_foregroundThresholdHighs.empty()) {
    j["foregroundThresholdHighs"] = settings.m_foregroundThresholdHighs;
  }

  if (serialize::ProjectEdgeDetectionMethod::Voxel == settings.m_edgeDetectionMethod) {
    j["colormapEdges"] = settings.m_colormapEdges;
  }
}

void from_json(const json& j, serialize::ImageSettings& settings)
{
  if (j.count("displayName")) {
    j.at("displayName").get_to(settings.m_displayName);
  }
  if (j.count("globalVisibility")) {
    j.at("globalVisibility").get_to(settings.m_globalVisibility);
  }
  if (j.count("globalOpacity")) {
    j.at("globalOpacity").get_to(settings.m_globalOpacity);
  }
  if (const auto color = j.find("borderColor"); color != j.end() && color->is_array() && color->size() == 3) {
    settings.m_borderColor = vec3FromJson(*color);
  }
  if (j.count("lockedToReference")) {
    j.at("lockedToReference").get_to(settings.m_lockedToReference);
  }
  if (j.count("warpEnabled")) {
    j.at("warpEnabled").get_to(settings.m_warpEnabled);
  }
  if (j.count("warpStrength")) {
    j.at("warpStrength").get_to(settings.m_warpStrength);
  }
  if (j.count("allowExaggeratedWarp")) {
    j.at("allowExaggeratedWarp").get_to(settings.m_allowExaggeratedWarp);
  }
  if (j.count("level")) {
    j.at("level").get_to(settings.m_level);
  }
  if (j.count("window")) {
    j.at("window").get_to(settings.m_window);
  }
  if (j.count("thresholdLow")) {
    j.at("thresholdLow").get_to(settings.m_thresholdLow);
  }
  if (j.count("thresholdHigh")) {
    j.at("thresholdHigh").get_to(settings.m_thresholdHigh);
  }
  if (j.count("opacity")) {
    j.at("opacity").get_to(settings.m_opacity);
  }
  if (const auto activeComponent = unsignedIntFromJson(j.value("activeComponent", json{}))) {
    settings.m_activeComponent = *activeComponent;
  }
  if (const auto activeTimePoint = unsignedIntFromJson(j.value("activeTimePoint", json{}))) {
    settings.m_activeTimePoint = *activeTimePoint;
  }
  if (j.count("timePlaybackLoop")) {
    j.at("timePlaybackLoop").get_to(settings.m_timePlaybackLoop);
  }
  if (j.count("timePlaybackPlaying")) {
    j.at("timePlaybackPlaying").get_to(settings.m_timePlaybackPlaying);
  }
  if (j.count("timePlaybackSpeed")) {
    j.at("timePlaybackSpeed").get_to(settings.m_timePlaybackSpeed);
  }
  if (
    const auto parsed = enumFromName<serialize::ProjectComponentRenderMode>(
      j.value("componentRenderMode", ""),
      k_componentRenderModeNames))
  {
    settings.m_componentRenderMode = *parsed;
  }
  if (
    const auto parsed =
      enumFromName<serialize::ProjectComplexPhaseUnit>(j.value("complexPhaseUnit", ""), k_complexPhaseUnitNames))
  {
    settings.m_complexPhaseUnit = *parsed;
  }
  if (
    const auto parsed =
      enumFromName<serialize::ProjectComplexPhaseRange>(j.value("complexPhaseRange", ""), k_complexPhaseRangeNames))
  {
    settings.m_complexPhaseRange = *parsed;
  }
  if (j.count("vectorArrowOverlayVisible")) {
    j.at("vectorArrowOverlayVisible").get_to(settings.m_vectorArrowOverlayVisible);
  }
  if (j.count("vectorArrowOverlayOnImage")) {
    j.at("vectorArrowOverlayOnImage").get_to(settings.m_vectorArrowOverlayOnImage);
  }
  if (j.count("vectorArrowOverlayDensity")) {
    j.at("vectorArrowOverlayDensity").get_to(settings.m_vectorArrowOverlayDensity);
  }
  if (j.count("vectorArrowOverlayVoxelSpacing")) {
    j.at("vectorArrowOverlayVoxelSpacing").get_to(settings.m_vectorArrowOverlayVoxelSpacing);
  }
  if (j.count("vectorArrowOverlayMillimeterSpacing")) {
    j.at("vectorArrowOverlayMillimeterSpacing").get_to(settings.m_vectorArrowOverlayMillimeterSpacing);
  }
  if (
    const auto parsed = enumFromName<serialize::ProjectVectorArrowOverlaySpacingMode>(
      j.value("vectorArrowOverlaySpacingMode", ""),
      k_vectorArrowOverlaySpacingModeNames))
  {
    settings.m_vectorArrowOverlaySpacingMode = *parsed;
  }
  if (const auto color = j.find("vectorArrowOverlayColor"); color != j.end() && color->is_array() && color->size() == 3)
  {
    settings.m_vectorArrowOverlayColor = vec3FromJson(*color);
  }
  if (j.count("vectorArrowOverlayUseDirectionColor")) {
    j.at("vectorArrowOverlayUseDirectionColor").get_to(settings.m_vectorArrowOverlayUseDirectionColor);
  }
  if (j.count("vectorArrowOverlayLineThickness")) {
    j.at("vectorArrowOverlayLineThickness").get_to(settings.m_vectorArrowOverlayLineThickness);
  }
  if (j.count("vectorArrowOverlayOpacity")) {
    j.at("vectorArrowOverlayOpacity").get_to(settings.m_vectorArrowOverlayOpacity);
  }
  if (j.count("vectorArrowOverlayScaleByMagnitude")) {
    j.at("vectorArrowOverlayScaleByMagnitude").get_to(settings.m_vectorArrowOverlayScaleByMagnitude);
  }
  if (j.count("vectorArrowOverlayScaleFactor")) {
    j.at("vectorArrowOverlayScaleFactor").get_to(settings.m_vectorArrowOverlayScaleFactor);
  }
  if (j.count("vectorWarpedGridVisible")) {
    j.at("vectorWarpedGridVisible").get_to(settings.m_vectorWarpedGridVisible);
  }
  if (j.count("vectorWarpedGridOverlayOnImage")) {
    j.at("vectorWarpedGridOverlayOnImage").get_to(settings.m_vectorWarpedGridOverlayOnImage);
  }
  if (
    const auto parsed = enumFromName<serialize::ProjectVectorWarpedGridConvention>(
      j.value("vectorWarpedGridConvention", ""),
      k_vectorWarpedGridConventionNames))
  {
    settings.m_vectorWarpedGridConvention = *parsed;
  }
  if (j.count("vectorWarpedGridPixelSpacing")) {
    j.at("vectorWarpedGridPixelSpacing").get_to(settings.m_vectorWarpedGridPixelSpacing);
  }
  if (j.count("vectorWarpedGridVoxelSpacing")) {
    j.at("vectorWarpedGridVoxelSpacing").get_to(settings.m_vectorWarpedGridVoxelSpacing);
  }
  if (j.count("vectorWarpedGridMillimeterSpacing")) {
    j.at("vectorWarpedGridMillimeterSpacing").get_to(settings.m_vectorWarpedGridMillimeterSpacing);
  }
  if (
    const auto parsed = enumFromName<serialize::ProjectVectorArrowOverlaySpacingMode>(
      j.value("vectorWarpedGridSpacingMode", ""),
      k_vectorArrowOverlaySpacingModeNames))
  {
    settings.m_vectorWarpedGridSpacingMode = *parsed;
  }
  if (j.count("vectorWarpedGridLineThickness")) {
    j.at("vectorWarpedGridLineThickness").get_to(settings.m_vectorWarpedGridLineThickness);
  }
  if (j.count("vectorWarpedGridScaleFactor")) {
    j.at("vectorWarpedGridScaleFactor").get_to(settings.m_vectorWarpedGridScaleFactor);
  }
  if (const auto color = j.find("vectorWarpedGridForegroundColor");
      color != j.end() && color->is_array() && color->size() == 4)
  {
    settings.m_vectorWarpedGridForegroundColor = vec4FromJson(*color);
  }
  if (const auto color = j.find("vectorWarpedGridBackgroundColor");
      color != j.end() && color->is_array() && color->size() == 4)
  {
    settings.m_vectorWarpedGridBackgroundColor = vec4FromJson(*color);
  }
  if (j.count("vectorPlanarProjectionSignedColors")) {
    j.at("vectorPlanarProjectionSignedColors").get_to(settings.m_vectorPlanarProjectionSignedColors);
  }
  if (j.count("vectorLogJacobianDeterminant")) {
    j.at("vectorLogJacobianDeterminant").get_to(settings.m_vectorLogJacobianDeterminant);
  }
  if (j.count("ignoreAlpha")) {
    j.at("ignoreAlpha").get_to(settings.m_ignoreAlpha);
  }
  if (
    const auto parsed =
      enumFromName<InterpolationMode>(j.value("colorInterpolationMode", ""), k_interpolationModeNames))
  {
    settings.m_colorInterpolationMode = *parsed;
  }
  if (j.count("componentLevels")) {
    j.at("componentLevels").get_to(settings.m_componentLevels);
  }
  if (j.count("componentWindows")) {
    j.at("componentWindows").get_to(settings.m_componentWindows);
  }
  if (j.count("componentThresholdLows")) {
    j.at("componentThresholdLows").get_to(settings.m_componentThresholdLows);
  }
  if (j.count("componentThresholdHighs")) {
    j.at("componentThresholdHighs").get_to(settings.m_componentThresholdHighs);
  }
  if (j.count("componentVisibility")) {
    j.at("componentVisibility").get_to(settings.m_componentVisibility);
  }
  if (j.count("componentOpacities")) {
    j.at("componentOpacities").get_to(settings.m_componentOpacities);
  }
  if (j.count("colorMapIndices")) {
    j.at("colorMapIndices").get_to(settings.m_colorMapIndices);
  }
  if (j.count("colorMapInverted")) {
    j.at("colorMapInverted").get_to(settings.m_colorMapInverted);
  }
  if (j.count("colorMapContinuous")) {
    j.at("colorMapContinuous").get_to(settings.m_colorMapContinuous);
  }
  if (j.count("colorMapLevels")) {
    j.at("colorMapLevels").get_to(settings.m_colorMapLevels);
  }
  if (const auto modifiers = j.find("colorMapHsvModifiers"); modifiers != j.end()) {
    settings.m_colorMapHsvModifiers = vec3ArrayFromJson(*modifiers);
  }
  if (const auto modes = j.find("interpolationModes"); modes != j.end()) {
    settings.m_interpolationModes = interpolationModesFromJson(*modes);
  }
  if (j.count("foregroundThresholdLows")) {
    j.at("foregroundThresholdLows").get_to(settings.m_foregroundThresholdLows);
  }
  if (j.count("foregroundThresholdHighs")) {
    j.at("foregroundThresholdHighs").get_to(settings.m_foregroundThresholdHighs);
  }
  if (
    const auto parsed = enumFromName<serialize::ProjectEdgeDetectionMethod>(
      j.value("edgeDetectionMethod", ""),
      k_edgeDetectionMethodNames))
  {
    settings.m_edgeDetectionMethod = *parsed;
  }
  if (j.count("showEdges")) {
    j.at("showEdges").get_to(settings.m_showEdges);
  }
  if (j.count("hardEdges")) {
    j.at("hardEdges").get_to(settings.m_thresholdEdges);
  }
  if (j.count("thinPixelEdges")) {
    j.at("thinPixelEdges").get_to(settings.m_thinPixelEdges);
  }
  if (j.count("overlayEdges")) {
    j.at("overlayEdges").get_to(settings.m_overlayEdges);
  }
  if (serialize::ProjectEdgeDetectionMethod::Voxel == settings.m_edgeDetectionMethod && j.count("colormapEdges")) {
    j.at("colormapEdges").get_to(settings.m_colormapEdges);
  }
  if (j.count("edgeMagnitude")) {
    j.at("edgeMagnitude").get_to(settings.m_edgeMagnitude);
  }
  if (j.count("pixelEdgeScale")) {
    j.at("pixelEdgeScale").get_to(settings.m_pixelEdgeScale);
  }
  if (j.count("pixelEdgeThreshold")) {
    j.at("pixelEdgeThreshold").get_to(settings.m_pixelEdgeThreshold);
  }
  if (const auto color = j.find("edgeColor"); color != j.end() && color->is_array() && color->size() == 3) {
    settings.m_edgeColor = glm::vec3{color->at(0).get<float>(), color->at(1).get<float>(), color->at(2).get<float>()};
  }
  if (j.count("edgeOpacity")) {
    j.at("edgeOpacity").get_to(settings.m_edgeOpacity);
  }
  if (j.count("useDistanceMapForRaycasting")) {
    j.at("useDistanceMapForRaycasting").get_to(settings.m_useDistanceMapForRaycasting);
  }
  if (j.count("isosurfacesVisible")) {
    j.at("isosurfacesVisible").get_to(settings.m_isosurfacesVisible);
  }
  if (j.count("applyImageColormapToIsosurfaces")) {
    j.at("applyImageColormapToIsosurfaces").get_to(settings.m_applyImageColormapToIsosurfaces);
  }
  if (j.count("showIsocontoursIn2D")) {
    j.at("showIsocontoursIn2D").get_to(settings.m_showIsocontoursIn2D);
  }
  if (j.count("isocontourLineWidthIn2D")) {
    j.at("isocontourLineWidthIn2D").get_to(settings.m_isocontourLineWidthIn2D);
  }
  if (j.count("isosurfaceOpacityModulator")) {
    j.at("isosurfaceOpacityModulator").get_to(settings.m_isosurfaceOpacityModulator);
  }
}

void to_json(json& j, const serialize::SegSettings& settings)
{
  j = json{
    {"displayName", settings.m_displayName},
    {"visibility", settings.m_visibility},
    {"opacity", settings.m_opacity},
    {"activeComponent", settings.m_activeComponent}};

  if (!settings.m_componentVisibility.empty()) {
    j["componentVisibility"] = settings.m_componentVisibility;
  }
  if (!settings.m_componentOpacities.empty()) {
    j["componentOpacities"] = settings.m_componentOpacities;
  }
  if (!settings.m_labelTableIndices.empty()) {
    j["labelTableIndices"] = settings.m_labelTableIndices;
  }
  if (!settings.m_interpolationModes.empty()) {
    j["interpolationModes"] = interpolationModesToJson(settings.m_interpolationModes);
  }
}

void from_json(const json& j, serialize::SegSettings& settings)
{
  if (j.count("displayName")) {
    j.at("displayName").get_to(settings.m_displayName);
  }
  if (j.count("visibility")) {
    j.at("visibility").get_to(settings.m_visibility);
  }
  if (j.count("opacity")) {
    j.at("opacity").get_to(settings.m_opacity);
  }
  if (const auto activeComponent = unsignedIntFromJson(j.value("activeComponent", json{}))) {
    settings.m_activeComponent = *activeComponent;
  }
  if (j.count("componentVisibility")) {
    j.at("componentVisibility").get_to(settings.m_componentVisibility);
  }
  if (j.count("componentOpacities")) {
    j.at("componentOpacities").get_to(settings.m_componentOpacities);
  }
  if (j.count("labelTableIndices")) {
    j.at("labelTableIndices").get_to(settings.m_labelTableIndices);
  }
  if (const auto modes = j.find("interpolationModes"); modes != j.end()) {
    settings.m_interpolationModes = interpolationModesFromJson(*modes);
  }
}

void to_json(json& j, const serialize::Segmentation& seg)
{
  j = json{{"path", seg.m_segFileName.string()}};

  if (seg.m_settings) {
    j["settings"] = *seg.m_settings;
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

void to_json(json& j, const serialize::LandmarkGroup& landmarks)
{
  j = json{{"path", landmarks.m_csvFileName}, {"inVoxelSpace", landmarks.m_inVoxelSpace}};
}

void from_json(const json& j, serialize::LandmarkGroup& landmarks)
{
  j.at("path").get_to(landmarks.m_csvFileName);

  if (j.count("inVoxelSpace")) {
    landmarks.m_inVoxelSpace = j.at("inVoxelSpace").get<bool>();
  }
  else {
    // If not defined, then assume false
    landmarks.m_inVoxelSpace = false;
  }
}

void to_json(json& j, const serialize::ImageIsosurface& isosurface)
{
  j = json{{"component", isosurface.m_component}, {"surface", isosurface.m_surface}};
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
  j = json{{"seriesInstanceUid", source.m_seriesInstanceUid}};

  if (!source.m_rootPath.empty()) {
    j["root"] = source.m_rootPath.string();
  }

  if (!source.m_studyInstanceUid.empty()) {
    j["studyInstanceUid"] = source.m_studyInstanceUid;
  }

  if (!source.m_files.empty()) {
    std::vector<std::string> files;
    files.reserve(source.m_files.size());
    for (const auto& file : source.m_files) {
      files.push_back(file.string());
    }
    j["files"] = files;
  }
}

void from_json(const json& j, serialize::DicomSource& source)
{
  if (j.count("root")) {
    source.m_rootPath = j.at("root").get<std::string>();
  }
  if (j.count("studyInstanceUid")) {
    source.m_studyInstanceUid = j.at("studyInstanceUid").get<std::string>();
  }
  if (j.count("seriesInstanceUid")) {
    source.m_seriesInstanceUid = j.at("seriesInstanceUid").get<std::string>();
  }
  if (j.count("files")) {
    const auto files = j.at("files").get<std::vector<std::string>>();
    source.m_files.clear();
    source.m_files.reserve(files.size());
    for (const auto& file : files) {
      source.m_files.emplace_back(file);
    }
  }
}

void to_json(json& j, const serialize::Image& image)
{
  j = json{{"image", image.m_imageFileName.string()}};

  if (image.m_spatialMetadata) {
    j["spatialMetadata"] = {
      {"spacingMm", vec3ToJson(image.m_spatialMetadata->spacingMm)},
      {"originMm", vec3ToJson(image.m_spatialMetadata->originMm)},
      {"directions", mat3ToJson(image.m_spatialMetadata->directions)}};
  }

  if (image.m_dicomSource) {
    j["dicomSource"] = *image.m_dicomSource;
  }

  if (image.m_affineTxFileName) {
    j["affine"] = image.m_affineTxFileName->string();
  }

  if (image.m_inverseWarpFileName) {
    j["inverseWarp"] = image.m_inverseWarpFileName->string();
  }

  if (image.m_inverseWarpReferenceImageFileName) {
    j["inverseWarpReferenceImage"] = image.m_inverseWarpReferenceImageFileName->string();
  }

  if (image.m_forwardWarpFileName) {
    j["forwardWarp"] = image.m_forwardWarpFileName->string();
  }

  if (image.m_worldDefTx) {
    j["manualTransformation"] = matrixToJson(*image.m_worldDefTx);
  }

  if (image.m_annotationsFileName) {
    j["annotations"] = image.m_annotationsFileName->string();
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
    j["settings"] = *image.m_settings;
  }
}

void from_json(const json& j, serialize::Image& image)
{
  std::string p;
  j.at("image").get_to(p);
  image.m_imageFileName = p;

  if (const auto metadata = j.find("spatialMetadata"); metadata != j.end() && metadata->is_object()) {
    ImageSpatialMetadata spatialMetadata;
    spatialMetadata.spacingMm = vec3FromJson(metadata->at("spacingMm"));
    spatialMetadata.originMm = vec3FromJson(metadata->at("originMm"));
    spatialMetadata.directions = mat3FromJson(metadata->at("directions"));
    image.m_spatialMetadata = spatialMetadata;
  }

  if (j.count("dicomSource")) {
    image.m_dicomSource = j.at("dicomSource").get<serialize::DicomSource>();
  }

  if (j.count("affine")) {
    image.m_affineTxFileName = j.at("affine").get<std::string>();
  }

  if (j.count("inverseWarp")) {
    image.m_inverseWarpFileName = j.at("inverseWarp").get<std::string>();
  }

  if (j.count("inverseWarpReferenceImage")) {
    image.m_inverseWarpReferenceImageFileName = j.at("inverseWarpReferenceImage").get<std::string>();
  }

  if (j.count("forwardWarp")) {
    image.m_forwardWarpFileName = j.at("forwardWarp").get<std::string>();
  }

  if (j.count("manualTransformation")) {
    image.m_worldDefTx = matrixFromJson(j.at("manualTransformation"));
  }

  if (j.count("annotations")) {
    image.m_annotationsFileName = j.at("annotations").get<std::string>();
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
  j = json{{"synchronizeTimeSeries", settings.m_synchronizeTimeSeries}};
}

void from_json(const json& j, ProjectInterfaceSettings& settings)
{
  if (const auto syncTime = j.find("synchronizeTimeSeries"); syncTime != j.end() && syncTime->is_boolean()) {
    settings.m_synchronizeTimeSeries = syncTime->get<bool>();
  }
}

void to_json(json& j, const ProjectViewSettings& settings)
{
  j = json{
    {"showImageBorders", settings.m_showImageBorders},
    {"showImageBordersInLightboxViews", settings.m_showImageBordersInLightboxViews},
    {"showCrosshairs", settings.m_showCrosshairs},
    {"showCrosshairsInLightboxViews", settings.m_showCrosshairsInLightboxViews},
    {"showAnatomicalLabels", settings.m_showAnatomicalLabels},
    {"showAnatomicalLabelsInLightboxViews", settings.m_showAnatomicalLabelsInLightboxViews},
    {"showScaleBars", settings.m_showScaleBars},
    {"showScaleBarsInLightboxViews", settings.m_showScaleBarsInLightboxViews},
    {"anatomicalLabelType", enumToName(settings.m_anatomicalLabelType, k_anatomicalLabelNames)},
    {"lockAnatomicalDirectionsToReferenceImage", settings.m_lockAnatomicalDirectionsToReferenceImage},
    {"crosshairsSnapping", enumToName(settings.m_crosshairsSnapping, k_crosshairsSnappingNames)}};
}

void from_json(const json& j, ProjectViewSettings& settings)
{
  if (const auto value = j.find("showImageBorders"); value != j.end() && value->is_boolean()) {
    settings.m_showImageBorders = value->get<bool>();
  }
  if (const auto value = j.find("showImageBordersInLightboxViews"); value != j.end() && value->is_boolean()) {
    settings.m_showImageBordersInLightboxViews = value->get<bool>();
  }
  if (const auto value = j.find("showCrosshairs"); value != j.end() && value->is_boolean()) {
    settings.m_showCrosshairs = value->get<bool>();
  }
  if (const auto value = j.find("showCrosshairsInLightboxViews"); value != j.end() && value->is_boolean()) {
    settings.m_showCrosshairsInLightboxViews = value->get<bool>();
  }
  if (const auto value = j.find("showAnatomicalLabels"); value != j.end() && value->is_boolean()) {
    settings.m_showAnatomicalLabels = value->get<bool>();
  }
  if (const auto value = j.find("showAnatomicalLabelsInLightboxViews"); value != j.end() && value->is_boolean()) {
    settings.m_showAnatomicalLabelsInLightboxViews = value->get<bool>();
  }
  if (const auto value = j.find("showScaleBars"); value != j.end() && value->is_boolean()) {
    settings.m_showScaleBars = value->get<bool>();
  }
  if (const auto value = j.find("showScaleBarsInLightboxViews"); value != j.end() && value->is_boolean()) {
    settings.m_showScaleBarsInLightboxViews = value->get<bool>();
  }
  if (const auto parsed = enumFromName<AnatomicalLabelType>(j.value("anatomicalLabelType", ""), k_anatomicalLabelNames))
  {
    settings.m_anatomicalLabelType = *parsed;
    if (AnatomicalLabelType::Disabled == settings.m_anatomicalLabelType) {
      settings.m_showAnatomicalLabels = false;
      settings.m_anatomicalLabelType = AnatomicalLabelType::Human;
    }
  }
  if (const auto lock = j.find("lockAnatomicalDirectionsToReferenceImage"); lock != j.end() && lock->is_boolean()) {
    settings.m_lockAnatomicalDirectionsToReferenceImage = lock->get<bool>();
  }
  if (
    const auto parsed = enumFromName<CrosshairsSnapping>(j.value("crosshairsSnapping", ""), k_crosshairsSnappingNames))
  {
    settings.m_crosshairsSnapping = *parsed;
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
  j = json{
    {"colormapIndex", settings.m_colorMapIndex},
    {"windowSlopeIntercept", vec2ToJson(settings.m_slopeIntercept)},
    {"invertColormap", settings.m_invertColormap},
    {"continuousColormap", settings.m_continuousColormap},
    {"colormapLevels", settings.m_colormapLevels}};
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
  j = json{{"squared", settings.m_squared}, {"metric", settings.m_metric}};
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
  j = json{
    {"metric", settings.m_metric},
    {"presentation", enumToName(settings.m_presentation, k_localNccPresentationNames)},
    {"negativeCorrelationAsMismatch", settings.m_negativeCorrelationAsMismatch},
    {"patchRadius", settings.m_patchRadius},
    {"sampleSpacing", settings.m_sampleSpacing},
    {"minimumValidFraction", settings.m_minimumValidFraction},
    {"varianceEpsilon", settings.m_varianceEpsilon},
    {"invalidStyle", enumToName(settings.m_invalidStyle, k_localMetricInvalidStyleNames)}};
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
  j = json{
    {"metric", settings.m_metric},
    {"patchRadius", settings.m_patchRadius},
    {"sampleSpacing", settings.m_sampleSpacing},
    {"minimumValidFraction", settings.m_minimumValidFraction},
    {"varianceEpsilon", settings.m_varianceEpsilon},
    {"invalidStyle", enumToName(settings.m_invalidStyle, k_localMetricInvalidStyleNames)}};
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
  j = json{
    {"difference", settings.m_difference},
    {"localNormalizedCrossCorrelation", settings.m_localNcc},
    {"localLinearResidual", settings.m_localLinearResidual},
    {"overlay", {{"magentaCyan", settings.m_overlayMagentaCyan}}},
    {"quadrants", {{"x", static_cast<bool>(settings.m_quadrants.x)}, {"y", static_cast<bool>(settings.m_quadrants.y)}}},
    {"checkerboard", {{"squares", settings.m_checkerboardSquares}}},
    {"flashlight",
     {{"radiusFraction", settings.m_flashlightRadiusFraction},
      {"overlayMovingImage", settings.m_flashlightOverlayMovingImage}}}};
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
  j = json{
    {"samplingFactor", settings.m_samplingFactor},
    {"transparentBackgroundWhenNoHit", settings.m_transparentBackgroundWhenNoHit},
    {"backgroundEdgeBrighteningEnabled", settings.m_backgroundEdgeBrighteningEnabled},
    {"showCrosshairsIn3D", settings.m_showCrosshairsIn3D},
    {"crosshairs3DGlyphDiameterVoxelDiagonals", settings.m_crosshairs3DGlyphDiameterVoxelDiagonals},
    {"showThreeDCameraFrustumIn2DViews", settings.m_showThreeDCameraFrustumIn2DViews},
    {"reverseThreeDRotateAboutEye", settings.m_reverseThreeDRotateAboutEye},
    {"threeDCameraFrustumColor", vec4ToJson(settings.m_threeDCameraFrustumColor)},
    {"renderFrontFaces", settings.m_renderFrontFaces},
    {"renderBackFaces", settings.m_renderBackFaces},
    {"segmentationMasking", enumToName(settings.m_segmentationMasking, k_raycastSegmentationMaskingNames)}};
}

void from_json(const json& j, ProjectRaycastingSettings& settings)
{
  if (const auto value = j.find("samplingFactor"); value != j.end() && value->is_number()) {
    settings.m_samplingFactor = std::clamp(value->get<float>(), 0.5f, 2.0f);
  }
  if (const auto value = j.find("transparentBackgroundWhenNoHit"); value != j.end() && value->is_boolean()) {
    settings.m_transparentBackgroundWhenNoHit = value->get<bool>();
  }
  if (const auto value = j.find("backgroundEdgeBrighteningEnabled"); value != j.end() && value->is_boolean()) {
    settings.m_backgroundEdgeBrighteningEnabled = value->get<bool>();
  }
  if (const auto value = j.find("showCrosshairsIn3D"); value != j.end() && value->is_boolean()) {
    settings.m_showCrosshairsIn3D = value->get<bool>();
  }
  if (const auto value = j.find("crosshairs3DGlyphDiameterVoxelDiagonals"); value != j.end() && value->is_number()) {
    settings.m_crosshairs3DGlyphDiameterVoxelDiagonals = std::clamp(value->get<float>(), 0.1f, 10.0f);
  }
  if (const auto value = j.find("showThreeDCameraFrustumIn2DViews"); value != j.end() && value->is_boolean()) {
    settings.m_showThreeDCameraFrustumIn2DViews = value->get<bool>();
  }
  if (const auto value = j.find("reverseThreeDRotateAboutEye"); value != j.end() && value->is_boolean()) {
    settings.m_reverseThreeDRotateAboutEye = value->get<bool>();
  }
  if (const auto value = j.find("threeDCameraFrustumColor");
      value != j.end() && value->is_array() && value->size() == 4)
  {
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
  j = json{
    {"useMaximumImageExtent", settings.m_useMaximumImageExtent},
    {"slabThicknessMm", settings.m_slabThicknessMm},
    {"xrayEnergyKeV", settings.m_xrayEnergyKeV},
    {"xrayWindow", settings.m_xrayWindow},
    {"xrayLevel", settings.m_xrayLevel}};
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
  j = json{
    {"modulateOpacityWithImageOpacity", settings.m_modulateOpacityWithImageOpacity},
    {"outlineStyle", enumToName(settings.m_outlineStyle, k_segmentationOutlineNames)},
    {"interiorOpacity", settings.m_interiorOpacity},
    {"erosionFactor", settings.m_erosionFactor}};
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
  j = json{
    {"floatingPointInterpolationPolicy",
     enumToName(settings.m_floatingPointInterpolationPolicy, k_floatingPointInterpolationPolicyNames)},
    {"modulateOpacityWithImageOpacity", settings.m_modulateOpacityWithImageOpacity}};
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
  j = json{
    {"annotationsOnTop", settings.m_annotationsOnTop},
    {"landmarksOnTop", settings.m_landmarksOnTop},
    {"hideAnnotationVertices", settings.m_hideAnnotationVertices}};
}

void from_json(const json& j, ProjectAnnotationDisplaySettings& settings)
{
  if (const auto value = j.find("annotationsOnTop"); value != j.end() && value->is_boolean()) {
    settings.m_annotationsOnTop = value->get<bool>();
  }
  if (const auto value = j.find("landmarksOnTop"); value != j.end() && value->is_boolean()) {
    settings.m_landmarksOnTop = value->get<bool>();
  }
  if (const auto value = j.find("hideAnnotationVertices"); value != j.end() && value->is_boolean()) {
    settings.m_hideAnnotationVertices = value->get<bool>();
  }
}

void to_json(json& j, const RegistrationResult& result)
{
  j = json{
    {"backend", result.m_backend},
    {"fixedImageUid", result.m_fixedImageUid},
    {"movingImageUid", result.m_movingImageUid},
    {"warpedSegmentations", pathVectorToStrings(result.m_warpedSegmentations)},
    {"transformedSurfaces", pathVectorToStrings(result.m_transformedSurfaces)},
    {"transformedLandmarks", pathVectorToStrings(result.m_transformedLandmarks)},
    {"warnings", result.m_warnings}};

  if (result.m_manifestFileName) {
    j["manifest"] = optionalPathToString(result.m_manifestFileName);
  }
  if (result.m_warpedImage) {
    j["warpedImage"] = optionalPathToString(result.m_warpedImage);
  }
  if (result.m_inverseWarp) {
    j["inverseWarp"] = optionalPathToString(result.m_inverseWarp);
  }
  if (result.m_forwardWarp) {
    j["forwardWarp"] = optionalPathToString(result.m_forwardWarp);
  }
  if (result.m_affineTransform) {
    j["affineTransform"] = optionalPathToString(result.m_affineTransform);
  }
}

void from_json(const json& j, RegistrationResult& result)
{
  if (const auto value = j.find("backend"); value != j.end() && value->is_string()) {
    result.m_backend = value->get<std::string>();
  }
  if (const auto value = j.find("fixedImageUid"); value != j.end() && value->is_string()) {
    result.m_fixedImageUid = value->get<std::string>();
  }
  if (const auto value = j.find("movingImageUid"); value != j.end() && value->is_string()) {
    result.m_movingImageUid = value->get<std::string>();
  }
  optionalPathFromJson(j, "manifest", result.m_manifestFileName);
  optionalPathFromJson(j, "warpedImage", result.m_warpedImage);
  optionalPathFromJson(j, "inverseWarp", result.m_inverseWarp);
  optionalPathFromJson(j, "forwardWarp", result.m_forwardWarp);
  optionalPathFromJson(j, "affineTransform", result.m_affineTransform);
  if (const auto value = j.find("warpedSegmentations"); value != j.end()) {
    result.m_warpedSegmentations = pathVectorFromJson(*value);
  }
  if (const auto value = j.find("transformedSurfaces"); value != j.end()) {
    result.m_transformedSurfaces = pathVectorFromJson(*value);
  }
  if (const auto value = j.find("transformedLandmarks"); value != j.end()) {
    result.m_transformedLandmarks = pathVectorFromJson(*value);
  }
  if (const auto value = j.find("warnings"); value != j.end() && value->is_array()) {
    result.m_warnings = value->get<std::vector<std::string>>();
  }
}

void to_json(json& j, const EntropyProject& project)
{
  j = json{{"reference", project.m_referenceImage}};

  if (!project.m_additionalImages.empty()) {
    j["additional"] = project.m_additionalImages;
  }
  if (project.m_layoutsFileName) {
    j["layoutsFile"] = project.m_layoutsFileName->string();
  }
  else if (!project.m_layouts.empty()) {
    j["layouts"] = project.m_layouts;
  }
  if (project.m_currentLayoutIndex) {
    j["currentLayoutIndex"] = *project.m_currentLayoutIndex;
  }
  j["interface"] = project.m_interface;
  j["view"] = project.m_view;
  j["comparison"] = project.m_comparison;
  j["raycasting"] = project.m_raycasting;
  j["intensityProjection"] = project.m_intensityProjection;
  j["segmentationDisplay"] = project.m_segmentationDisplay;
  j["isosurfaces"] = project.m_isosurfaces;
  j["annotationDisplay"] = project.m_annotationDisplay;
  if (!project.m_registrationResults.empty()) {
    j["registrationResults"] = project.m_registrationResults;
  }
}

void from_json(const json& j, EntropyProject& project)
{
  j.at("reference").get_to(project.m_referenceImage);

  if (j.count("additional")) {
    j.at("additional").get_to(project.m_additionalImages);
  }
  if (j.count("layoutsFile")) {
    project.m_layoutsFileName = j.at("layoutsFile").get<std::string>();
  }
  if (j.count("layouts")) {
    j.at("layouts").get_to(project.m_layouts);
  }
  if (j.count("currentLayoutIndex")) {
    project.m_currentLayoutIndex = j.at("currentLayoutIndex").get<std::size_t>();
  }
  if (const auto interface = j.find("interface"); interface != j.end() && interface->is_object()) {
    interface->get_to(project.m_interface);
  }
  if (const auto view = j.find("view"); view != j.end() && view->is_object()) {
    view->get_to(project.m_view);
  }
  if (const auto comparison = j.find("comparison"); comparison != j.end() && comparison->is_object()) {
    comparison->get_to(project.m_comparison);
  }
  if (const auto raycasting = j.find("raycasting"); raycasting != j.end() && raycasting->is_object()) {
    raycasting->get_to(project.m_raycasting);
  }
  if (const auto intensityProjection = j.find("intensityProjection");
      intensityProjection != j.end() && intensityProjection->is_object())
  {
    intensityProjection->get_to(project.m_intensityProjection);
  }
  if (const auto segmentationDisplay = j.find("segmentationDisplay");
      segmentationDisplay != j.end() && segmentationDisplay->is_object())
  {
    segmentationDisplay->get_to(project.m_segmentationDisplay);
  }
  if (const auto isosurfaces = j.find("isosurfaces"); isosurfaces != j.end() && isosurfaces->is_object()) {
    isosurfaces->get_to(project.m_isosurfaces);
  }
  if (const auto annotationDisplay = j.find("annotationDisplay");
      annotationDisplay != j.end() && annotationDisplay->is_object())
  {
    annotationDisplay->get_to(project.m_annotationDisplay);
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
      makePathCanonicalAbsolute(result.m_manifestFileName, projectBasePath, "registration manifest");
      makePathCanonicalAbsolute(result.m_warpedImage, projectBasePath, "registered image");
      makePathCanonicalAbsolute(result.m_inverseWarp, projectBasePath, "registration inverse warp");
      makePathCanonicalAbsolute(result.m_forwardWarp, projectBasePath, "registration forward warp");
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

    if (image.m_affineTxFileName) {
      if (image.m_affineTxFileName->empty()) {
        spdlog::warn("Ignoring empty affine transform path for image {}", image.m_imageFileName);
        image.m_affineTxFileName = std::nullopt;
      }
      else {
        image.m_affineTxFileName = fs::canonical(*image.m_affineTxFileName);
      }
    }

    if (image.m_inverseWarpFileName) {
      if (image.m_inverseWarpFileName->empty()) {
        spdlog::warn("Ignoring empty inverse warp path for image {}", image.m_imageFileName);
        image.m_inverseWarpFileName = std::nullopt;
      }
      else {
        image.m_inverseWarpFileName = fs::canonical(*image.m_inverseWarpFileName);
      }
    }

    if (image.m_forwardWarpFileName) {
      if (image.m_forwardWarpFileName->empty()) {
        spdlog::warn("Ignoring empty forward warp path for image {}", image.m_imageFileName);
        image.m_forwardWarpFileName = std::nullopt;
      }
      else {
        image.m_forwardWarpFileName = fs::canonical(*image.m_forwardWarpFileName);
      }
    }

    if (image.m_inverseWarpReferenceImageFileName) {
      if (image.m_inverseWarpReferenceImageFileName->empty()) {
        spdlog::warn("Ignoring empty inverse warp reference image path for image {}", image.m_imageFileName);
        image.m_inverseWarpReferenceImageFileName = std::nullopt;
      }
      else {
        image.m_inverseWarpReferenceImageFileName = fs::canonical(*image.m_inverseWarpReferenceImageFileName);
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

    for (auto lmIt = image.m_landmarkGroups.begin(); lmIt != image.m_landmarkGroups.end();) {
      if (lmIt->m_csvFileName.empty()) {
        spdlog::warn("Ignoring empty landmark path for image {}", image.m_imageFileName);
        lmIt = image.m_landmarkGroups.erase(lmIt);
      }
      else {
        lmIt->m_csvFileName = fs::canonical(lmIt->m_csvFileName).string();
        ++lmIt;
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

    if (image.m_affineTxFileName) {
      image.m_affineTxFileName = fs::relative(*image.m_affineTxFileName, projectBasePath);
    }

    if (image.m_inverseWarpFileName) {
      image.m_inverseWarpFileName = fs::relative(*image.m_inverseWarpFileName, projectBasePath);
    }

    if (image.m_forwardWarpFileName) {
      image.m_forwardWarpFileName = fs::relative(*image.m_forwardWarpFileName, projectBasePath);
    }

    if (image.m_inverseWarpReferenceImageFileName) {
      image.m_inverseWarpReferenceImageFileName =
        fs::relative(*image.m_inverseWarpReferenceImageFileName, projectBasePath);
    }

    if (image.m_annotationsFileName) {
      image.m_annotationsFileName = fs::relative(*image.m_annotationsFileName, projectBasePath);
    }

    for (serialize::Segmentation& seg : image.m_segmentations) {
      seg.m_segFileName = fs::relative(seg.m_segFileName, projectBasePath);
    }

    for (serialize::LandmarkGroup& lm : image.m_landmarkGroups) {
      lm.m_csvFileName = fs::relative(lm.m_csvFileName, projectBasePath).string();
    }
  };

  auto makeRelativeIfPresent = [](std::optional<fs::path>& path, const fs::path& projectBasePath) {
    if (path && !path->empty()) {
      path = fs::relative(*path, projectBasePath);
    }
  };

  auto makeRegistrationResultPathsRelative =
    [&](serialize::RegistrationResult& result, const fs::path& projectBasePath) {
      makeRelativeIfPresent(result.m_manifestFileName, projectBasePath);
      makeRelativeIfPresent(result.m_warpedImage, projectBasePath);
      makeRelativeIfPresent(result.m_inverseWarp, projectBasePath);
      makeRelativeIfPresent(result.m_forwardWarp, projectBasePath);
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

    annots = j.get<std::vector<Annotation>>();
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

void appendAnnotationToJson(const Annotation& annot, json& j)
{
  j.emplace_back(annot);
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
