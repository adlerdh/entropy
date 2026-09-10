#pragma once

#include "mesh/MeshTransform.h"
#include "mesh/MeshTypes.h"

#include <expected>
#include <filesystem>
#include <glm/ext/matrix_double4x4.hpp>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace mesh
{

/// Categories of mesh I/O failures
enum class MeshIoErrorCode
{
  InvalidRequest,
  UnsupportedFormat,
  FileNotFound,
  FileAccessFailed,
  InvalidData,
  AmbiguousCoordinates,
  TransformFailed,
  ReadFailed,
  WriteFailed
};

/// Failure information returned by mesh loading and writing operations
struct MeshIoError
{
  MeshIoError(
    MeshIoErrorCode errorCode,
    std::filesystem::path errorPath,
    std::string errorMessage,
    std::error_code underlyingError = {})
    : code{errorCode}, path{std::move(errorPath)}, message{std::move(errorMessage)}, systemError{underlyingError}
  {
  }

  MeshIoErrorCode code = MeshIoErrorCode::InvalidData;
  std::filesystem::path path;
  std::string message;
  std::error_code systemError;
};

/// Information produced while interpreting a mesh file
struct MeshIoDiagnostic
{
  enum class Severity
  {
    Information,
    Warning
  };

  Severity severity = Severity::Information;
  std::string message;
};

/// Controls how a FreeSurfer surface without unambiguous coordinate metadata is interpreted
enum class FreeSurferCoordinatePolicy
{
  /// Use the embedded volume geometry and refuse ambiguous files without it
  UseEmbeddedVolumeGeometry,

  /// The caller explicitly asserts that stored vertices already use image physical coordinates
  AssumeImagePhysical
};

/// Inputs required to load a mesh and associate it with an image coordinate system
struct MeshLoadRequest
{
  std::filesystem::path path;
  std::string meshUid;
  std::string associatedImageUid;

  /// Entropy/ITK image physical coordinates are LPS. Other clients may explicitly select RAS.
  AnatomicalCoordinateSystem imagePhysicalSystem = AnatomicalCoordinateSystem::LPS;
  std::optional<glm::dmat4> sourceToImagePhysical;
  FreeSurferCoordinatePolicy freeSurferCoordinates = FreeSurferCoordinatePolicy::UseEmbeddedVolumeGeometry;
};

/// A successfully loaded mesh together with any diagnostics
struct MeshLoadResult
{
  MeshRecord mesh;
  std::vector<MeshIoDiagnostic> diagnostics;
};

/// Selects whether export preserves image physical vertices or bakes in the current world transform
enum class MeshExportSpace
{
  /// Untransformed coordinates associated with the image
  ImagePhysical,

  /// Image affine/manual/registration transform and optional deformation are baked into vertices
  CurrentWorld
};

/// Inputs required to serialize a mesh, including optional coordinate transformations.
/// All pointer members are borrowed and must remain valid for the duration of writing.
struct MeshWriteRequest
{
  std::filesystem::path path;
  const MeshRecord* mesh = nullptr;
  std::optional<MeshFormat> format;
  MeshExportSpace coordinateSpace = MeshExportSpace::ImagePhysical;
  glm::dmat4 imagePhysicalToWorld{1.0};
  const IPointTransform* deformation = nullptr;

  /// Optional explicit RAS/LPS conversion. Unspecified preserves the mesh's physical convention.
  AnatomicalCoordinateSystem outputAnatomicalSystem = AnatomicalCoordinateSystem::Unspecified;
};

/// Mesh loading interface
class IMeshReader
{
public:
  virtual ~IMeshReader() = default;

  /// Load and normalize a mesh into the associated image's physical coordinate system
  [[nodiscard]] virtual std::expected<MeshLoadResult, MeshIoError> load(const MeshLoadRequest& request) const = 0;
};

/// Mesh writing interface
class IMeshWriter
{
public:
  virtual ~IMeshWriter() = default;

  /// Write a mesh in image physical or transformed world coordinates without modifying the source record
  [[nodiscard]] virtual std::expected<void, MeshIoError> write(const MeshWriteRequest& request) const = 0;
};

/// Default implementation for supported surface mesh formats
class MeshIO final : public IMeshReader, public IMeshWriter
{
public:
  /// Load any supported format through this library's private toolkit adapters
  [[nodiscard]] std::expected<MeshLoadResult, MeshIoError> load(const MeshLoadRequest& request) const override;

  /// Write any supported format through this library's private toolkit adapters
  [[nodiscard]] std::expected<void, MeshIoError> write(const MeshWriteRequest& request) const override;
};

} // namespace mesh
