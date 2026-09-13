#include "rendering/mesh/MeshPlaneIntersection.h"

#include "rendering/mesh/MeshData.h"

#include <glm/geometric.hpp>

#include <vtkCellArray.h>
#include <vtkIdList.h>
#include <vtkNew.h>
#include <vtkPlane.h>
#include <vtkPlaneCutter.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace rendering::mesh
{
namespace
{

bool isFinite(const glm::vec3& value)
{
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

std::expected<vtkSmartPointer<vtkPolyData>, std::string> makePolyData(const MeshData& mesh)
{
  if (mesh.positions.empty()) {
    return std::unexpected("The mesh has no vertices");
  }
  if (mesh.indices.empty() || mesh.indices.size() % 3u != 0u) {
    return std::unexpected("The mesh index buffer does not contain complete triangles");
  }
  if (mesh.positions.size() > static_cast<std::size_t>(std::numeric_limits<vtkIdType>::max())) {
    return std::unexpected("The mesh has too many vertices for VTK");
  }

  vtkNew<vtkPoints> points;
  points->SetDataTypeToFloat();
  points->SetNumberOfPoints(static_cast<vtkIdType>(mesh.positions.size()));
  for (std::size_t index = 0; index < mesh.positions.size(); ++index) {
    const glm::vec3& position = mesh.positions[index];
    if (!isFinite(position)) {
      return std::unexpected("The mesh contains a non-finite vertex");
    }
    points->SetPoint(static_cast<vtkIdType>(index), position.x, position.y, position.z);
  }

  vtkNew<vtkCellArray> triangles;
  for (std::size_t index = 0; index < mesh.indices.size(); index += 3u) {
    const uint32_t i0 = mesh.indices[index];
    const uint32_t i1 = mesh.indices[index + 1u];
    const uint32_t i2 = mesh.indices[index + 2u];
    if (i0 >= mesh.positions.size() || i1 >= mesh.positions.size() || i2 >= mesh.positions.size()) {
      return std::unexpected("The mesh contains a triangle index outside its vertex array");
    }
    const vtkIdType ids[3]{static_cast<vtkIdType>(i0), static_cast<vtkIdType>(i1), static_cast<vtkIdType>(i2)};
    triangles->InsertNextCell(3, ids);
  }

  vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
  polyData->SetPoints(points);
  polyData->SetPolys(triangles);
  return polyData;
}

} // namespace

struct MeshPlaneIntersector::Impl
{
  vtkSmartPointer<vtkPolyData> mesh;
  vtkSmartPointer<vtkPlane> plane;
  vtkSmartPointer<vtkPlaneCutter> cutter;
};

MeshPlaneIntersector::MeshPlaneIntersector(std::unique_ptr<Impl> implementation) : m_impl{std::move(implementation)} {}

MeshPlaneIntersector::MeshPlaneIntersector(MeshPlaneIntersector&&) noexcept = default;
MeshPlaneIntersector& MeshPlaneIntersector::operator=(MeshPlaneIntersector&&) noexcept = default;
MeshPlaneIntersector::~MeshPlaneIntersector() = default;

std::expected<MeshPlaneIntersector, std::string> MeshPlaneIntersector::create(const MeshData& mesh)
{
  auto polyData = makePolyData(mesh);
  if (!polyData) {
    return std::unexpected(polyData.error());
  }

  auto implementation = std::make_unique<Impl>();
  implementation->mesh = std::move(*polyData);
  implementation->plane = vtkSmartPointer<vtkPlane>::New();
  implementation->cutter = vtkSmartPointer<vtkPlaneCutter>::New();
  implementation->cutter->SetInputData(implementation->mesh);
  implementation->cutter->SetPlane(implementation->plane);
  implementation->cutter->BuildTreeOn();
  implementation->cutter->ComputeNormalsOff();
  implementation->cutter->InterpolateAttributesOff();
  return MeshPlaneIntersector{std::move(implementation)};
}

std::expected<std::vector<MeshPlaneIntersectionSegment>, std::string> MeshPlaneIntersector::intersect(
  const glm::vec3& planeOrigin,
  const glm::vec3& planeNormal)
{
  if (!isFinite(planeOrigin) || !isFinite(planeNormal)) {
    return std::unexpected("The intersection plane contains a non-finite value");
  }
  const float normalLengthSquared = glm::dot(planeNormal, planeNormal);
  if (normalLengthSquared <= std::numeric_limits<float>::epsilon()) {
    return std::unexpected("The intersection plane normal is zero");
  }

  const glm::vec3 unitNormal = planeNormal / std::sqrt(normalLengthSquared);
  m_impl->plane->SetOrigin(planeOrigin.x, planeOrigin.y, planeOrigin.z);
  m_impl->plane->SetNormal(unitNormal.x, unitNormal.y, unitNormal.z);
  m_impl->plane->Modified();
  m_impl->cutter->Update();

  vtkPolyData* output = vtkPolyData::SafeDownCast(m_impl->cutter->GetOutputDataObject(0));
  if (!output || !output->GetPoints() || !output->GetLines()) {
    return std::vector<MeshPlaneIntersectionSegment>{};
  }

  std::vector<MeshPlaneIntersectionSegment> segments;
  segments.reserve(static_cast<std::size_t>(output->GetNumberOfLines()));
  vtkCellArray* lines = output->GetLines();
  vtkNew<vtkIdList> pointIds;
  lines->InitTraversal();
  while (lines->GetNextCell(pointIds)) {
    for (vtkIdType index = 1; index < pointIds->GetNumberOfIds(); ++index) {
      double first[3]{};
      double second[3]{};
      output->GetPoint(pointIds->GetId(index - 1), first);
      output->GetPoint(pointIds->GetId(index), second);
      segments.push_back(
        {.first = glm::vec3{first[0], first[1], first[2]}, .second = glm::vec3{second[0], second[1], second[2]}});
    }
  }
  return segments;
}

} // namespace rendering::mesh
