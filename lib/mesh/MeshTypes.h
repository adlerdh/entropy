#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mesh
{

static_assert(sizeof(glm::vec3) == 3u * sizeof(float), "Mesh positions must remain tightly packed");

/// Surface mesh file formats supported by the default reader and writer
enum class MeshFormat
{
  Vtp,
  LegacyVtk,
  Stl,
  Ply,
  Obj,
  Off,
  Gifti,
  FreeSurferBinary,
  FreeSurferAscii
};

/// Coordinate space in which vertices are stored in the source file
enum class MeshCoordinateSpace
{
  ImagePhysical,
  GiftiCoordinates,
  FreeSurferSurfaceRas,
  ScannerRas
};

/// Anatomical axis convention used by a mesh coordinate space
enum class AnatomicalCoordinateSystem
{
  Unspecified,
  LPS,
  RAS
};

/// Indexed triangle geometry suitable for CPU processing and direct GPU upload
struct MeshGeometry
{
  /// Vertices in image physical space, before the image's registration, manual, or deformable transforms
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<uint32_t> triangleIndices;

  [[nodiscard]] std::size_t triangleCount() const noexcept
  {
    return triangleIndices.size() / 3u;
  }

  bool operator==(const MeshGeometry&) const = default;
};

/// Appearance and visibility state associated with one mesh (independent of the renderer)
struct MeshDisplaySettings
{
  glm::vec3 baseColor{0.8f};
  float opacity = 1.0f;
  bool visible = true;

  bool operator==(const MeshDisplaySettings&) const = default;
};

/// Provenance and transformations used to normalize imported vertices into image physical space
struct MeshCoordinateMetadata
{
  /// Original coordinate spac on disk. Geometry is normalized to image physical space after loading.
  MeshCoordinateSpace storedSpace = MeshCoordinateSpace::ImagePhysical;
  AnatomicalCoordinateSystem anatomicalSystem = AnatomicalCoordinateSystem::Unspecified;

  /// Transform encoded by the source file, retained for provenance
  std::optional<glm::dmat4> embeddedSourceTransform;

  /// Transform used to place source vertices in the associated image's physical space
  std::optional<glm::dmat4> sourceToImagePhysical;

  bool sourceToImagePhysicalApplied = false;
  bool coordinatesAreWorldSpace = false;
  std::string description;

  bool operator==(const MeshCoordinateMetadata&) const = default;
};

/// Complete record for an imported mesh associated with an image
struct MeshRecord
{
  std::string uid;
  std::string associatedImageUid;
  std::string name; //!< User-visible name, initially derived from the source file name
  std::filesystem::path sourcePath;
  MeshFormat sourceFormat = MeshFormat::Vtp;
  MeshGeometry geometry;
  MeshDisplaySettings display;
  MeshCoordinateMetadata coordinates;
};

} // namespace mesh
