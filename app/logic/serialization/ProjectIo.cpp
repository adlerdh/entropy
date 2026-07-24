#include "logic/serialization/ProjectSerialization.h"

#include <safeclib/strerrorlen_s.h>

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <cerrno>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <system_error>

#if !defined(_MSC_VER)
#define HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR 1
#else
#define HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR 0
#endif

using json = nlohmann::json;

namespace fs = std::filesystem;

namespace
{
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

#if !HAS_IOS_BASE_FAILURE_DERIVED_FROM_SYSTEM_ERROR
void logStdErrno()
{
  const size_t errmsglen = safeclib::strerrorlen_s(errno) + 1;
  std::unique_ptr<char[]> errmsg(new char[errmsglen]);
  strerror_s(errmsg.get(), errmsglen, errno);

  spdlog::error("Error #{}: {}", errno, errmsg.get());
}
#endif
} // namespace

namespace serialize
{

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

} // namespace serialize
