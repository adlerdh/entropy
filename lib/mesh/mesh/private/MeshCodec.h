#pragma once

#include "mesh/MeshIO.h"

#include <expected>

namespace mesh::detail
{

/// Geometry and coordinate metadata decoded by toolkit before construction of a MeshRecord
struct DecodedMesh
{
  MeshGeometry geometry;
  MeshCoordinateMetadata coordinates;
  std::vector<MeshIoDiagnostic> diagnostics;
};

[[nodiscard]] std::expected<DecodedMesh, MeshIoError> readVtkMesh(const std::filesystem::path& path, MeshFormat format);

[[nodiscard]] std::expected<void, MeshIoError>
writeVtkMesh(const std::filesystem::path& path, MeshFormat format, const MeshGeometry& geometry);

[[nodiscard]] std::expected<DecodedMesh, MeshIoError> readItkMesh(const std::filesystem::path& path, MeshFormat format);

[[nodiscard]] std::expected<void, MeshIoError> writeItkMesh(
  const std::filesystem::path& path,
  MeshFormat format,
  const MeshGeometry& geometry,
  const MeshCoordinateMetadata& coordinates);

} // namespace mesh::detail
