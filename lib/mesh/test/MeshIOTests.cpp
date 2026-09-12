#include "mesh/MeshIO.h"

#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string_view>

using namespace mesh;

namespace
{
MeshRecord tetrahedron()
{
  MeshRecord mesh;
  mesh.uid = "mesh-id";
  mesh.associatedImageUid = "image-id";
  mesh.geometry.positions = {{0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {0.0f, 3.0f, 0.0f}, {0.0f, 0.0f, 4.0f}};
  mesh.geometry.triangleIndices = {0u, 2u, 1u, 0u, 1u, 3u, 0u, 3u, 2u, 1u, 2u, 3u};
  regenerateNormals(mesh.geometry);
  mesh.coordinates.anatomicalSystem = AnatomicalCoordinateSystem::LPS;
  return mesh;
}

std::vector<glm::vec3> sortedPositions(const MeshGeometry& geometry)
{
  std::vector<glm::vec3> positions = geometry.positions;
  std::ranges::sort(positions, [](const glm::vec3& left, const glm::vec3& right) {
    if (left.x != right.x) return left.x < right.x;
    if (left.y != right.y) return left.y < right.y;
    return left.z < right.z;
  });
  return positions;
}

void checkCoordinateFidelity(const MeshGeometry& actual, const MeshGeometry& expected)
{
  const auto actualPositions = sortedPositions(actual);
  const auto expectedPositions = sortedPositions(expected);
  REQUIRE(actualPositions.size() == expectedPositions.size());
  for (std::size_t index = 0; index < expectedPositions.size(); ++index) {
    CHECK(actualPositions[index].x == Catch::Approx(expectedPositions[index].x).margin(1.0e-5));
    CHECK(actualPositions[index].y == Catch::Approx(expectedPositions[index].y).margin(1.0e-5));
    CHECK(actualPositions[index].z == Catch::Approx(expectedPositions[index].z).margin(1.0e-5));
  }
}

class NonlinearExportTransform final : public IPointTransform
{
public:
  [[nodiscard]] std::expected<glm::dvec3, std::string> transformPoint(const glm::dvec3& point) const override
  {
    return glm::dvec3{point.x, point.y, point.z + 0.25 * point.x * point.x};
  }
};

std::filesystem::path temporaryPath(std::string_view extension)
{
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() / ("entropy-mesh-io-" + std::to_string(stamp) + std::string(extension));
}

struct RemoveFile
{
  std::filesystem::path path;
  ~RemoveFile()
  {
    std::error_code error;
    std::filesystem::remove(path, error);
  }
};

void writeBigEndianUint32(std::ostream& output, uint32_t value)
{
  output.put(static_cast<char>((value >> 24u) & 0xffu));
  output.put(static_cast<char>((value >> 16u) & 0xffu));
  output.put(static_cast<char>((value >> 8u) & 0xffu));
  output.put(static_cast<char>(value & 0xffu));
}

void appendFreeSurferVolumeGeometry(const std::filesystem::path& path, std::string_view xDirection = "-1 0 0")
{
  std::ofstream output(path, std::ios::binary | std::ios::app);
  writeBigEndianUint32(output, 2u);
  writeBigEndianUint32(output, 0u);
  writeBigEndianUint32(output, 20u);
  output << "valid = 1  # volume info valid\n"
            "filename = orig.mgz\n"
            "volume = 256 256 256\n"
            "voxelsize = 1 1 1\n"
            "xras = "
         << xDirection
         << "\n"
            "yras = 0 0 -1\n"
            "zras = 0 1 0\n"
            "cras = 10 20 30\n";
}
} // namespace

TEST_CASE("Supported toolkit formats round trip compact triangle geometry")
{
  const MeshIO meshIo;
  const std::string_view extension =
    GENERATE(".vtp", ".vtk", ".stl", ".ply", ".obj", ".off", ".gii", ".fsb", ".fcv", ".fsa", ".pial");
  const RemoveFile output{temporaryPath(extension)};
  MeshRecord source = tetrahedron();
  MeshWriteRequest writeRequest;
  writeRequest.path = output.path;
  writeRequest.mesh = &source;
  REQUIRE(meshIo.write(writeRequest));

  MeshLoadRequest loadRequest;
  loadRequest.path = output.path;
  loadRequest.meshUid = "loaded-mesh";
  loadRequest.associatedImageUid = "loaded-image";
  if (extension == ".fsb" || extension == ".fcv" || extension == ".fsa" || extension == ".pial") {
    loadRequest.freeSurferCoordinates = FreeSurferCoordinatePolicy::AssumeImagePhysical;
  }
  const auto loaded = meshIo.load(loadRequest);
  REQUIRE(loaded);
  CHECK(loaded->mesh.uid == "loaded-mesh");
  CHECK(loaded->mesh.associatedImageUid == "loaded-image");
  CHECK(loaded->mesh.geometry.positions.size() == source.geometry.positions.size());
  CHECK(loaded->mesh.geometry.triangleCount() == source.geometry.triangleCount());
  CHECK(loaded->mesh.geometry.normals.size() == loaded->mesh.geometry.positions.size());
  checkCoordinateFidelity(loaded->mesh.geometry, source.geometry);
  CHECK(std::ranges::all_of(loaded->mesh.geometry.normals, [](const glm::vec3& normal) {
    return std::isfinite(normal.x) && std::isfinite(normal.y) && std::isfinite(normal.z) &&
           glm::length(normal) == Catch::Approx(1.0f).margin(1.0e-5);
  }));
}

TEST_CASE("Independently authored mesh files interoperate with the supported readers")
{
  const MeshIO meshIo;
  const auto checkFixture = [&meshIo](std::string_view extension, std::string_view contents) {
    const RemoveFile input{temporaryPath(extension)};
    {
      std::ofstream stream{input.path, std::ios::binary};
      REQUIRE(stream);
      stream << contents;
      REQUIRE(stream.good());
    }
    MeshLoadRequest request;
    request.path = input.path;
    request.meshUid = "fixture-mesh";
    request.associatedImageUid = "fixture-image";
    if (extension == ".pial") request.freeSurferCoordinates = FreeSurferCoordinatePolicy::AssumeImagePhysical;
    const auto loaded = meshIo.load(request);
    REQUIRE(loaded);
    CHECK(loaded->mesh.geometry.positions.size() == 4u);
    CHECK(loaded->mesh.geometry.triangleCount() == 4u);
    checkCoordinateFidelity(loaded->mesh.geometry, tetrahedron().geometry);
  };

  SECTION("legacy VTK ASCII")
  {
    checkFixture(
      ".vtk",
      "# vtk DataFile Version 3.0\n"
      "independent tetrahedron\n"
      "ASCII\n"
      "DATASET POLYDATA\n"
      "POINTS 4 float\n"
      "0 0 0  2 0 0  0 3 0  0 0 4\n"
      "POLYGONS 4 16\n"
      "3 0 2 1\n3 0 1 3\n3 0 3 2\n3 1 2 3\n");
  }
  SECTION("OBJ")
  {
    checkFixture(
      ".obj",
      "# independent tetrahedron\n"
      "v 0 0 0\nv 2 0 0\nv 0 3 0\nv 0 0 4\n"
      "f 1 3 2\nf 1 2 4\nf 1 4 3\nf 2 3 4\n");
  }
  SECTION("OFF")
  {
    checkFixture(
      ".off",
      "OFF\n4 4 0\n"
      "0 0 0\n2 0 0\n0 3 0\n0 0 4\n"
      "3 0 2 1\n3 0 1 3\n3 0 3 2\n3 1 2 3\n");
  }
  SECTION("PLY ASCII")
  {
    checkFixture(
      ".ply",
      "ply\nformat ascii 1.0\n"
      "element vertex 4\nproperty float x\nproperty float y\nproperty float z\n"
      "element face 4\nproperty list uchar int vertex_indices\nend_header\n"
      "0 0 0\n2 0 0\n0 3 0\n0 0 4\n"
      "3 0 2 1\n3 0 1 3\n3 0 3 2\n3 1 2 3\n");
  }
  SECTION("native FreeSurfer filename with ASCII content")
  {
    checkFixture(
      ".pial",
      "#!ascii version of independently authored tetrahedron\n"
      "4 4\n"
      "0 0 0 0\n2 0 0 0\n0 3 0 0\n0 0 4 0\n"
      "0 2 1 0\n0 1 3 0\n0 3 2 0\n1 2 3 0\n");
  }
}

TEST_CASE("FreeSurfer volume geometry maps surface RAS through scanner RAS into image LPS")
{
  const MeshIO meshIo;
  const RemoveFile output{temporaryPath(".pial")};
  MeshRecord source = tetrahedron();
  source.coordinates.anatomicalSystem = AnatomicalCoordinateSystem::RAS;
  MeshWriteRequest writeRequest;
  writeRequest.path = output.path;
  writeRequest.mesh = &source;
  REQUIRE(meshIo.write(writeRequest));
  appendFreeSurferVolumeGeometry(output.path);

  MeshLoadRequest loadRequest;
  loadRequest.path = output.path;
  loadRequest.meshUid = "mesh";
  loadRequest.associatedImageUid = "image";
  loadRequest.imagePhysicalSystem = AnatomicalCoordinateSystem::LPS;
  const auto loaded = meshIo.load(loadRequest);
  REQUIRE(loaded);
  REQUIRE_FALSE(loaded->mesh.geometry.positions.empty());
  CHECK(loaded->mesh.geometry.positions[0].x == Catch::Approx(-10.0f));
  CHECK(loaded->mesh.geometry.positions[0].y == Catch::Approx(-20.0f));
  CHECK(loaded->mesh.geometry.positions[0].z == Catch::Approx(30.0f));
  CHECK(loaded->mesh.coordinates.sourceToImagePhysicalApplied);
}

TEST_CASE("Current-world export bakes affine and nonlinear transformations")
{
  const MeshIO meshIo;
  const RemoveFile output{temporaryPath(".vtp")};
  MeshRecord source = tetrahedron();
  glm::dmat4 transform{1.0};
  transform[3][0] = 10.0;
  const NonlinearExportTransform deformation;
  MeshWriteRequest request;
  request.path = output.path;
  request.mesh = &source;
  request.coordinateSpace = MeshExportSpace::CurrentWorld;
  request.imagePhysicalToWorld = transform;
  request.deformation = &deformation;
  REQUIRE(meshIo.write(request));
  MeshLoadRequest loadRequest;
  loadRequest.path = output.path;
  loadRequest.meshUid = "mesh";
  loadRequest.associatedImageUid = "image";
  const auto loaded = meshIo.load(loadRequest);
  REQUIRE(loaded);
  CHECK(loaded->mesh.geometry.positions[0].x == Catch::Approx(10.0f));
  CHECK(loaded->mesh.geometry.positions[0].z == Catch::Approx(25.0f));
  CHECK(std::ranges::any_of(loaded->mesh.geometry.positions, [](const glm::vec3& point) {
    return point.x == Catch::Approx(12.0f) && point.z == Catch::Approx(36.0f);
  }));
}

TEST_CASE("Imported meshes default to three-dimensional visibility")
{
  const MeshDisplaySettings display;
  CHECK_FALSE(display.visibleIn2d);
  CHECK(display.visibleIn3d);
}

TEST_CASE("FreeSurfer coordinates without volume geometry require an explicit policy")
{
  const MeshIO meshIo;
  const RemoveFile output{temporaryPath(".fsb")};
  MeshRecord source = tetrahedron();
  MeshWriteRequest writeRequest;
  writeRequest.path = output.path;
  writeRequest.mesh = &source;
  REQUIRE(meshIo.write(writeRequest));
  MeshLoadRequest loadRequest;
  loadRequest.path = output.path;
  loadRequest.meshUid = "mesh";
  loadRequest.associatedImageUid = "image";
  const auto loaded = meshIo.load(loadRequest);
  REQUIRE_FALSE(loaded);
  CHECK(loaded.error().code == MeshIoErrorCode::AmbiguousCoordinates);
}

TEST_CASE("FreeSurfer volume geometry rejects non-orthonormal direction vectors")
{
  const MeshIO meshIo;
  const RemoveFile output{temporaryPath(".pial")};
  MeshRecord source = tetrahedron();
  source.coordinates.anatomicalSystem = AnatomicalCoordinateSystem::RAS;
  MeshWriteRequest writeRequest;
  writeRequest.path = output.path;
  writeRequest.mesh = &source;
  REQUIRE(meshIo.write(writeRequest));
  appendFreeSurferVolumeGeometry(output.path, "0 0 -1");

  MeshLoadRequest loadRequest;
  loadRequest.path = output.path;
  loadRequest.meshUid = "mesh";
  loadRequest.associatedImageUid = "image";
  const auto loaded = meshIo.load(loadRequest);
  REQUIRE_FALSE(loaded);
  CHECK(loaded.error().code == MeshIoErrorCode::InvalidData);
}

TEST_CASE("FreeSurfer adapter recognizes ASCII content behind a native surface name")
{
  const MeshIO meshIo;
  const RemoveFile output{temporaryPath(".pial")};
  MeshRecord source = tetrahedron();
  MeshWriteRequest writeRequest;
  writeRequest.path = output.path;
  writeRequest.mesh = &source;
  writeRequest.format = MeshFormat::FreeSurferAscii;
  REQUIRE(meshIo.write(writeRequest));

  MeshLoadRequest loadRequest;
  loadRequest.path = output.path;
  loadRequest.meshUid = "mesh";
  loadRequest.associatedImageUid = "image";
  loadRequest.freeSurferCoordinates = FreeSurferCoordinatePolicy::AssumeImagePhysical;
  const auto loaded = meshIo.load(loadRequest);
  REQUIRE(loaded);
  CHECK(loaded->mesh.sourceFormat == MeshFormat::FreeSurferAscii);
  CHECK(loaded->mesh.geometry.triangleCount() == source.geometry.triangleCount());
}

TEST_CASE("Mesh I/O reports invalid requests and missing inputs with actionable codes")
{
  const MeshIO meshIo;

  MeshLoadRequest emptyRequest;
  auto loaded = meshIo.load(emptyRequest);
  REQUIRE_FALSE(loaded);
  CHECK(loaded.error().code == MeshIoErrorCode::InvalidRequest);

  MeshLoadRequest missingRequest;
  missingRequest.path = temporaryPath(".vtp");
  missingRequest.meshUid = "mesh";
  missingRequest.associatedImageUid = "image";
  loaded = meshIo.load(missingRequest);
  REQUIRE_FALSE(loaded);
  CHECK(loaded.error().code == MeshIoErrorCode::FileNotFound);
  CHECK(loaded.error().path == missingRequest.path);

  MeshWriteRequest writeRequest;
  writeRequest.path = temporaryPath(".vtp");
  const auto written = meshIo.write(writeRequest);
  REQUIRE_FALSE(written);
  CHECK(written.error().code == MeshIoErrorCode::InvalidRequest);
}

TEST_CASE("Mesh I/O distinguishes unsupported formats, invalid geometry, and transform failures")
{
  const MeshIO meshIo;
  const RemoveFile unsupported{temporaryPath(".txt")};
  std::ofstream{unsupported.path} << "not a mesh";

  MeshLoadRequest loadRequest;
  loadRequest.path = unsupported.path;
  loadRequest.meshUid = "mesh";
  loadRequest.associatedImageUid = "image";
  const auto loaded = meshIo.load(loadRequest);
  REQUIRE_FALSE(loaded);
  CHECK(loaded.error().code == MeshIoErrorCode::UnsupportedFormat);

  MeshRecord invalidMesh;
  MeshWriteRequest invalidRequest;
  invalidRequest.path = temporaryPath(".vtp");
  invalidRequest.mesh = &invalidMesh;
  auto written = meshIo.write(invalidRequest);
  REQUIRE_FALSE(written);
  CHECK(written.error().code == MeshIoErrorCode::InvalidData);

  MeshRecord source = tetrahedron();
  MeshWriteRequest transformRequest;
  transformRequest.path = temporaryPath(".vtp");
  transformRequest.mesh = &source;
  transformRequest.coordinateSpace = MeshExportSpace::CurrentWorld;
  transformRequest.imagePhysicalToWorld[0][3] = 0.25;
  written = meshIo.write(transformRequest);
  REQUIRE_FALSE(written);
  CHECK(written.error().code == MeshIoErrorCode::TransformFailed);
}

TEST_CASE("Mesh loading reports an invalid source coordinate transform as a transform failure")
{
  const MeshIO meshIo;
  const RemoveFile output{temporaryPath(".vtp")};
  MeshRecord source = tetrahedron();
  MeshWriteRequest writeRequest;
  writeRequest.path = output.path;
  writeRequest.mesh = &source;
  REQUIRE(meshIo.write(writeRequest));

  MeshLoadRequest loadRequest;
  loadRequest.path = output.path;
  loadRequest.meshUid = "mesh";
  loadRequest.associatedImageUid = "image";
  glm::dmat4 nonAffine{1.0};
  nonAffine[0][3] = 0.25;
  loadRequest.sourceToImagePhysical = nonAffine;

  const auto loaded = meshIo.load(loadRequest);
  REQUIRE_FALSE(loaded);
  CHECK(loaded.error().code == MeshIoErrorCode::TransformFailed);
}
