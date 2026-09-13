#pragma once

#include "mesh/MeshIO.h"

#include <array>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace mesh::detail
{

/// Volume geometry embedded in a FreeSurfer surface and needed to convert tkRegRAS to scanner RAS
struct FreeSurferVolumeGeometry
{
  bool verticesUseScannerRas = false;
  std::array<uint32_t, 3> dimensions{};
  glm::dvec3 voxelSize{0.0};
  glm::dvec3 xDirection{0.0};
  glm::dvec3 yDirection{0.0};
  glm::dvec3 zDirection{0.0};
  glm::dvec3 centerRas{0.0};
};

[[nodiscard]] std::expected<std::optional<FreeSurferVolumeGeometry>, MeshIoError> readFreeSurferVolumeGeometry(
  const std::filesystem::path& path);

[[nodiscard]] std::expected<glm::dmat4, MeshTransformError> freeSurferSurfaceRasToScannerRas(
  const FreeSurferVolumeGeometry& geometry);

} // namespace mesh::detail
