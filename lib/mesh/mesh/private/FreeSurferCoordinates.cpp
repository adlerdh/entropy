#include "mesh/private/FreeSurferCoordinates.h"

#include "mesh/MeshTransform.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <glm/geometric.hpp>
#include <limits>
#include <sstream>
#include <string>

namespace mesh::detail
{
namespace
{
std::expected<uint32_t, MeshIoError> readBigEndianUint32(std::istream& stream, const std::filesystem::path& path)
{
  std::array<unsigned char, 4> bytes{};
  stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

  if (!stream) {
    return std::unexpected(MeshIoError{MeshIoErrorCode::InvalidData, path, "Truncated FreeSurfer surface"});
  }

  return (static_cast<uint32_t>(bytes[0]) << 24u) | (static_cast<uint32_t>(bytes[1]) << 16u) |
         (static_cast<uint32_t>(bytes[2]) << 8u) | static_cast<uint32_t>(bytes[3]);
}

std::string valueAfterEquals(const std::string& line)
{
  const std::size_t equals = line.find('=');
  if (equals == std::string::npos) {
    return {};
  }
  return line.substr(equals + 1u);
}

template<class T>
bool parseTriple(const std::string& line, T& x, T& y, T& z)
{
  std::istringstream input(valueAfterEquals(line));
  return static_cast<bool>(input >> x >> y >> z);
}

bool parseDimensions(const std::string& line, std::array<uint32_t, 3>& dimensions)
{
  std::array<uint64_t, 3> parsed{};

  if (!parseTriple(line, parsed[0], parsed[1], parsed[2])) {
    return false;
  }

  for (std::size_t axis = 0; axis < parsed.size(); ++axis) {
    if (parsed[axis] == 0u || parsed[axis] > std::numeric_limits<uint32_t>::max()) return false;
    dimensions[axis] = static_cast<uint32_t>(parsed[axis]);
  }
  return true;
}

bool finite(const glm::dvec3& vector)
{
  return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

bool validDirectionFrame(const FreeSurferVolumeGeometry& geometry)
{
  constexpr double sk_directionTolerance = 1.0e-3;
  const auto unitLength = [](const glm::dvec3& direction) {
    return std::abs(glm::length(direction) - 1.0) <= sk_directionTolerance;
  };
  const auto orthogonal = [](const glm::dvec3& lhs, const glm::dvec3& rhs) {
    return std::abs(glm::dot(lhs, rhs)) <= sk_directionTolerance;
  };

  return finite(geometry.xDirection) && finite(geometry.yDirection) && finite(geometry.zDirection) &&
         unitLength(geometry.xDirection) && unitLength(geometry.yDirection) && unitLength(geometry.zDirection) &&
         orthogonal(geometry.xDirection, geometry.yDirection) && orthogonal(geometry.xDirection, geometry.zDirection) &&
         orthogonal(geometry.yDirection, geometry.zDirection);
}

glm::dmat4 affineMatrix(const glm::dmat3& linear, const glm::dvec3& translation)
{
  return {
    glm::dvec4{linear[0], 0.0},
    glm::dvec4{linear[1], 0.0},
    glm::dvec4{linear[2], 0.0},
    glm::dvec4{translation, 1.0}};
}

bool readExpectedLine(std::istream& stream, std::string_view key, std::string& value)
{
  if (!std::getline(stream, value)) {
    return false;
  }

  const std::size_t equals = value.find('=');

  if (equals == std::string::npos) {
    return false;
  }

  std::string field = value.substr(0, equals);

  while (!field.empty() && std::isspace(static_cast<unsigned char>(field.back()))) {
    field.pop_back();
  }

  const auto first = std::ranges::find_if_not(field, [](unsigned char c) { return std::isspace(c); });
  field.erase(field.begin(), first);
  return field == key;
}
} // namespace

std::expected<std::optional<FreeSurferVolumeGeometry>, MeshIoError> readFreeSurferVolumeGeometry(
  const std::filesystem::path& path)
{
  std::ifstream input(path, std::ios::binary);

  if (!input) {
    return std::unexpected(MeshIoError{MeshIoErrorCode::ReadFailed, path, "Cannot open FreeSurfer surface"});
  }

  std::array<unsigned char, 3> magic{};
  input.read(reinterpret_cast<char*>(magic.data()), static_cast<std::streamsize>(magic.size()));

  if (!input || magic != std::array<unsigned char, 3>{255u, 255u, 254u}) {
    return std::optional<FreeSurferVolumeGeometry>{};
  }

  std::string line;
  if (!std::getline(input, line)) {
    return std::unexpected(MeshIoError{MeshIoErrorCode::InvalidData, path, "Truncated FreeSurfer surface header"});
  }

  if (!std::getline(input, line)) {
    return std::unexpected(MeshIoError{MeshIoErrorCode::InvalidData, path, "Truncated FreeSurfer surface header"});
  }

  auto vertexCount = readBigEndianUint32(input, path);
  auto triangleCount = readBigEndianUint32(input, path);

  if (!vertexCount) {
    return std::unexpected(vertexCount.error());
  }

  if (!triangleCount) {
    return std::unexpected(triangleCount.error());
  }

  constexpr uint64_t sk_vertexBytes = 3u * sizeof(float);
  constexpr uint64_t sk_triangleBytes = 3u * sizeof(uint32_t);
  const uint64_t payloadBytes =
    static_cast<uint64_t>(*vertexCount) * sk_vertexBytes + static_cast<uint64_t>(*triangleCount) * sk_triangleBytes;

  input.seekg(static_cast<std::streamoff>(payloadBytes), std::ios::cur);
  if (!input || input.peek() == std::char_traits<char>::eof()) return std::optional<FreeSurferVolumeGeometry>{};

  auto firstTag = readBigEndianUint32(input, path);
  if (!firstTag) {
    return std::unexpected(firstTag.error());
  }

  bool scannerRas = false;

  if (*firstTag != 20u) {
    auto realRas = readBigEndianUint32(input, path);
    auto volumeGeometryTag = readBigEndianUint32(input, path);

    if (!realRas) {
      return std::unexpected(realRas.error());
    }

    if (!volumeGeometryTag) {
      return std::unexpected(volumeGeometryTag.error());
    }

    if (*firstTag != 2u || (*realRas != 0u && *realRas != 1u) || *volumeGeometryTag != 20u) {
      return std::unexpected(
        MeshIoError{MeshIoErrorCode::InvalidData, path, "Unknown FreeSurfer surface metadata tag"});
    }

    scannerRas = *realRas == 1u;
  }

  std::array<std::string, 8> values;
  constexpr std::array<std::string_view, 8>
    keys{"valid", "filename", "volume", "voxelsize", "xras", "yras", "zras", "cras"};

  for (std::size_t index = 0; index < keys.size(); ++index) {
    if (!readExpectedLine(input, keys[index], values[index])) {
      return std::unexpected(MeshIoError{MeshIoErrorCode::InvalidData, path, "Malformed FreeSurfer volume geometry"});
    }
  }

  FreeSurferVolumeGeometry geometry;
  geometry.verticesUseScannerRas = scannerRas;
  unsigned int volumeGeometryValid = 0u;
  std::istringstream validInput(valueAfterEquals(values[0]));

  if (!(validInput >> volumeGeometryValid) || volumeGeometryValid > 1u) {
    return std::unexpected(
      MeshIoError{MeshIoErrorCode::InvalidData, path, "Invalid FreeSurfer volume-geometry validity flag"});
  }

  if (volumeGeometryValid == 0u) {
    return std::optional<FreeSurferVolumeGeometry>{};
  }

  if (
    !parseDimensions(values[2], geometry.dimensions) ||
    !parseTriple(values[3], geometry.voxelSize.x, geometry.voxelSize.y, geometry.voxelSize.z) ||
    !parseTriple(values[4], geometry.xDirection.x, geometry.xDirection.y, geometry.xDirection.z) ||
    !parseTriple(values[5], geometry.yDirection.x, geometry.yDirection.y, geometry.yDirection.z) ||
    !parseTriple(values[6], geometry.zDirection.x, geometry.zDirection.y, geometry.zDirection.z) ||
    !parseTriple(values[7], geometry.centerRas.x, geometry.centerRas.y, geometry.centerRas.z))
  {
    return std::unexpected(
      MeshIoError{MeshIoErrorCode::InvalidData, path, "Invalid FreeSurfer volume geometry values"});
  }

  return std::optional<FreeSurferVolumeGeometry>{geometry};
}

std::expected<glm::dmat4, MeshTransformError> freeSurferSurfaceRasToScannerRas(const FreeSurferVolumeGeometry& geometry)
{
  if (
    !finite(geometry.voxelSize) || !finite(geometry.centerRas) || geometry.voxelSize.x <= 0.0 ||
    geometry.voxelSize.y <= 0.0 || geometry.voxelSize.z <= 0.0 || !validDirectionFrame(geometry))
  {
    return std::unexpected(MeshTransformError{
      MeshTransformErrorCode::InvalidGeometry,
      "FreeSurfer volume geometry has invalid voxel sizes, center, or direction vectors"});
  }

  const glm::dvec3 halfDimensions{
    geometry.dimensions[0] / 2.0,
    geometry.dimensions[1] / 2.0,
    geometry.dimensions[2] / 2.0};

  const glm::dmat3 voxelsToScannerLinear{
    geometry.xDirection * geometry.voxelSize.x,
    geometry.yDirection * geometry.voxelSize.y,
    geometry.zDirection * geometry.voxelSize.z};

  const glm::dmat4 voxelsToScannerRas =
    affineMatrix(voxelsToScannerLinear, geometry.centerRas - voxelsToScannerLinear * halfDimensions);

  const glm::dmat3 voxelsToSurfaceLinear{
    glm::dvec3{-geometry.voxelSize.x, 0.0, 0.0},
    glm::dvec3{0.0, 0.0, -geometry.voxelSize.y},
    glm::dvec3{0.0, geometry.voxelSize.z, 0.0}};

  const glm::dmat4 voxelsToSurfaceRas = affineMatrix(voxelsToSurfaceLinear, -(voxelsToSurfaceLinear * halfDimensions));
  const auto surfaceRasToVoxels = inverseAffine(voxelsToSurfaceRas);

  if (!surfaceRasToVoxels) {
    return std::unexpected(surfaceRasToVoxels.error());
  }

  const glm::dmat4 result = voxelsToScannerRas * *surfaceRasToVoxels;
  const double determinant = linearDeterminant(result);

  if (!std::isfinite(determinant) || std::abs(determinant) < 1.0e-15) {
    return std::unexpected(MeshTransformError{
      MeshTransformErrorCode::SingularMatrix,
      "FreeSurfer volume geometry contains singular direction vectors"});
  }

  return result;
}

} // namespace mesh::detail
