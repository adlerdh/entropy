#include "logic/app/MeshExport.h"

#include "common/UuidUtility.h"
#include "image/Image.h"
#include "image/Isosurface.h"
#include "logic/app/Data.h"
#include "logic/app/DeformationWarp.h"
#include "mesh/MeshIO.h"
#include "rendering/mesh/MeshGeneration.h"
#include "rendering/mesh/MeshImageAdapter.h"
#include "ui/ExportJobService.h"
#include "ui/NativeFileDialogs.h"
#include "ui/dialogs/NativeMessageDialogs.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mesh_export
{
namespace
{
namespace fs = std::filesystem;

struct ExportTransformSnapshot
{
  glm::dmat4 imagePhysicalToWorld{1.0};
  std::shared_ptr<const Image> forwardWarp;
  float warpStrength = 0.0f;
};

class SnapshotForwardWarpTransform final : public mesh::IPointTransform
{
public:
  SnapshotForwardWarpTransform(std::shared_ptr<const Image> warp, const float strength)
    : m_warp{std::move(warp)}, m_strength{strength}
  {
  }

  std::expected<glm::dvec3, std::string> transformPoint(const glm::dvec3& point) const override
  {
    if (!m_warp || m_strength <= 0.0f) {
      return point;
    }
    const std::optional<glm::vec3> displacement =
      deformation_warp::sampleWarpDisplacementWorld(*m_warp, glm::vec3{point});
    if (!displacement) {
      return std::unexpected("The forward deformation could not be sampled at an exported vertex");
    }
    const glm::dvec3 transformed = point + static_cast<double>(m_strength) * glm::dvec3{*displacement};
    if (!std::isfinite(transformed.x) || !std::isfinite(transformed.y) || !std::isfinite(transformed.z)) {
      return std::unexpected("The forward deformation produced a non-finite vertex");
    }
    return transformed;
  }

private:
  std::shared_ptr<const Image> m_warp;
  float m_strength;
};

std::optional<mesh::MeshExportSpace> chooseCoordinateSpace()
{
  const auto result = native_dialog::showMessageDialog(
    {.title = "Mesh Export Coordinates",
     .message = "Choose the coordinates to write.",
     .informativeText =
       "Image physical coordinates preserve the mesh before image transforms. Current world coordinates bake in "
       "the image's affine and forward deformation transforms.",
     .firstButton = "Image Physical",
     .secondButton = "Current World",
     .thirdButton = "Cancel"});
  if (!result) {
    return mesh::MeshExportSpace::ImagePhysical;
  }
  if (*result == native_dialog::MessageDialogResult::FirstButton) {
    return mesh::MeshExportSpace::ImagePhysical;
  }
  if (*result == native_dialog::MessageDialogResult::SecondButton) {
    return mesh::MeshExportSpace::CurrentWorld;
  }
  return std::nullopt;
}

void showMeshExportFormatGuide(AppSettings& settings, const bool exportingAllLabels)
{
  if (!settings.showMeshExportFormatGuide()) return;

  std::string details =
    "Select the output format using its filename extension:\n\n"
    "VTK XML PolyData: .vtp\n"
    "Legacy VTK PolyData: .vtk\n"
    "STL: .stl\n"
    "PLY: .ply\n"
    "Wavefront OBJ: .obj\n"
    "Object File Format: .off\n"
    "GIFTI surface: .gii, .surf.gii\n"
    "FreeSurfer binary surface: .fsb, .fcv, or a native surface extension\n"
    "FreeSurfer ASCII surface: .fsa, .asc";
  if (exportingAllLabels) {
    details +=
      "\n\nOne file will be written for every non-empty label. Each output name will be suffixed with its label "
      "index, for example '_label-12'.";
  }

  const auto result = native_dialog::showMessageDialog(
    {.title = "Mesh Export Formats",
     .message = "Entropy writes the mesh format selected by the filename extension.",
     .informativeText = details,
     .firstButton = "Continue",
     .secondButton = "Don't Show Again",
     .thirdButton = "",
     .severity = native_dialog::MessageDialogSeverity::Information});
  if (result && *result == native_dialog::MessageDialogResult::SecondButton) {
    settings.setShowMeshExportFormatGuide(false);
  }
}

mesh::MeshRecord
meshRecord(const std::string& sourceUid, const std::string& imageUid, const rendering::mesh::MeshData& generated)
{
  mesh::MeshRecord result;
  result.uid = sourceUid;
  result.associatedImageUid = imageUid;
  result.geometry.positions = generated.positions;
  result.geometry.normals = generated.normals;
  result.geometry.triangleIndices = generated.indices;
  result.coordinates.storedSpace = mesh::MeshCoordinateSpace::ImagePhysical;
  result.coordinates.anatomicalSystem = mesh::AnatomicalCoordinateSystem::LPS;
  result.coordinates.description = "Generated in associated image physical coordinates";
  return result;
}

ExportTransformSnapshot captureTransform(const AppData& appData, const uuids::uuid& imageUid)
{
  ExportTransformSnapshot result;
  const Image* image = appData.image(imageUid);
  if (!image) return result;

  result.imagePhysicalToWorld = glm::dmat4{image->transformations().worldDef_T_subject()};
  if (!image->settings().warpEnabled() || image->settings().warpStrength() <= 0.0f) return result;

  const auto warpUid = appData.imageToActiveForwardWarpUid(imageUid);
  const Image* warp = warpUid ? appData.warpField(*warpUid) : nullptr;
  if (warp && deformation_warp::warpFieldMatchesImageDomain(*warp, *image)) {
    result.forwardWarp = std::make_shared<Image>(*warp);
    result.warpStrength = image->settings().warpStrength();
  }
  return result;
}

std::optional<std::string> writeMesh(
  const mesh::MeshRecord& record,
  const fs::path& path,
  const mesh::MeshExportSpace space,
  const ExportTransformSnapshot& transform)
{
  const SnapshotForwardWarpTransform deformation{transform.forwardWarp, transform.warpStrength};
  const mesh::MeshIO io;
  const auto result = io.write(mesh::MeshWriteRequest{
    .path = path,
    .mesh = &record,
    .coordinateSpace = space,
    .imagePhysicalToWorld = transform.imagePhysicalToWorld,
    .deformation = space == mesh::MeshExportSpace::CurrentWorld && transform.forwardWarp ? &deformation : nullptr,
    .outputAnatomicalSystem = mesh::AnatomicalCoordinateSystem::LPS});
  return result ? std::nullopt : std::optional{result.error().message};
}

rendering::mesh::MeshGenerationOptions generationOptions(const AppData& appData, const bool segmentation)
{
  return {
    .threadCount = 0,
    .smoothSurface = segmentation ? appData.renderSettings().m_smoothSegmentationMeshes
                                  : appData.renderSettings().m_smoothIsosurfaceMeshes,
    .smoothingIterations = appData.renderSettings().m_meshSmoothingIterations,
    .smoothingPassBand = appData.renderSettings().m_meshSmoothingPassBand};
}

fs::path labelPath(const fs::path& basePath, const std::size_t labelIndex)
{
  return basePath.parent_path() /
         (basePath.stem().string() + "_label-" + std::to_string(labelIndex) + basePath.extension().string());
}

std::shared_ptr<ui::export_jobs::Service> exportService(AppData& appData)
{
  const auto service = appData.guiData().m_exportJobs;
  if (!service) {
    native_dialog::showErrorMessageDialog("Export Failed", "The background export service is unavailable.");
    return nullptr;
  }
  const auto status = service->snapshot();
  if (status.hasJob && ui::export_jobs::Outcome::Running == status.outcome) {
    native_dialog::showErrorMessageDialog(
      "Export Already in Progress",
      "Wait for the current export to finish or cancel it before starting another export.");
    return nullptr;
  }
  return service;
}

void showBusyError()
{
  native_dialog::showErrorMessageDialog(
    "Export Already in Progress",
    "Wait for the current export to finish or cancel it before starting another export.");
}

ui::export_jobs::Result commitSingleOutput(ui::export_jobs::JobContext& context, ui::export_jobs::StagedOutput& staged)
{
  if (context.cancellationRequested()) return ui::export_jobs::Result::cancelled();
  context.update("Committing mesh file", 0.98f);
  if (const auto error = staged.commit()) {
    return ui::export_jobs::Result::failure("The completed export could not replace the destination: " + *error);
  }
  return ui::export_jobs::Result::success({staged.destination()});
}
} // namespace

void exportIsosurface(
  AppData& appData,
  const uuids::uuid& imageUid,
  const uint32_t component,
  const uuids::uuid& surfaceUid)
{
  const Image* image = appData.image(imageUid);
  const Isosurface* surface = appData.isosurface(imageUid, component, surfaceUid);
  const auto service = exportService(appData);
  if (!image || !surface || !service) return;

  showMeshExportFormatGuide(appData.settings(), false);
  const auto space = chooseCoordinateSpace();
  if (!space) return;
  const std::string defaultName = (surface->name.empty() ? "isosurface" : surface->name) + ".vtp";
  const auto path = native_dialog::saveFile(native_dialog::meshFilters(), {}, defaultName);
  if (!path) return;

  const auto imageSnapshot = std::make_shared<Image>(*image);
  const auto options = generationOptions(appData, false);
  const ExportTransformSnapshot transform = captureTransform(appData, imageUid);
  const std::string imageUidString = uuids::to_string(imageUid);
  const std::string surfaceUidString = uuids::to_string(surfaceUid);
  const float isoValue = surface->value;
  const uint32_t timePoint = image->timeAxis().clamp(image->settings().activeTimePoint());
  const fs::path destination = *path;
  const bool submitted = service->submit(
    {.description = "Exporting isosurface mesh",
     .destination = destination,
     .task = [imageSnapshot,
              options,
              transform,
              imageUidString,
              surfaceUidString,
              component,
              timePoint,
              isoValue,
              destination,
              space = *space](ui::export_jobs::JobContext& context) {
       context.update("Preparing isosurface data", 0.05f);
       const auto grid = rendering::mesh::scalarGridFromImageComponent(
         *imageSnapshot,
         component,
         timePoint,
         rendering::mesh::MeshCoordinateSpace::ImageSubject);
       if (!grid) {
         return ui::export_jobs::Result::failure("The selected image component could not be prepared.");
       }
       if (context.cancellationRequested()) return ui::export_jobs::Result::cancelled();

       context.update("Generating isosurface mesh");
       const auto generated = rendering::mesh::generateIsoSurfaceMesh(*grid, isoValue, options);
       if (!generated) return ui::export_jobs::Result::failure("No isosurface mesh could be generated.");
       if (context.cancellationRequested()) return ui::export_jobs::Result::cancelled();

       ui::export_jobs::StagedOutput staged{destination};
       const mesh::MeshRecord record = meshRecord(surfaceUidString, imageUidString, *generated);
       context.update("Writing mesh file");
       if (const auto error = writeMesh(record, staged.temporaryPath(), space, transform)) {
         spdlog::error("Could not export isosurface mesh {} to '{}': {}", surfaceUidString, destination, *error);
         return ui::export_jobs::Result::failure(*error);
       }
       auto result = commitSingleOutput(context, staged);
       if (ui::export_jobs::Outcome::Succeeded == result.outcome) {
         spdlog::info("Exported isosurface mesh {} to '{}'", surfaceUidString, destination);
       }
       return result;
     }});
  if (!submitted) showBusyError();
}

void exportSegmentationLabel(
  AppData& appData,
  const uuids::uuid& imageUid,
  const uuids::uuid& segmentationUid,
  const std::size_t labelIndex)
{
  const Image* segmentation = appData.seg(segmentationUid);
  const Image* image = appData.image(imageUid);
  const auto service = exportService(appData);
  if (!segmentation || !image || !service) return;

  showMeshExportFormatGuide(appData.settings(), false);
  const auto space = chooseCoordinateSpace();
  if (!space) return;
  const auto path =
    native_dialog::saveFile(native_dialog::meshFilters(), {}, std::format("segmentation_label-{}.vtp", labelIndex));
  if (!path) return;

  const auto segmentationSnapshot = std::make_shared<Image>(*segmentation);
  const auto options = generationOptions(appData, true);
  const ExportTransformSnapshot transform = captureTransform(appData, imageUid);
  const uint32_t timePoint = segmentation->timeAxis().clamp(segmentation->settings().activeTimePoint());
  const std::string segmentationUidString = uuids::to_string(segmentationUid);
  const std::string imageUidString = uuids::to_string(imageUid);
  const std::string meshUid = uuids::to_string(generateRandomUuid());
  const fs::path destination = *path;
  const bool submitted = service->submit(
    {.description = std::format("Exporting segmentation label {} mesh", labelIndex),
     .destination = destination,
     .task = [segmentationSnapshot,
              options,
              transform,
              timePoint,
              labelIndex,
              segmentationUidString,
              imageUidString,
              meshUid,
              destination,
              space = *space](ui::export_jobs::JobContext& context) {
       context.update("Finding segmentation label", 0.05f);
       const auto inventory = rendering::mesh::segmentationLabelInventory(*segmentationSnapshot, 0, timePoint);
       if (!inventory) return ui::export_jobs::Result::failure("The selected label contains no voxels.");
       const auto found = inventory->find(static_cast<int64_t>(labelIndex));
       if (found == inventory->end()) {
         return ui::export_jobs::Result::failure("The selected label contains no voxels.");
       }
       if (context.cancellationRequested()) return ui::export_jobs::Result::cancelled();

       context.update("Generating segmentation mesh");
       const auto grid = rendering::mesh::labelMaskGridFromImageComponent(
         *segmentationSnapshot,
         0,
         static_cast<int64_t>(labelIndex),
         found->second,
         timePoint,
         rendering::mesh::MeshCoordinateSpace::ImageSubject);
       const auto generated = grid ? rendering::mesh::generateBinaryMaskSurface(*grid, options) : std::nullopt;
       if (!generated) {
         return ui::export_jobs::Result::failure("No mesh could be generated for the selected label.");
       }
       if (context.cancellationRequested()) return ui::export_jobs::Result::cancelled();

       ui::export_jobs::StagedOutput staged{destination};
       const mesh::MeshRecord record = meshRecord(meshUid, imageUidString, *generated);
       context.update("Writing mesh file");
       if (const auto error = writeMesh(record, staged.temporaryPath(), space, transform)) {
         spdlog::error(
           "Could not export label {} of segmentation {} to '{}': {}",
           labelIndex,
           segmentationUidString,
           destination,
           *error);
         return ui::export_jobs::Result::failure(*error);
       }
       auto result = commitSingleOutput(context, staged);
       if (ui::export_jobs::Outcome::Succeeded == result.outcome) {
         spdlog::info("Exported label {} of segmentation {} to '{}'", labelIndex, segmentationUidString, destination);
       }
       return result;
     }});
  if (!submitted) showBusyError();
}

void exportAllSegmentationLabels(AppData& appData, const uuids::uuid& imageUid, const uuids::uuid& segmentationUid)
{
  const Image* segmentation = appData.seg(segmentationUid);
  const Image* image = appData.image(imageUid);
  const auto service = exportService(appData);
  if (!segmentation || !image || !service) return;

  showMeshExportFormatGuide(appData.settings(), true);
  const auto space = chooseCoordinateSpace();
  if (!space) return;
  const auto basePath = native_dialog::saveFile(native_dialog::meshFilters(), {}, "segmentation.vtp");
  if (!basePath) return;

  const auto segmentationSnapshot = std::make_shared<Image>(*segmentation);
  const auto options = generationOptions(appData, true);
  const ExportTransformSnapshot transform = captureTransform(appData, imageUid);
  const uint32_t timePoint = segmentation->timeAxis().clamp(segmentation->settings().activeTimePoint());
  const std::string segmentationUidString = uuids::to_string(segmentationUid);
  const std::string imageUidString = uuids::to_string(imageUid);
  const fs::path destination = *basePath;
  const bool submitted = service->submit(
    {.description = "Exporting all segmentation label meshes",
     .destination = destination,
     .task = [segmentationSnapshot,
              options,
              transform,
              timePoint,
              segmentationUidString,
              imageUidString,
              destination,
              space = *space](ui::export_jobs::JobContext& context) {
       context.update("Finding non-empty segmentation labels", 0.02f);
       const auto inventory = rendering::mesh::segmentationLabelInventory(*segmentationSnapshot, 0, timePoint);
       if (!inventory) return ui::export_jobs::Result::failure("Segmentation labels could not be read.");

       std::vector<std::pair<int64_t, rendering::mesh::SegmentationLabelBounds>> labels;
       for (const auto& [labelValue, bounds] : *inventory) {
         if (labelValue > 0) labels.emplace_back(labelValue, bounds);
       }
       if (labels.empty()) {
         return ui::export_jobs::Result::failure("The segmentation contains no non-empty foreground labels.");
       }

       std::vector<ui::export_jobs::StagedOutput> stagedOutputs;
       std::vector<fs::path> outputPaths;
       stagedOutputs.reserve(labels.size());
       outputPaths.reserve(labels.size());
       for (std::size_t index = 0; index < labels.size(); ++index) {
         if (context.cancellationRequested()) return ui::export_jobs::Result::cancelled();
         const auto& [labelValue, bounds] = labels[index];
         const float baseProgress = static_cast<float>(index) / static_cast<float>(labels.size());
         context.update(
           std::format("Generating label {} ({}/{})", labelValue, index + 1u, labels.size()),
           0.05f + 0.80f * baseProgress);
         const auto grid = rendering::mesh::labelMaskGridFromImageComponent(
           *segmentationSnapshot,
           0,
           labelValue,
           bounds,
           timePoint,
           rendering::mesh::MeshCoordinateSpace::ImageSubject);
         const auto generated = grid ? rendering::mesh::generateBinaryMaskSurface(*grid, options) : std::nullopt;
         if (!generated) {
           spdlog::warn(
             "Skipping segmentation label {} during mesh export because it produced no triangles",
             labelValue);
           continue;
         }

         const fs::path outputPath = labelPath(destination, static_cast<std::size_t>(labelValue));
         stagedOutputs.emplace_back(outputPath);
         const mesh::MeshRecord record = meshRecord(uuids::to_string(generateRandomUuid()), imageUidString, *generated);
         context.update(
           std::format("Writing label {} ({}/{})", labelValue, index + 1u, labels.size()),
           0.05f + 0.80f * (baseProgress + 0.5f / static_cast<float>(labels.size())));
         if (const auto error = writeMesh(record, stagedOutputs.back().temporaryPath(), space, transform)) {
           spdlog::error(
             "Could not export label {} of segmentation {} to '{}': {}",
             labelValue,
             segmentationUidString,
             outputPath,
             *error);
           return ui::export_jobs::Result::failure(*error);
         }
         outputPaths.push_back(outputPath);
       }

       if (outputPaths.empty()) {
         return ui::export_jobs::Result::failure("No segmentation label produced an exportable mesh.");
       }
       if (context.cancellationRequested()) return ui::export_jobs::Result::cancelled();
       context.update("Committing mesh files", 0.95f);
       for (auto& staged : stagedOutputs) {
         if (const auto error = staged.commit()) {
           return ui::export_jobs::Result::failure("A completed export could not replace its destination: " + *error);
         }
       }
       spdlog::info("Exported {} label meshes for segmentation {}", outputPaths.size(), segmentationUidString);
       return ui::export_jobs::Result::success(
         std::move(outputPaths),
         std::format("Exported {} label meshes.", stagedOutputs.size()));
     }});
  if (!submitted) showBusyError();
}
} // namespace mesh_export
