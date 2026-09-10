#include "mesh/MeshIO.h"

#include "mesh/MeshFormat.h"
#include "mesh/private/MeshCodec.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <system_error>

namespace mesh
{
namespace
{
bool usesVtkCodec(MeshFormat format)
{
  return format == MeshFormat::Vtp || format == MeshFormat::LegacyVtk || format == MeshFormat::Stl ||
         format == MeshFormat::Ply || format == MeshFormat::Obj;
}

bool isFreeSurfer(MeshFormat format)
{
  return format == MeshFormat::FreeSurferBinary || format == MeshFormat::FreeSurferAscii;
}

std::optional<glm::dmat4> anatomicalConversion(
  AnatomicalCoordinateSystem source,
  AnatomicalCoordinateSystem destination)
{
  if (
    source == AnatomicalCoordinateSystem::Unspecified || destination == AnatomicalCoordinateSystem::Unspecified ||
    source == destination)
  {
    return std::nullopt;
  }
  return rasToLpsMatrix(); // The RAS/LPS conversion is its own inverse
}

std::expected<detail::DecodedMesh, MeshIoError> decode(const std::filesystem::path& path, MeshFormat format)
{
  return usesVtkCodec(format) ? detail::readVtkMesh(path, format) : detail::readItkMesh(path, format);
}

MeshFormat sniffFreeSurferFormat(const std::filesystem::path& path, MeshFormat fallback)
{
  std::ifstream input(path, std::ios::binary);
  std::array<unsigned char, 8> header{};

  input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
  if (input.gcount() >= 3 && header[0] == 255u && header[1] == 255u && header[2] == 254u) {
    return MeshFormat::FreeSurferBinary;
  }

  constexpr std::array<unsigned char, 7> sk_asciiHeader{'#', '!', 'a', 's', 'c', 'i', 'i'};

  if (
    input.gcount() >= static_cast<std::streamsize>(sk_asciiHeader.size()) &&
    std::equal(sk_asciiHeader.begin(), sk_asciiHeader.end(), header.begin()))
  {
    return MeshFormat::FreeSurferAscii;
  }
  return fallback;
}
} // namespace

std::expected<MeshLoadResult, MeshIoError> MeshIO::load(const MeshLoadRequest& request) const
{
  if (request.path.empty()) {
    return std::unexpected(
      MeshIoError{MeshIoErrorCode::InvalidRequest, request.path, "No mesh input path was provided"});
  }

  if (request.meshUid.empty() || request.associatedImageUid.empty()) {
    return std::unexpected(MeshIoError{
      MeshIoErrorCode::InvalidRequest,
      request.path,
      "Mesh UID and associated image UID must both be provided"});
  }

  std::error_code error;
  const std::filesystem::file_status status = std::filesystem::status(request.path, error);

  if (error) {
    if (error == std::errc::no_such_file_or_directory) {
      return std::unexpected(
        MeshIoError{MeshIoErrorCode::FileNotFound, request.path, "Mesh input file does not exist", error});
    }

    return std::unexpected(MeshIoError{
      MeshIoErrorCode::FileAccessFailed,
      request.path,
      "Could not inspect mesh input file: " + error.message(),
      error});
  }

  if (!std::filesystem::exists(status)) {
    return std::unexpected(MeshIoError{MeshIoErrorCode::FileNotFound, request.path, "Mesh input file does not exist"});
  }

  if (!std::filesystem::is_regular_file(status)) {
    return std::unexpected(
      MeshIoError{MeshIoErrorCode::FileAccessFailed, request.path, "Mesh input path is not a regular file"});
  }

  const auto pathFormat = formatFromPath(request.path);
  if (!pathFormat) {
    return std::unexpected(
      MeshIoError{MeshIoErrorCode::UnsupportedFormat, request.path, "Unsupported mesh filename or extension"});
  }

  const MeshFormat format = isFreeSurfer(*pathFormat) ? sniffFreeSurferFormat(request.path, *pathFormat) : *pathFormat;
  auto decoded = decode(request.path, format);
  if (!decoded) return std::unexpected(decoded.error());

  glm::dmat4 sourceToImage{1.0};
  bool applyTransform = false;

  if (request.sourceToImagePhysical) {
    sourceToImage = *request.sourceToImagePhysical;
    applyTransform = true;
  }
  else if (isFreeSurfer(format)) {
    if (request.freeSurferCoordinates == FreeSurferCoordinatePolicy::AssumeImagePhysical) {
      decoded->diagnostics.push_back(
        {MeshIoDiagnostic::Severity::Warning,
         "FreeSurfer vertices were explicitly assumed to use image physical coordinates"});
    }
    else if (!decoded->coordinates.embeddedSourceTransform) {
      return std::unexpected(MeshIoError{
        MeshIoErrorCode::AmbiguousCoordinates,
        request.path,
        "FreeSurfer surface has no usable volume geometry. Supply an explicit source-to-image transform or explicitly "
        "assume image physical coordinates."});
    }
    else {
      sourceToImage = *decoded->coordinates.embeddedSourceTransform;
      if (auto conversion = anatomicalConversion(AnatomicalCoordinateSystem::RAS, request.imagePhysicalSystem)) {
        sourceToImage = *conversion * sourceToImage;
      }
      applyTransform = true;
    }
  }
  else if (format == MeshFormat::Gifti) {
    decoded->coordinates.anatomicalSystem = AnatomicalCoordinateSystem::RAS;
    if (decoded->coordinates.embeddedSourceTransform) {
      sourceToImage = *decoded->coordinates.embeddedSourceTransform;
      applyTransform = true;
    }

    if (auto conversion = anatomicalConversion(AnatomicalCoordinateSystem::RAS, request.imagePhysicalSystem)) {
      sourceToImage = *conversion * sourceToImage;
      applyTransform = true;
    }
  }

  if (applyTransform) {
    auto transformed = transformGeometry(decoded->geometry, sourceToImage);
    if (!transformed) {
      return std::unexpected(MeshIoError{MeshIoErrorCode::TransformFailed, request.path, transformed.error().message});
    }

    decoded->geometry = std::move(*transformed);
    decoded->coordinates.sourceToImagePhysical = sourceToImage;
    decoded->coordinates.sourceToImagePhysicalApplied = true;
  }

  decoded->coordinates.anatomicalSystem = request.imagePhysicalSystem;

  MeshRecord mesh;
  mesh.uid = request.meshUid;
  mesh.associatedImageUid = request.associatedImageUid;
  mesh.sourcePath = request.path;
  mesh.sourceFormat = format;
  mesh.geometry = std::move(decoded->geometry);
  mesh.coordinates = std::move(decoded->coordinates);

  return MeshLoadResult{std::move(mesh), std::move(decoded->diagnostics)};
}

std::expected<void, MeshIoError> MeshIO::write(const MeshWriteRequest& request) const
{
  if (request.path.empty()) {
    return std::unexpected(
      MeshIoError{MeshIoErrorCode::InvalidRequest, request.path, "No mesh output path was provided"});
  }

  if (!request.mesh) {
    return std::unexpected(
      MeshIoError{MeshIoErrorCode::InvalidRequest, request.path, "No mesh was supplied for export"});
  }

  const auto format = request.format ? request.format : formatFromPath(request.path);
  if (!format) {
    return std::unexpected(
      MeshIoError{MeshIoErrorCode::UnsupportedFormat, request.path, "Unsupported mesh filename or extension"});
  }

  if (auto valid = validateGeometry(request.mesh->geometry); !valid) {
    return std::unexpected(MeshIoError{MeshIoErrorCode::InvalidData, request.path, valid.error().message});
  }

  MeshGeometry outputGeometry = request.mesh->geometry;

  if (request.coordinateSpace == MeshExportSpace::CurrentWorld) {
    auto transformed = transformGeometry(outputGeometry, request.imagePhysicalToWorld, request.deformation);
    if (!transformed)
      return std::unexpected(MeshIoError{MeshIoErrorCode::TransformFailed, request.path, transformed.error().message});
    outputGeometry = std::move(*transformed);
  }

  AnatomicalCoordinateSystem destination = request.outputAnatomicalSystem;
  if (destination == AnatomicalCoordinateSystem::Unspecified && *format == MeshFormat::Gifti) {
    destination = AnatomicalCoordinateSystem::RAS;
  }

  if (auto conversion = anatomicalConversion(request.mesh->coordinates.anatomicalSystem, destination)) {
    auto transformed = transformGeometry(outputGeometry, *conversion);
    if (!transformed) {
      return std::unexpected(MeshIoError{MeshIoErrorCode::TransformFailed, request.path, transformed.error().message});
    }
    outputGeometry = std::move(*transformed);
  }

  if (usesVtkCodec(*format)) {
    return detail::writeVtkMesh(request.path, *format, outputGeometry);
  }

  MeshCoordinateMetadata outputCoordinates;
  outputCoordinates.storedSpace = MeshCoordinateSpace::ImagePhysical;
  outputCoordinates.anatomicalSystem =
    destination == AnatomicalCoordinateSystem::Unspecified ? request.mesh->coordinates.anatomicalSystem : destination;
  outputCoordinates.coordinatesAreWorldSpace = request.coordinateSpace == MeshExportSpace::CurrentWorld;

  return detail::writeItkMesh(request.path, *format, outputGeometry, outputCoordinates);
}

} // namespace mesh
