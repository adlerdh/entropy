#ifndef POLYDATA_GENERATOR_H
#define POLYDATA_GENERATOR_H

#include <glm/fwd.hpp>

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

namespace vtkutils
{

vtkSmartPointer<vtkPolyData> generateCone();
vtkSmartPointer<vtkPolyData> generateCube();
vtkSmartPointer<vtkPolyData> generateCylinder(const glm::dvec3& center, double radius, double height);
vtkSmartPointer<vtkPolyData> generateSphere();
vtkSmartPointer<vtkPolyData> generatePointyCylinders(double coneToCylinderLengthRatio);

} // namespace vtkutils

#endif // POLYDATA_GENERATOR_H
