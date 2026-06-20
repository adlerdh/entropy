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

template<typename Enum>
struct EnumName
{
  Enum value;
  std::string_view name;
};

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

constexpr std::array k_layoutTabPlacementNames{
  EnumName{serialize::ProjectLayoutTabPlacement::Top, "top"},
  EnumName{serialize::ProjectLayoutTabPlacement::Bottom, "bottom"}};

constexpr std::array k_edgeDetectionMethodNames{
  EnumName{serialize::ProjectEdgeDetectionMethod::Pixel, "pixel"},
  EnumName{serialize::ProjectEdgeDetectionMethod::Voxel, "voxel"}};

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
  std::function<void(serialize::Image& image, const fs::path& projectBasePath)> func)
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
    {"level", settings.m_level},
    {"window", settings.m_window},
    {"thresholdLow", settings.m_thresholdLow},
    {"thresholdHigh", settings.m_thresholdHigh},
    {"opacity", settings.m_opacity},
    {"edgeDetectionMethod", enumToName(settings.m_edgeDetectionMethod, k_edgeDetectionMethodNames)},
    {"showEdges", settings.m_showEdges},
    {"hardEdges", settings.m_thresholdEdges},
    {"thinPixelEdges", settings.m_thinPixelEdges},
    {"overlayEdges", settings.m_overlayEdges},
    {"edgeMagnitude", settings.m_edgeMagnitude},
    {"pixelEdgeScale", settings.m_pixelEdgeScale},
    {"pixelEdgeThreshold", settings.m_pixelEdgeThreshold},
    {"edgeColor", {settings.m_edgeColor.r, settings.m_edgeColor.g, settings.m_edgeColor.b}},
    {"edgeOpacity", settings.m_edgeOpacity}};

  if (serialize::ProjectEdgeDetectionMethod::Voxel == settings.m_edgeDetectionMethod) {
    j["colormapEdges"] = settings.m_colormapEdges;
  }
}

void from_json(const json& j, serialize::ImageSettings& settings)
{
  if (j.count("displayName")) {
    j.at("displayName").get_to(settings.m_displayName);
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
}

void to_json(json& j, const serialize::SegSettings& settings)
{
  j = json{{"opacity", settings.m_opacity}};
}

void from_json(const json& j, serialize::SegSettings& settings)
{
  if (j.count("opacity")) {
    j.at("opacity").get_to(settings.m_opacity);
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

  if (image.m_dicomSource) {
    j["dicomSource"] = *image.m_dicomSource;
  }

  if (image.m_affineTxFileName) {
    j["affine"] = image.m_affineTxFileName->string();
  }

  if (image.m_deformationFileName) {
    j["deformation"] = image.m_deformationFileName->string();
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

  if (image.m_settings) {
    j["settings"] = *image.m_settings;
  }
}

void from_json(const json& j, serialize::Image& image)
{
  std::string p;
  j.at("image").get_to(p);
  image.m_imageFileName = p;

  if (j.count("dicomSource")) {
    image.m_dicomSource = j.at("dicomSource").get<serialize::DicomSource>();
  }

  if (j.count("affine")) {
    image.m_affineTxFileName = j.at("affine").get<std::string>();
  }

  if (j.count("deformation")) {
    image.m_deformationFileName = j.at("deformation").get<std::string>();
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

  if (j.count("settings")) {
    image.m_settings = j.at("settings").get<serialize::ImageSettings>();
  }
}

void to_json(json& j, const ProjectInterfaceSettings& settings)
{
  j = json{
    {"showLayoutTabs", settings.m_showLayoutTabs},
    {"layoutTabsPosition", enumToName(settings.m_layoutTabPlacement, k_layoutTabPlacementNames)},
    {"imageValuePrecision", settings.m_imageValuePrecision},
    {"coordinatesPrecision", settings.m_coordsPrecision},
    {"transformPrecision", settings.m_txPrecision},
    {"percentilePrecision", settings.m_percentilePrecision}};
}

void from_json(const json& j, ProjectInterfaceSettings& settings)
{
  if (const auto showTabs = j.find("showLayoutTabs"); showTabs != j.end() && showTabs->is_boolean()) {
    settings.m_showLayoutTabs = showTabs->get<bool>();
  }
  if (
    const auto parsed =
      enumFromName<ProjectLayoutTabPlacement>(j.value("layoutTabsPosition", ""), k_layoutTabPlacementNames))
  {
    settings.m_layoutTabPlacement = *parsed;
  }
  if (const auto precision = j.find("imageValuePrecision"); precision != j.end()) {
    if (const auto parsed = unsignedIntFromJson(*precision)) {
      settings.m_imageValuePrecision = *parsed;
    }
  }
  if (const auto precision = j.find("coordinatesPrecision"); precision != j.end()) {
    if (const auto parsed = unsignedIntFromJson(*precision)) {
      settings.m_coordsPrecision = *parsed;
    }
  }
  if (const auto precision = j.find("transformPrecision"); precision != j.end()) {
    if (const auto parsed = unsignedIntFromJson(*precision)) {
      settings.m_txPrecision = *parsed;
    }
  }
  if (const auto precision = j.find("percentilePrecision"); precision != j.end()) {
    if (const auto parsed = unsignedIntFromJson(*precision)) {
      settings.m_percentilePrecision = *parsed;
    }
  }
}

void to_json(json& j, const EntropyProject& project)
{
  j = json{{"reference", project.m_referenceImage}};

  if (!project.m_additionalImages.empty()) {
    j["additional"] = project.m_additionalImages;
  }
  if (!project.m_layouts.empty()) {
    j["layouts"] = project.m_layouts;
  }
  if (project.m_currentLayoutIndex) {
    j["currentLayoutIndex"] = *project.m_currentLayoutIndex;
  }
  j["interface"] = project.m_interface;
}

void from_json(const json& j, EntropyProject& project)
{
  j.at("reference").get_to(project.m_referenceImage);

  if (j.count("additional")) {
    j.at("additional").get_to(project.m_additionalImages);
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

    if (image.m_deformationFileName) {
      if (image.m_deformationFileName->empty()) {
        spdlog::warn("Ignoring empty deformation path for image {}", image.m_imageFileName);
        image.m_deformationFileName = std::nullopt;
      }
      else {
        image.m_deformationFileName = fs::canonical(*image.m_deformationFileName);
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

    if (image.m_deformationFileName) {
      image.m_deformationFileName = fs::relative(*image.m_deformationFileName, projectBasePath);
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

    // Make all image paths relative to the project file:
    applyToImagePaths(projectRelative, projectBasePath, makeRelative);
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
