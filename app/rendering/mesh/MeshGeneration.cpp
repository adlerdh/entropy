#include "rendering/mesh/MeshGeneration.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <vtkCellArray.h>
#include <vtkCleanPolyData.h>
#include <vtkDataArray.h>
#include <vtkDiscreteFlyingEdges3D.h>
#include <vtkFlyingEdges3D.h>
#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkMultiThreader.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkReverseSense.h>
#include <vtkSMPTools.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkTriangleFilter.h>
#include <vtkTrivialProducer.h>
#include <vtkWindowedSincPolyDataFilter.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <thread>

namespace rendering::mesh
{
namespace
{

constexpr int k_minMeshGenerationThreads = 1;
std::mutex& vtkMeshGenerationMutex()
{
  static std::mutex mutex;
  return mutex;
}

int boundedThreadCount(const std::size_t requestedThreadCount)
{
  if (requestedThreadCount > 0) {
    return std::max(
      k_minMeshGenerationThreads,
      static_cast<int>(std::min<std::size_t>(requestedThreadCount, std::numeric_limits<int>::max())));
  }

  const unsigned int hardwareThreads = std::thread::hardware_concurrency();
  if (hardwareThreads <= 2) {
    return k_minMeshGenerationThreads;
  }

  const int halfHardwareThreads = std::max(k_minMeshGenerationThreads, static_cast<int>(hardwareThreads / 2));
  return std::clamp(static_cast<int>(hardwareThreads - 2), k_minMeshGenerationThreads, halfHardwareThreads);
}

void configureVtkThreading(const MeshGenerationOptions& options)
{
  const int threadCount = boundedThreadCount(options.threadCount);
  vtkMultiThreader::SetGlobalMaximumNumberOfThreads(threadCount);
  vtkMultiThreader::SetGlobalDefaultNumberOfThreads(threadCount);
  vtkSMPTools::Initialize(threadCount);
}

vtkSmartPointer<vtkImageData> makeVtkImageData(const ScalarGrid3D& grid)
{
  if (!isValidScalarGrid(grid)) {
    return nullptr;
  }

  vtkSmartPointer<vtkImageData> imageData = vtkSmartPointer<vtkImageData>::New();
  imageData->SetDimensions(
    static_cast<int>(grid.dimensions.x),
    static_cast<int>(grid.dimensions.y),
    static_cast<int>(grid.dimensions.z));
  imageData->SetOrigin(0.0, 0.0, 0.0);
  imageData->SetSpacing(1.0, 1.0, 1.0);
  imageData->AllocateScalars(VTK_FLOAT, 1);

  auto* destination = static_cast<float*>(imageData->GetScalarPointer());
  if (!destination) {
    return nullptr;
  }

  std::ranges::copy(grid.values, destination);
  return imageData;
}

vtkSmartPointer<vtkTransform> makeGridTransform(const glm::mat4& grid_T_voxelIndex)
{
  vtkNew<vtkMatrix4x4> matrix;
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      matrix->SetElement(row, column, static_cast<double>(grid_T_voxelIndex[column][row]));
    }
  }

  vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
  transform->SetMatrix(matrix);
  return transform;
}

bool flipsOrientation(const glm::mat4& transform)
{
  return glm::determinant(glm::mat3{transform}) < 0.0f;
}

vtkAlgorithmOutput* transformToGridOutput(
  vtkAlgorithmOutput* input,
  const ScalarGrid3D& grid,
  vtkTransformPolyDataFilter& transformFilter,
  vtkReverseSense& reverseSense)
{
  if (!input) {
    return nullptr;
  }

  transformFilter.SetInputConnection(input);
  transformFilter.SetTransform(makeGridTransform(grid.grid_T_voxelIndex));
  vtkAlgorithmOutput* output = transformFilter.GetOutputPort();

  if (flipsOrientation(grid.grid_T_voxelIndex)) {
    reverseSense.SetInputConnection(output);
    reverseSense.ReverseCellsOn();
    reverseSense.ReverseNormalsOn();
    output = reverseSense.GetOutputPort();
  }

  return output;
}

