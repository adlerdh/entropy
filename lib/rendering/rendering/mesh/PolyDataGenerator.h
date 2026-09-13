#pragma once

#include <glm/fwd.hpp>

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

namespace vtkutils
{

/// Generate a canonical cone mesh.
vtkSmartPointer<vtkPolyData> generateCone();

/// Generate a canonical cube mesh.
vtkSmartPointer<vtkPolyData> generateCube();

/// Generate a cylinder mesh centered at `center` with the requested radius and height.
vtkSmartPointer<vtkPolyData> generateCylinder(const glm::dvec3& center, double radius, double height);

/// Generate a canonical sphere mesh.
vtkSmartPointer<vtkPolyData> generateSphere();

/// Generate the arrow-like crosshair glyph mesh from cylinders and cones.
vtkSmartPointer<vtkPolyData> generatePointyCylinders(double coneToCylinderLengthRatio);

} // namespace vtkutils
