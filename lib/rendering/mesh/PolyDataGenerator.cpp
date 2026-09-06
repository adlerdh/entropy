#include "rendering/mesh/PolyDataGenerator.h"

#include <glm/glm.hpp>

#include <vtkAppendPolyData.h>
#include <vtkCleanPolyData.h>
#include <vtkConeSource.h>
#include <vtkCubeSource.h>
#include <vtkCylinderSource.h>
#include <vtkNew.h>
#include <vtkPolyDataNormals.h>
#include <vtkSphereSource.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkTriangleFilter.h>

namespace vtkutils
{

vtkSmartPointer<vtkPolyData> generateCone()
{
  vtkNew<vtkConeSource> coneSource;

  vtkNew<vtkAppendPolyData> appendFilter;

  vtkNew<vtkTransformPolyDataFilter> transformFilter;
  vtkNew<vtkTransform> rotationTx;

  vtkNew<vtkTriangleFilter> triangleFilter;
  vtkNew<vtkCleanPolyData> cleanFilter;
  vtkNew<vtkPolyDataNormals> normalsGenerator;

  rotationTx->RotateX(90.0);

  appendFilter->AddInputConnection(coneSource->GetOutputPort());

  transformFilter->SetInputConnection(appendFilter->GetOutputPort());
  transformFilter->SetTransform(rotationTx.GetPointer());

  /// Ensure triangles and remove duplicate points
  triangleFilter->SetInputConnection(transformFilter->GetOutputPort());
  cleanFilter->SetInputConnection(triangleFilter->GetOutputPort());
  normalsGenerator->SetInputConnection(cleanFilter->GetOutputPort());

  static constexpr int sk_resolution = 60;

  coneSource->CappingOn();
  coneSource->SetHeight(1.0);
  coneSource->SetRadius(1.0);
  coneSource->SetResolution(sk_resolution);
  coneSource->SetDirection(0.0, 1.0, 0.0);
  coneSource->SetCenter(0.0, 0.5, 0.0);

  normalsGenerator->ComputePointNormalsOn();
  normalsGenerator->ComputeCellNormalsOff();
  normalsGenerator->SplittingOn();
  normalsGenerator->ConsistencyOff();
  normalsGenerator->Update();

  return normalsGenerator->GetOutput();
}

vtkSmartPointer<vtkPolyData> generateCube()
{
  vtkNew<vtkCubeSource> cubeSource;
  vtkNew<vtkTriangleFilter> triangleFilter;
  vtkNew<vtkCleanPolyData> cleanFilter;
  vtkNew<vtkPolyDataNormals> normalsGenerator;

  triangleFilter->SetInputConnection(cubeSource->GetOutputPort());
  cleanFilter->SetInputConnection(triangleFilter->GetOutputPort());
  normalsGenerator->SetInputConnection(cleanFilter->GetOutputPort());

  normalsGenerator->ComputePointNormalsOn();
  normalsGenerator->ComputeCellNormalsOff();
  normalsGenerator->Update();

  return normalsGenerator->GetOutput();
}

vtkSmartPointer<vtkPolyData> generateCylinder(const glm::dvec3& center, double radius, double height)
{
  vtkNew<vtkCylinderSource> cylinderSource;

  vtkNew<vtkAppendPolyData> appendFilter;

  vtkNew<vtkTransformPolyDataFilter> transformFilter;
  vtkNew<vtkTransform> rotationTx;

  vtkNew<vtkTriangleFilter> triangleFilter;
  vtkNew<vtkCleanPolyData> cleanFilter;
  vtkNew<vtkPolyDataNormals> normalsGenerator;

  rotationTx->RotateX(90.0);

  appendFilter->AddInputConnection(cylinderSource->GetOutputPort());

  transformFilter->SetInputConnection(appendFilter->GetOutputPort());
  transformFilter->SetTransform(rotationTx.GetPointer());

  /// Ensure triangles and remove duplicate points
  triangleFilter->SetInputConnection(transformFilter->GetOutputPort());
  cleanFilter->SetInputConnection(triangleFilter->GetOutputPort());
  normalsGenerator->SetInputConnection(cleanFilter->GetOutputPort());

  static constexpr int sk_resolution = 60;

  cylinderSource->CappingOn();
  cylinderSource->SetCenter(center.x, center.y, center.z);
  cylinderSource->SetHeight(height);
  cylinderSource->SetRadius(radius);
  cylinderSource->SetResolution(sk_resolution);

  normalsGenerator->ComputePointNormalsOn();
  normalsGenerator->ComputeCellNormalsOff();
  normalsGenerator->SplittingOn();
  normalsGenerator->ConsistencyOff();
  normalsGenerator->Update();

  return normalsGenerator->GetOutput();
}

vtkSmartPointer<vtkPolyData> generateSphere()
{
  static constexpr int sk_resolution = 60;

  vtkNew<vtkSphereSource> sphereSource;
  vtkNew<vtkTriangleFilter> triangleFilter;
  vtkNew<vtkCleanPolyData> cleanFilter;
  vtkNew<vtkPolyDataNormals> normalsGenerator;

  triangleFilter->SetInputConnection(sphereSource->GetOutputPort());
  cleanFilter->SetInputConnection(triangleFilter->GetOutputPort());
  normalsGenerator->SetInputConnection(cleanFilter->GetOutputPort());

  sphereSource->SetCenter(0.0, 0.0, 0.0);
  sphereSource->SetRadius(1.0);
  sphereSource->SetPhiResolution(sk_resolution);
  sphereSource->SetThetaResolution(2 * sk_resolution);

  //    normalGenerator->SetSplitting(0);
  normalsGenerator->ComputePointNormalsOn();
  normalsGenerator->ComputeCellNormalsOff();
  normalsGenerator->Update();

  return normalsGenerator->GetOutput();
}

vtkSmartPointer<vtkPolyData> generatePointyCylinders(double coneToCylinderLengthRatio)
{
  vtkNew<vtkConeSource> coneSource1;
  vtkNew<vtkConeSource> coneSource2;
  vtkNew<vtkCylinderSource> cylinderSource1;
  vtkNew<vtkCylinderSource> cylinderSource2;

  vtkNew<vtkAppendPolyData> appendFilter;

  vtkNew<vtkTransformPolyDataFilter> transformFilter;
  vtkNew<vtkTransform> rotationTx;

  vtkNew<vtkTriangleFilter> triangleFilter;
  vtkNew<vtkCleanPolyData> cleanFilter;
  vtkNew<vtkPolyDataNormals> normalsGenerator;

  rotationTx->RotateX(90.0);

  appendFilter->AddInputConnection(coneSource1->GetOutputPort());
  appendFilter->AddInputConnection(coneSource2->GetOutputPort());
  appendFilter->AddInputConnection(cylinderSource1->GetOutputPort());
  appendFilter->AddInputConnection(cylinderSource2->GetOutputPort());

  transformFilter->SetInputConnection(appendFilter->GetOutputPort());
  transformFilter->SetTransform(rotationTx.GetPointer());

  /// Ensure triangles and remove duplicate points
  triangleFilter->SetInputConnection(transformFilter->GetOutputPort());
  cleanFilter->SetInputConnection(triangleFilter->GetOutputPort());
  normalsGenerator->SetInputConnection(cleanFilter->GetOutputPort());

  static constexpr int sk_resolution = 60;

  coneSource1->CappingOn();
  coneSource1->SetHeight(coneToCylinderLengthRatio);
  coneSource1->SetRadius(1.0);
  coneSource1->SetResolution(sk_resolution);
  coneSource1->SetDirection(0.0, -1.0, 0.0);
  coneSource1->SetCenter(0.0, coneToCylinderLengthRatio / 2.0, 0.0);

  coneSource2->CappingOn();
  coneSource2->SetHeight(coneToCylinderLengthRatio);
  coneSource2->SetRadius(1.0);
  coneSource2->SetResolution(sk_resolution);
  coneSource2->SetDirection(0.0, 1.0, 0.0);
  coneSource2->SetCenter(0.0, -coneToCylinderLengthRatio / 2.0, 0.0);

  cylinderSource1->CappingOn();
  cylinderSource1->SetCenter(0.0, 0.5 * (coneToCylinderLengthRatio + 1.0), 0.0);
  cylinderSource1->SetHeight(1.0 - coneToCylinderLengthRatio);
  cylinderSource1->SetRadius(1.0);
  cylinderSource1->SetResolution(sk_resolution);

  cylinderSource2->CappingOn();
  cylinderSource2->SetCenter(0.0, -0.5 * (coneToCylinderLengthRatio + 1.0), 0.0);
  cylinderSource2->SetHeight(1.0 - coneToCylinderLengthRatio);
  cylinderSource2->SetRadius(1.0);
  cylinderSource2->SetResolution(sk_resolution);

  normalsGenerator->ComputePointNormalsOn();
  normalsGenerator->ComputeCellNormalsOff();
  normalsGenerator->SplittingOn();
  normalsGenerator->ConsistencyOff();
  normalsGenerator->Update();

  return normalsGenerator->GetOutput();
}

} // namespace vtkutils
