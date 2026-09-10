#include "logic/app/MeshExport.h"

#include "image/Image.h"
#include "image/Isosurface.h"
#include "common/UuidUtility.h"
#include "logic/app/Data.h"
#include "logic/app/DeformationWarp.h"
#include "mesh/MeshIO.h"
#include "rendering/mesh/MeshGeneration.h"
#include "rendering/mesh/MeshImageAdapter.h"
#include "ui/NativeFileDialogs.h"
#include "ui/dialogs/NativeMessageDialogs.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <expected>
#include <cmath>
#include <filesystem>
#include <format>
#include <optional>
#include <string>

namespace mesh_export
{
namespace
{
namespace fs = std::filesystem;

class ForwardWarpTransform final : public mesh::IPointTransform
{
public:
  ForwardWarpTransform(const AppData& appData, const uuids::uuid& imageUid) : m_appData{appData}, m_imageUid{imageUid}
  {
  }

  std::expected<glm::dvec3, std::string> transformPoint(const glm::dvec3& point) const override
  {
    const glm::vec4 transformed =
      deformation_warp::forwardWarpDisplayWorldPosition(m_appData, m_imageUid, glm::vec4{glm::vec3{point}, 1.0f});
    if (
      !std::isfinite(transformed.x) || !std::isfinite(transformed.y) || !std::isfinite(transformed.z) ||
      !std::isfinite(transformed.w) || std::abs(transformed.w) <= 1.0e-12f)
    {
      return std::unexpected("The forward deformation produced a non-finite vertex");
    }
    return glm::dvec3{transformed} / static_cast<double>(transformed.w);
  }

private:
  const AppData& m_appData;
  uuids::uuid m_imageUid;
};

std::optional<mesh::MeshExportSpace> chooseCoordinateSpace()
{
  const auto result = native_dialog::showMessageDialog(
    {.title = "Mesh Export Coordinates",
     .message = "Choose the coordinates to write.",
     .informativeText =
       "Image physical coordinates preserve the mesh before image transforms. Current world coordinates bake in "
       "the image's affine and forward deformation transforms.\n\n"
       "Select the output format using its filename extension:\n"
       "VTK XML PolyData (.vtp)\n"
       "Legacy VTK PolyData (.vtk)\n"
       "STL (.stl)\n"
       "PLY (.ply)\n"
       "Wavefront OBJ (.obj)\n"
       "Object File Format (.off)\n"
       "GIFTI surface (.gii or .surf.gii)\n"
       "FreeSurfer binary surface (.fsb, .fcv, or a native surface extension)\n"
       "FreeSurfer ASCII surface (.fsa or .asc)",
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

mesh::MeshRecord
meshRecord(const uuids::uuid& sourceUid, const uuids::uuid& imageUid, const rendering::mesh::MeshData& generated)
{
  mesh::MeshRecord result;
  result.uid = uuids::to_string(sourceUid);
  result.associatedImageUid = uuids::to_string(imageUid);
  result.geometry.positions = generated.positions;
  result.geometry.normals = generated.normals;
  result.geometry.triangleIndices = generated.indices;
  result.coordinates.storedSpace = mesh::MeshCoordinateSpace::ImagePhysical;
  result.coordinates.anatomicalSystem = mesh::AnatomicalCoordinateSystem::LPS;
  result.coordinates.description = "Generated in associated image physical coordinates";
  return result;
}

bool writeMesh(
  const AppData& appData,
  const uuids::uuid& imageUid,
  const mesh::MeshRecord& record,
  const fs::path& path,
  const mesh::MeshExportSpace space)
{
  const Image* image = appData.image(imageUid);
  if (!image) {
    return false;
  }
  const ForwardWarpTransform deformation{appData, imageUid};
  const mesh::MeshIO io;
  const auto result = io.write(mesh::MeshWriteRequest{
    .path = path,
    .mesh = &record,
    .coordinateSpace = space,
    .imagePhysicalToWorld = glm::dmat4{image->transformations().worldDef_T_subject()},
    .deformation = space == mesh::MeshExportSpace::CurrentWorld ? &deformation : nullptr,
    .outputAnatomicalSystem = mesh::AnatomicalCoordinateSystem::LPS});
  if (!result) {
    spdlog::error("Could not export mesh {} to {}: {}", record.uid, path, result.error().message);
    native_dialog::showErrorMessageDialog(
      "Mesh Export Failed",
      "The surface mesh could not be saved.",
      result.error().message);
    return false;
  }
  spdlog::info("Exported mesh {} to {}", record.uid, path);
  return true;
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

} // namespace

void exportIsosurface(
  AppData& appData,
  const uuids::uuid& imageUid,
  const uint32_t component,
  const uuids::uuid& surfaceUid)
{
  const Image* image = appData.image(imageUid);
  const Isosurface* surface = appData.isosurface(imageUid, component, surfaceUid);
  if (!image || !surface) {
    return;
  }
  const auto space = chooseCoordinateSpace();
  if (!space) {
    return;
  }
  const std::string defaultName = (surface->name.empty() ? "isosurface" : surface->name) + ".vtp";
  const auto path = native_dialog::saveFile(native_dialog::meshFilters(), {}, defaultName);
  if (!path) {
    return;
  }
  const auto grid = rendering::mesh::scalarGridFromImageComponent(
    *image,
    component,
    image->timeAxis().clamp(image->settings().activeTimePoint()),
    rendering::mesh::MeshCoordinateSpace::ImageSubject);
  const auto generated =
    grid ? rendering::mesh::generateIsoSurfaceMesh(*grid, surface->value, generationOptions(appData, false))
         : std::nullopt;
  if (!generated) {
    native_dialog::showErrorMessageDialog("Mesh Export Failed", "No isosurface mesh could be generated.");
    return;
  }
  const mesh::MeshRecord record = meshRecord(surfaceUid, imageUid, *generated);
  writeMesh(appData, imageUid, record, *path, *space);
}

void exportSegmentationLabel(
  AppData& appData,
  const uuids::uuid& imageUid,
  const uuids::uuid& segmentationUid,
  const std::size_t labelIndex)
{
  const Image* segmentation = appData.seg(segmentationUid);
  if (!segmentation) {
    return;
  }
  const auto space = chooseCoordinateSpace();
  if (!space) {
    return;
  }
  const auto path =
    native_dialog::saveFile(native_dialog::meshFilters(), {}, std::format("segmentation_label-{}.vtp", labelIndex));
  if (!path) {
    return;
  }
  const uint32_t timePoint = segmentation->timeAxis().clamp(segmentation->settings().activeTimePoint());
  const auto inventory = rendering::mesh::segmentationLabelInventory(*segmentation, 0, timePoint);
  if (!inventory) {
    native_dialog::showErrorMessageDialog("Mesh Export Failed", "The selected label contains no voxels.");
    return;
  }
  const auto found = inventory->find(static_cast<int64_t>(labelIndex));
  if (found == inventory->end()) {
    native_dialog::showErrorMessageDialog("Mesh Export Failed", "The selected label contains no voxels.");
    return;
  }
  const auto grid = rendering::mesh::labelMaskGridFromImageComponent(
    *segmentation,
    0,
    static_cast<int64_t>(labelIndex),
    found->second,
    timePoint,
    rendering::mesh::MeshCoordinateSpace::ImageSubject);
  const auto generated =
    grid ? rendering::mesh::generateBinaryMaskSurface(*grid, generationOptions(appData, true)) : std::nullopt;
  if (!generated) {
    native_dialog::showErrorMessageDialog("Mesh Export Failed", "No mesh could be generated for the selected label.");
    return;
  }
  const mesh::MeshRecord record = meshRecord(generateRandomUuid(), imageUid, *generated);
  writeMesh(appData, imageUid, record, *path, *space);
}

void exportAllSegmentationLabels(AppData& appData, const uuids::uuid& imageUid, const uuids::uuid& segmentationUid)
{
  const Image* segmentation = appData.seg(segmentationUid);
  if (!segmentation) {
    return;
  }
  const auto proceed = native_dialog::showMessageDialog(
    {.title = "Export All Segmentation Labels",
     .message = "One mesh file will be written for every non-empty label.",
     .informativeText = "Each output name will be suffixed with its label index, for example '_label-12'.",
     .firstButton = "Continue",
     .secondButton = "Cancel"});
  if (proceed && *proceed != native_dialog::MessageDialogResult::FirstButton) {
    return;
  }
  const auto space = chooseCoordinateSpace();
  if (!space) {
    return;
  }
  const auto basePath = native_dialog::saveFile(native_dialog::meshFilters(), {}, "segmentation.vtp");
  if (!basePath) {
    return;
  }
  const uint32_t timePoint = segmentation->timeAxis().clamp(segmentation->settings().activeTimePoint());
  const auto inventory = rendering::mesh::segmentationLabelInventory(*segmentation, 0, timePoint);
  if (!inventory) {
    native_dialog::showErrorMessageDialog("Mesh Export Failed", "Segmentation labels could not be read.");
    return;
  }
  std::size_t exported = 0;
  for (const auto& [labelValue, bounds] : *inventory) {
    if (labelValue <= 0) {
      continue;
    }
    const auto grid = rendering::mesh::labelMaskGridFromImageComponent(
      *segmentation,
      0,
      labelValue,
      bounds,
      timePoint,
      rendering::mesh::MeshCoordinateSpace::ImageSubject);
    const auto generated =
      grid ? rendering::mesh::generateBinaryMaskSurface(*grid, generationOptions(appData, true)) : std::nullopt;
    if (!generated) {
      spdlog::warn("Skipping segmentation label {} during mesh export because it produced no triangles", labelValue);
      continue;
    }
    const mesh::MeshRecord record = meshRecord(generateRandomUuid(), imageUid, *generated);
    exported +=
      writeMesh(appData, imageUid, record, labelPath(*basePath, static_cast<std::size_t>(labelValue)), *space);
  }
  spdlog::info("Exported {} segmentation label meshes for segmentation {}", exported, segmentationUid);
}

} // namespace mesh_export
