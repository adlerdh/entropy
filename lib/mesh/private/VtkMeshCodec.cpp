#include "mesh/private/MeshCodec.h"

#include "mesh/MeshTransform.h"

#include <vtkCellArray.h>
#include <vtkDataArray.h>
#include <vtkErrorCode.h>
#include <vtkFloatArray.h>
#include <vtkIdList.h>
#include <vtkOBJReader.h>
#include <vtkOBJWriter.h>
#include <vtkPLYReader.h>
#include <vtkPLYWriter.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkPolyDataReader.h>
#include <vtkPolyDataWriter.h>
#include <vtkSTLReader.h>
#include <vtkSTLWriter.h>
#include <vtkSmartPointer.h>
#include <vtkTriangleFilter.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkXMLPolyDataWriter.h>

#include <limits>
#include <cmath>
#include <string>

namespace mesh::detail
{
namespace
{
template<class Reader>
std::expected<void, MeshIoError> readPolyData(const std::filesystem::path& path, vtkSmartPointer<vtkPolyData>& polyData)
{
  auto reader = vtkSmartPointer<Reader>::New();
  reader->SetFileName(path.string().c_str());
  reader->Update();

  if (reader->GetErrorCode() != vtkErrorCode::NoError) {
    return std::unexpected(MeshIoError{
      MeshIoErrorCode::ReadFailed,
      path,
      std::string{"VTK failed to read mesh: "} + vtkErrorCode::GetStringFromErrorCode(reader->GetErrorCode())});
  }

  polyData = reader->GetOutput();
  return {};
}

std::expected<MeshGeometry, MeshIoError> fromPolyData(vtkPolyData* input, const std::filesystem::path& path)
{
  if (!input || input->GetNumberOfPoints() == 0 || input->GetNumberOfCells() == 0) {
    return std::unexpected(MeshIoError{MeshIoErrorCode::InvalidData, path, "Mesh contains no surface geometry"});
  }

  auto triangles = vtkSmartPointer<vtkTriangleFilter>::New();
  triangles->SetInputData(input);
  triangles->PassLinesOff();
  triangles->PassVertsOff();
  triangles->Update();

  vtkPolyData* polyData = triangles->GetOutput();
  const vtkIdType pointCount = polyData->GetNumberOfPoints();

  if (pointCount <= 0 || static_cast<unsigned long long>(pointCount) > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(
      MeshIoError{MeshIoErrorCode::InvalidData, path, "Mesh vertex count exceeds 32-bit indexing"});
  }

  MeshGeometry geometry;
  geometry.positions.reserve(static_cast<std::size_t>(pointCount));

  for (vtkIdType index = 0; index < pointCount; ++index) {
    double point[3]{};
    polyData->GetPoint(index, point);
    geometry.positions.emplace_back(
      static_cast<float>(point[0]),
      static_cast<float>(point[1]),
      static_cast<float>(point[2]));
  }

  auto ids = vtkSmartPointer<vtkIdList>::New();
  vtkCellArray* polygons = polyData->GetPolys();
  polygons->InitTraversal();

  while (polygons->GetNextCell(ids)) {
    if (ids->GetNumberOfIds() != 3) {
      return std::unexpected(
        MeshIoError{MeshIoErrorCode::InvalidData, path, "Mesh triangulation produced a non-triangle cell"});
    }

    for (vtkIdType corner = 0; corner < 3; ++corner) {
      const vtkIdType pointIndex = ids->GetId(corner);
      if (pointIndex < 0 || pointIndex >= pointCount) {
        return std::unexpected(
          MeshIoError{MeshIoErrorCode::InvalidData, path, "Mesh contains an invalid vertex index"});
      }
      geometry.triangleIndices.push_back(static_cast<uint32_t>(pointIndex));
    }
  }

  vtkDataArray* normals = polyData->GetPointData()->GetNormals();

  if (normals && normals->GetNumberOfComponents() >= 3 && normals->GetNumberOfTuples() == pointCount) {
    geometry.normals.reserve(static_cast<std::size_t>(pointCount));
    bool usableNormals = true;

    for (vtkIdType index = 0; index < pointCount; ++index) {
      double normal[3]{};
      normals->GetTuple(index, normal);
      const double length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);

      if (!std::isfinite(length) || length <= std::numeric_limits<double>::epsilon()) {
        usableNormals = false;
        break;
      }

      geometry.normals.emplace_back(
        static_cast<float>(normal[0] / length),
        static_cast<float>(normal[1] / length),
        static_cast<float>(normal[2] / length));
    }

    if (!usableNormals) regenerateNormals(geometry);
  }
  else {
    regenerateNormals(geometry);
  }

