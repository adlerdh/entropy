#include "logic/app/ProjectSnapshot.h"
#include "logic/app/Data.h"
#include "logic/app/DeformationArchive.h"
#include "common/UuidUtility.h"
#include "image/ImageWriter.h"
#include "ui/ExportJobService.h"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/std.h>

#include <memory>
#include <algorithm>
#include <stdexcept>
#include <set>

namespace fs = std::filesystem;

std::optional<serialize::EntropyProject> project_snapshot::persistProject(
  AppData& data,
  const fs::path& normalizedFileName,
  const DicomSources& dicomSources,
  const NativeViews& nativeViews,
  const ProjectWriter& writeProject,
  const std::vector<DeformationArchiveSource>& deformationArchives)
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
    ~AssetTransaction()
    {
      for (auto& asset : assets) {
        asset.image->header().setFileName(asset.originalPath);
        asset.image->header().setExistsOnDisk(asset.originallyOnDisk);
        std::error_code error;
        fs::remove(asset.path, error);
      }
    }

    void commit() noexcept
    {
      assets.clear();
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

  std::vector<std::unique_ptr<deformation_archive::StagedBundle>> bundles;
  try {
    std::set<std::string> editIds;
    for (const auto& source : deformationArchives) {
      if (!source.history) throw std::invalid_argument("Deformation archive source has no history");
      if (!editIds.insert(source.editId).second) throw std::invalid_argument("Duplicate deformation edit ID");
      auto bundle =
        std::make_unique<deformation_archive::StagedBundle>(normalizedFileName, source.editId, *source.history);
      const auto reference = bundle->publish();
      const auto existing =
        std::find_if(project.m_deformationEdits.begin(), project.m_deformationEdits.end(), [&](const auto& edit) {
          return edit.m_editId == source.editId;
        });
      if (existing == project.m_deformationEdits.end())
        project.m_deformationEdits.push_back(reference);
      else
        *existing = reference;
      bundles.push_back(std::move(bundle));
    }
  }
  catch (const std::exception& error) {
    spdlog::error("Cannot persist deformation history: {}", error.what());
    return std::nullopt;
  }

  if (project.m_referenceImage.m_imageFileName.empty()) {
    spdlog::error("Cannot save project without a reference image");
    return std::nullopt;
  }

  bool written = false;
  try {
    written = writeProject(project, normalizedFileName);
  }
  catch (const std::exception& error) {
    spdlog::error("Could not save project file {}: {}", normalizedFileName, error.what());
    return std::nullopt;
  }
  if (!written) {
    spdlog::error("Could not save project file {}", normalizedFileName);
    return std::nullopt;
  }

  assets.commit();
  for (auto& bundle : bundles)
    bundle->commit();
  return project;
}