vtkSmartPointer<vtkPolyData>
finalizeSurface(vtkAlgorithmOutput* sourceOutput, const ScalarGrid3D& grid, const bool smoothMesh)
{
  if (!sourceOutput) {
    return nullptr;
  }

  vtkNew<vtkTriangleFilter> triangleFilter;
  vtkNew<vtkCleanPolyData> cleanFilter;
  vtkNew<vtkWindowedSincPolyDataFilter> windowedSincSmoother;
  vtkNew<vtkTransformPolyDataFilter> transformFilter;
  vtkNew<vtkReverseSense> reverseSense;
  vtkNew<vtkPolyDataNormals> normalsGenerator;

  vtkAlgorithmOutput* pipelineTail = sourceOutput;

  // Flying Edges already emits triangles, but this keeps the downstream contract explicit and robust to future filters.
  triangleFilter->SetInputConnection(pipelineTail);
  pipelineTail = triangleFilter->GetOutputPort();

  cleanFilter->SetInputConnection(pipelineTail);
  pipelineTail = cleanFilter->GetOutputPort();

  if (smoothMesh) {
    windowedSincSmoother->SetInputConnection(pipelineTail);
    windowedSincSmoother->SetNumberOfIterations(25);
    windowedSincSmoother->SetFeatureEdgeSmoothing(1);
    windowedSincSmoother->SetFeatureAngle(120.0);
    windowedSincSmoother->SetPassBand(0.1);
    windowedSincSmoother->BoundarySmoothingOff();
    windowedSincSmoother->NonManifoldSmoothingOn();
    windowedSincSmoother->NormalizeCoordinatesOn();
    pipelineTail = windowedSincSmoother->GetOutputPort();
  }

  pipelineTail = transformToGridOutput(pipelineTail, grid, *transformFilter, *reverseSense);
  if (!pipelineTail) {
    return nullptr;
  }

  normalsGenerator->SetInputConnection(pipelineTail);
  normalsGenerator->ComputePointNormalsOn();
  normalsGenerator->ComputeCellNormalsOff();
  normalsGenerator->SetFeatureAngle(120.0);
  normalsGenerator->FlipNormalsOff();
  normalsGenerator->SplittingOn();
  normalsGenerator->ConsistencyOff();
  normalsGenerator->AutoOrientNormalsOn();
  normalsGenerator->Update();

  vtkSmartPointer<vtkPolyData> output = vtkSmartPointer<vtkPolyData>::New();
  output->DeepCopy(normalsGenerator->GetOutput());
  return output;
}

std::optional<MeshData> meshDataFromPolyData(vtkPolyData* polyData, const MeshCoordinateSpace coordinateSpace)
{
  if (!polyData || !polyData->GetPoints() || !polyData->GetPolys()) {
    return std::nullopt;
  }

  MeshData mesh;
  mesh.coordinateSpace = coordinateSpace;

  const vtkIdType numPoints = polyData->GetNumberOfPoints();
  if (numPoints <= 0 || numPoints > static_cast<vtkIdType>(std::numeric_limits<uint32_t>::max())) {
    return std::nullopt;
  }

  mesh.positions.reserve(static_cast<std::size_t>(numPoints));
  mesh.normals.reserve(static_cast<std::size_t>(numPoints));

  vtkDataArray* normals = polyData->GetPointData() ? polyData->GetPointData()->GetNormals() : nullptr;
  for (vtkIdType pointIndex = 0; pointIndex < numPoints; ++pointIndex) {
    double point[3] = {};
    polyData->GetPoint(pointIndex, point);
    mesh.positions.emplace_back(
      static_cast<float>(point[0]),
      static_cast<float>(point[1]),
      static_cast<float>(point[2]));

    if (normals && normals->GetNumberOfTuples() > pointIndex) {
      double normal[3] = {};
      normals->GetTuple(pointIndex, normal);
      glm::vec3 n{static_cast<float>(normal[0]), static_cast<float>(normal[1]), static_cast<float>(normal[2])};
      const float length = glm::length(n);
      mesh.normals.push_back(length > 0.0f ? n / length : glm::vec3{0.0f, 0.0f, 1.0f});
    }
    else {
      mesh.normals.emplace_back(0.0f, 0.0f, 1.0f);
    }
  }

  vtkCellArray* polygons = polyData->GetPolys();
  polygons->InitTraversal();

  vtkIdType numCellPoints = 0;
  const vtkIdType* cellPoints = nullptr;
  while (polygons->GetNextCell(numCellPoints, cellPoints)) {
    if (numCellPoints != 3) {
      continue;
    }

    if (
      cellPoints[0] < 0 || cellPoints[1] < 0 || cellPoints[2] < 0 || cellPoints[0] >= numPoints ||
      cellPoints[1] >= numPoints || cellPoints[2] >= numPoints)
    {
      return std::nullopt;
    }

    mesh.indices.push_back(static_cast<uint32_t>(cellPoints[0]));
    mesh.indices.push_back(static_cast<uint32_t>(cellPoints[1]));
    mesh.indices.push_back(static_cast<uint32_t>(cellPoints[2]));
  }

  if (mesh.indices.empty()) {
    return std::nullopt;
  }

  return mesh;
}

} // namespace

