#include "logic/app/ProjectSnapshot.h"
#include "logic/app/Data.h"
#include "common/UuidUtility.h"
#include "image/ImageWriter.h"
#include "ui/ExportJobService.h"
#include <spdlog/spdlog.h>
#include <spdlog/fmt/std.h>
namespace fs = std::filesystem;

std::optional<serialize::EntropyProject> project_snapshot::persistProject(
  AppData& data,
  const fs::path& normalizedFileName,
  const DicomSources& dicomSources,
  const NativeViews& nativeViews,
  const ProjectWriter& writeProject)
{
  struct NewWarpAsset
  {
    Image* image;
    fs::path originalPath;
    bool originallyOnDisk;
    fs::path path;
  };
  struct AssetTransaction
  {
    std::vector<NewWarpAsset> assets;
    bool published = false;
    ~AssetTransaction()
    {
      if (published) return;
      for (auto& asset : assets) {
        asset.image->header().setFileName(asset.originalPath);
        asset.image->header().setExistsOnDisk(asset.originallyOnDisk);
        std::error_code error;
        fs::remove(asset.path, error);
      }
    }
  } assets;
  try {
    for (const auto& imageUid : data.imageUidsOrdered()) {
      for (const auto& warpUid : data.imageToDefUids(imageUid)) {
        Image* warp = data.warpField(warpUid);
        if (!warp || (warp->header().existsOnDisk() && !warp->header().fileName().empty())) continue;
        const fs::path directory =
          normalizedFileName.parent_path() / (normalizedFileName.filename().string() + ".assets");
        fs::create_directories(directory);
        const fs::path path = fs::absolute(directory / (uuids::to_string(generateRandomUuid()) + ".nii.gz"));
        ui::export_jobs::StagedOutput output(path);
        const auto written = image_io::writeImage(*warp, output.temporaryPath());
        if (!written) {
          spdlog::error("Cannot save generated warp: {}", written.message);
          return std::nullopt;
        }
        if (const auto error = output.commit()) {
          spdlog::error("Cannot publish generated warp: {}", *error);
          return std::nullopt;
        }
        assets.assets.push_back({warp, warp->header().fileName(), warp->header().existsOnDisk(), path});
        warp->header().setFileName(path);
        warp->header().setExistsOnDisk(true);
      }
    }
  }
  catch (const std::exception& error) {
    spdlog::error("Cannot persist generated project assets: {}", error.what());
    return std::nullopt;
  }
  serialize::EntropyProject project = captureProject(data, dicomSources, nativeViews);

  if (project.m_referenceImage.m_imageFileName.empty()) {
    spdlog::error("Cannot save project without a reference image");
    return std::nullopt;
  }

  if (!writeProject(project, normalizedFileName)) {
    spdlog::error("Could not save project file {}", normalizedFileName);
    return std::nullopt;
  }
  assets.published = true;
  return project;
}