  if (auto valid = validateGeometry(geometry); !valid) {
    return std::unexpected(MeshIoError{MeshIoErrorCode::InvalidData, path, valid.error().message});
  }
  return geometry;
}

vtkSmartPointer<vtkPolyData> toPolyData(const MeshGeometry& geometry)
{
  auto points = vtkSmartPointer<vtkPoints>::New();
  points->SetDataTypeToFloat();
  points->SetNumberOfPoints(static_cast<vtkIdType>(geometry.positions.size()));

  for (std::size_t index = 0; index < geometry.positions.size(); ++index) {
    const glm::vec3& point = geometry.positions[index];
    points->SetPoint(static_cast<vtkIdType>(index), point.x, point.y, point.z);
  }

  auto triangles = vtkSmartPointer<vtkCellArray>::New();
  triangles->AllocateEstimate(static_cast<vtkIdType>(geometry.triangleCount()), 3);

  for (std::size_t index = 0; index < geometry.triangleIndices.size(); index += 3u) {
    const vtkIdType ids[3]{
      static_cast<vtkIdType>(geometry.triangleIndices[index]),
      static_cast<vtkIdType>(geometry.triangleIndices[index + 1u]),
      static_cast<vtkIdType>(geometry.triangleIndices[index + 2u])};
    triangles->InsertNextCell(3, ids);
  }

  auto polyData = vtkSmartPointer<vtkPolyData>::New();
  polyData->SetPoints(points);
  polyData->SetPolys(triangles);

  if (geometry.normals.size() == geometry.positions.size()) {
    auto normals = vtkSmartPointer<vtkFloatArray>::New();
    normals->SetName("Normals");
    normals->SetNumberOfComponents(3);
    normals->SetNumberOfTuples(static_cast<vtkIdType>(geometry.normals.size()));

    for (std::size_t index = 0; index < geometry.normals.size(); ++index) {
      const glm::vec3& normal = geometry.normals[index];
      normals->SetTuple3(static_cast<vtkIdType>(index), normal.x, normal.y, normal.z);
    }
    polyData->GetPointData()->SetNormals(normals);
  }
  return polyData;
}

template<class Writer>
std::expected<void, MeshIoError> writePolyData(const std::filesystem::path& path, vtkPolyData* polyData)
{
  auto writer = vtkSmartPointer<Writer>::New();
  writer->SetFileName(path.string().c_str());
  writer->SetInputData(polyData);
  const int success = writer->Write();

  if (writer->GetErrorCode() != vtkErrorCode::NoError) {
    return std::unexpected(MeshIoError{
      MeshIoErrorCode::WriteFailed,
      path,
      std::string{"VTK failed to write mesh: "} + vtkErrorCode::GetStringFromErrorCode(writer->GetErrorCode())});
  }

  if (success == 0) {
    return std::unexpected(MeshIoError{MeshIoErrorCode::WriteFailed, path, "VTK failed to write mesh"});
  }
  return {};
}
} // namespace

std::expected<DecodedMesh, MeshIoError> readVtkMesh(const std::filesystem::path& path, MeshFormat format)
{
  vtkSmartPointer<vtkPolyData> polyData;
  std::expected<void, MeshIoError> readResult;

  switch (format) {
    case MeshFormat::Vtp:
      readResult = readPolyData<vtkXMLPolyDataReader>(path, polyData);
      break;
    case MeshFormat::LegacyVtk:
      readResult = readPolyData<vtkPolyDataReader>(path, polyData);
      break;
    case MeshFormat::Stl:
      readResult = readPolyData<vtkSTLReader>(path, polyData);
      break;
    case MeshFormat::Ply:
      readResult = readPolyData<vtkPLYReader>(path, polyData);
      break;
    case MeshFormat::Obj:
      readResult = readPolyData<vtkOBJReader>(path, polyData);
      break;
    default:
      return std::unexpected(
        MeshIoError{MeshIoErrorCode::UnsupportedFormat, path, "Format is not handled by VTK mesh I/O"});
  }

  if (!readResult) {
    return std::unexpected(readResult.error());
  }

  auto geometry = fromPolyData(polyData, path);

  if (!geometry) {
    return std::unexpected(geometry.error());
  }

  DecodedMesh result;
  result.geometry = std::move(*geometry);
  result.coordinates.description = "Vertices are interpreted as associated-image physical coordinates";
  return result;
}

std::expected<void, MeshIoError>
writeVtkMesh(const std::filesystem::path& path, MeshFormat format, const MeshGeometry& geometry)
{
  if (auto valid = validateGeometry(geometry); !valid) {
    return std::unexpected(MeshIoError{MeshIoErrorCode::InvalidData, path, valid.error().message});
  }

  const auto polyData = toPolyData(geometry);
  switch (format) {
    case MeshFormat::Vtp:
      return writePolyData<vtkXMLPolyDataWriter>(path, polyData);
    case MeshFormat::LegacyVtk:
      return writePolyData<vtkPolyDataWriter>(path, polyData);
    case MeshFormat::Stl:
      return writePolyData<vtkSTLWriter>(path, polyData);
    case MeshFormat::Ply:
      return writePolyData<vtkPLYWriter>(path, polyData);
    case MeshFormat::Obj:
      return writePolyData<vtkOBJWriter>(path, polyData);
    default:
      return std::unexpected(
        MeshIoError{MeshIoErrorCode::UnsupportedFormat, path, "Format is not handled by VTK mesh I/O"});
  }
  return std::unexpected(MeshIoError{MeshIoErrorCode::UnsupportedFormat, path, "Unsupported VTK mesh format"});
}

} // namespace mesh::detail
