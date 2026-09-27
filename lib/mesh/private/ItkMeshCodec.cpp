#include "mesh/private/MeshCodec.h"

#include "mesh/MeshTransform.h"
#include "mesh/private/FreeSurferCoordinates.h"

#include <itkFreeSurferAsciiMeshIO.h>
#include <itkFreeSurferBinaryMeshIO.h>
#include <itkGiftiMeshIO.h>
#include <itkMesh.h>
#include <itkMeshFileReader.h>
#include <itkMeshFileWriter.h>
#include <itkOFFMeshIO.h>
#include <itkTriangleCell.h>

#include <limits>
#include <unordered_map>

namespace mesh::detail
{
namespace
{
using ItkMesh = itk::Mesh<float, 3>;

itk::MeshIOBase::Pointer meshIoForFormat(MeshFormat format)
{
  switch (format) {
    case MeshFormat::Gifti:
      return itk::GiftiMeshIO::New();
    case MeshFormat::Off:
      return itk::OFFMeshIO::New();
    case MeshFormat::FreeSurferBinary:
      return itk::FreeSurferBinaryMeshIO::New();
    case MeshFormat::FreeSurferAscii:
      return itk::FreeSurferAsciiMeshIO::New();
    default:
      return nullptr;
  }
}

std::expected<MeshGeometry, MeshIoError> fromItkMesh(const ItkMesh& mesh, const std::filesystem::path& path)
{
  if (mesh.GetNumberOfPoints() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(
      MeshIoError{MeshIoErrorCode::InvalidData, path, "Mesh vertex count exceeds 32-bit indexing"});
  }

  MeshGeometry geometry;
  geometry.positions.reserve(mesh.GetNumberOfPoints());
  std::unordered_map<ItkMesh::PointIdentifier, uint32_t> pointIndices;
  pointIndices.reserve(mesh.GetNumberOfPoints());
  const auto* points = mesh.GetPoints();

  if (!points) {
    return std::unexpected(MeshIoError{MeshIoErrorCode::InvalidData, path, "Mesh contains no vertices"});
  }

  for (auto iterator = points->Begin(); iterator != points->End(); ++iterator) {
    const auto& point = iterator.Value();
    const uint32_t index = static_cast<uint32_t>(geometry.positions.size());
    pointIndices.emplace(iterator.Index(), index);
    geometry.positions.emplace_back(point[0], point[1], point[2]);
  }

  const auto* cells = mesh.GetCells();
  if (!cells) {
    return std::unexpected(MeshIoError{MeshIoErrorCode::InvalidData, path, "Mesh contains no surface cells"});
  }

  for (auto iterator = cells->Begin(); iterator != cells->End(); ++iterator) {
    const auto* cell = iterator.Value();
    std::vector<uint32_t> cellIndices;
    cellIndices.reserve(cell->GetNumberOfPoints());

    for (const auto* point = cell->PointIdsBegin(); point != cell->PointIdsEnd(); ++point) {
      const auto found = pointIndices.find(*point);
      if (found == pointIndices.end()) {
        return std::unexpected(
          MeshIoError{MeshIoErrorCode::InvalidData, path, "Mesh cell references a missing vertex"});
      }
      cellIndices.push_back(found->second);
    }

    if (cellIndices.size() < 3u) {
      continue;
    }

    for (std::size_t corner = 1u; corner + 1u < cellIndices.size(); ++corner) {
      geometry.triangleIndices.insert(
        geometry.triangleIndices.end(),
        {cellIndices[0], cellIndices[corner], cellIndices[corner + 1u]});
    }
  }

  regenerateNormals(geometry);

  if (auto valid = validateGeometry(geometry); !valid) {
    return std::unexpected(MeshIoError{MeshIoErrorCode::InvalidData, path, valid.error().message});
  }

  return geometry;
}

ItkMesh::Pointer toItkMesh(const MeshGeometry& geometry)
{
  auto mesh = ItkMesh::New();
  for (std::size_t index = 0; index < geometry.positions.size(); ++index) {
    const glm::vec3& source = geometry.positions[index];
    ItkMesh::PointType point;
    point[0] = source.x;
    point[1] = source.y;
    point[2] = source.z;
    mesh->SetPoint(static_cast<ItkMesh::PointIdentifier>(index), point);
  }

  using Triangle = itk::TriangleCell<ItkMesh::CellType>;

  for (std::size_t index = 0; index < geometry.triangleIndices.size(); index += 3u) {
    ItkMesh::CellAutoPointer cell;
    cell.TakeOwnership(new Triangle);
    ItkMesh::CellType::PointIdentifier ids[3]{
      geometry.triangleIndices[index],
      geometry.triangleIndices[index + 1u],
      geometry.triangleIndices[index + 2u]};
    cell->SetPointIds(ids);
    mesh->SetCell(static_cast<ItkMesh::CellIdentifier>(index / 3u), cell);
  }

  return mesh;
}
} // namespace

std::expected<DecodedMesh, MeshIoError> readItkMesh(const std::filesystem::path& path, MeshFormat format)
{
  auto meshIo = meshIoForFormat(format);
  if (!meshIo) {
    return std::unexpected(
      MeshIoError{MeshIoErrorCode::UnsupportedFormat, path, "Format is not handled by ITK mesh I/O"});
  }

  try {
    auto reader = itk::MeshFileReader<ItkMesh>::New();
    reader->SetFileName(path.string());
    reader->SetMeshIO(meshIo);
    reader->Update();

    auto geometry = fromItkMesh(*reader->GetOutput(), path);
    if (!geometry) {
      return std::unexpected(geometry.error());
    }

    DecodedMesh result;
    result.geometry = std::move(*geometry);

    if (format == MeshFormat::Gifti) {
      result.coordinates.storedSpace = MeshCoordinateSpace::GiftiCoordinates;
      const auto* giftiIo = dynamic_cast<const itk::GiftiMeshIO*>(meshIo.GetPointer());
      glm::dmat4 direction{1.0};

      if (giftiIo) {
        for (std::size_t row = 0; row < 4u; ++row) {
          for (std::size_t column = 0; column < 4u; ++column) {
            direction[column][row] = giftiIo->GetDirection()[row][column];
          }
        }
        result.coordinates.embeddedSourceTransform = direction;
        result.coordinates.description = "GIFTI coordinate-system transform";
      }
    }
    else if (format == MeshFormat::FreeSurferBinary || format == MeshFormat::FreeSurferAscii) {
      result.coordinates.storedSpace = MeshCoordinateSpace::FreeSurferSurfaceRas;
      result.coordinates.anatomicalSystem = AnatomicalCoordinateSystem::RAS;
      result.coordinates.description = "FreeSurfer surface RAS (tkRegRAS)";

      auto volumeGeometry = readFreeSurferVolumeGeometry(path);
      if (!volumeGeometry) {
        return std::unexpected(volumeGeometry.error());
      }

      if (*volumeGeometry) {
        if ((*volumeGeometry)->verticesUseScannerRas) {
          result.coordinates.storedSpace = MeshCoordinateSpace::ScannerRas;
          result.coordinates.embeddedSourceTransform = glm::dmat4{1.0};
          result.coordinates.description = "FreeSurfer scanner RAS";
        }
        else {
          auto surfaceToScanner = freeSurferSurfaceRasToScannerRas(**volumeGeometry);
          if (!surfaceToScanner) {
            return std::unexpected(MeshIoError{MeshIoErrorCode::InvalidData, path, surfaceToScanner.error().message});
          }
          result.coordinates.embeddedSourceTransform = *surfaceToScanner;
        }
      }
    }
    return result;
  }
  catch (const itk::ExceptionObject& exception) {
    return std::unexpected(MeshIoError{MeshIoErrorCode::ReadFailed, path, exception.GetDescription()});
  }
}

std::expected<void, MeshIoError> writeItkMesh(
  const std::filesystem::path& path,
  MeshFormat format,
  const MeshGeometry& geometry,
  const MeshCoordinateMetadata& coordinates)
{
  auto meshIo = meshIoForFormat(format);
  if (!meshIo) {
    return std::unexpected(
      MeshIoError{MeshIoErrorCode::UnsupportedFormat, path, "Format is not handled by ITK mesh I/O"});
  }

  try {
    if (format == MeshFormat::Gifti) {
      auto* giftiIo = dynamic_cast<itk::GiftiMeshIO*>(meshIo.GetPointer());
      if (giftiIo) {
        itk::GiftiMeshIO::DirectionType direction;
        direction.SetIdentity();

        if (coordinates.embeddedSourceTransform) {
          for (std::size_t row = 0; row < 4u; ++row) {
            for (std::size_t column = 0; column < 4u; ++column) {
              direction[row][column] = (*coordinates.embeddedSourceTransform)[column][row];
            }
          }
        }
        giftiIo->SetDirection(direction);
      }
    }

    auto writer = itk::MeshFileWriter<ItkMesh>::New();
    writer->SetFileName(path.string());
    writer->SetMeshIO(meshIo);
    writer->SetInput(toItkMesh(geometry));
    writer->Update();
    return {};
  }
  catch (const itk::ExceptionObject& exception) {
    return std::unexpected(MeshIoError{MeshIoErrorCode::WriteFailed, path, exception.GetDescription()});
  }
}

} // namespace mesh::detail