std::optional<MeshData>
generateIsoSurfaceMesh(const ScalarGrid3D& grid, const double isoValue, const MeshGenerationOptions& options)
{
  std::scoped_lock lock(vtkMeshGenerationMutex());
  configureVtkThreading(options);

  vtkSmartPointer<vtkImageData> imageData = makeVtkImageData(grid);
  if (!imageData) {
    return std::nullopt;
  }

  vtkNew<vtkFlyingEdges3D> flyingEdges;
  flyingEdges->SetInputData(imageData);
  flyingEdges->ComputeNormalsOff();
  flyingEdges->ComputeScalarsOff();
  flyingEdges->ComputeGradientsOff();
  flyingEdges->SetNumberOfContours(1);
  flyingEdges->SetValue(0, isoValue);
  flyingEdges->Update();
  if (!flyingEdges->GetOutput() || flyingEdges->GetOutput()->GetNumberOfPolys() <= 0) {
    return std::nullopt;
  }

  vtkNew<vtkTrivialProducer> source;
  source->SetOutput(flyingEdges->GetOutput());

  vtkSmartPointer<vtkPolyData> polyData = finalizeSurface(source->GetOutputPort(), grid, false);
  return meshDataFromPolyData(polyData, grid.coordinateSpace);
}

std::optional<MeshData>
generateLabelMesh(const ScalarGrid3D& grid, const int64_t labelValue, const MeshGenerationOptions& options)
{
  if (
    labelValue < static_cast<int64_t>(std::numeric_limits<int>::min()) ||
    labelValue > static_cast<int64_t>(std::numeric_limits<int>::max()))
  {
    return std::nullopt;
  }

  std::scoped_lock lock(vtkMeshGenerationMutex());
  configureVtkThreading(options);

  vtkSmartPointer<vtkImageData> imageData = makeVtkImageData(grid);
  if (!imageData) {
    return std::nullopt;
  }

  vtkNew<vtkDiscreteFlyingEdges3D> flyingEdges;
  flyingEdges->SetInputData(imageData);
  flyingEdges->ComputeNormalsOff();
  flyingEdges->ComputeScalarsOff();
  flyingEdges->ComputeGradientsOff();
  flyingEdges->SetNumberOfContours(1);
  flyingEdges->SetValue(0, static_cast<double>(labelValue));
  flyingEdges->Update();
  if (!flyingEdges->GetOutput() || flyingEdges->GetOutput()->GetNumberOfPolys() <= 0) {
    return std::nullopt;
  }

  vtkNew<vtkTrivialProducer> source;
  source->SetOutput(flyingEdges->GetOutput());

  vtkSmartPointer<vtkPolyData> polyData = finalizeSurface(source->GetOutputPort(), grid, true);
  return meshDataFromPolyData(polyData, grid.coordinateSpace);
}

} // namespace rendering::mesh
