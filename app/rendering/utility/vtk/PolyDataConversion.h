#pragma once

#include "rendering/utility/vtk/VectorArrayBuffer.h"

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <memory>

namespace vtkconvert
{

/**
 * @brief Extract VTK point coordinates as tightly packed float xyz triples.
 */
std::unique_ptr<VectorArrayBuffer<float> > extractPointsToFloatArrayBuffer(const vtkSmartPointer<vtkPolyData> polyData);

/**
 * @brief Extract VTK point normals as tightly packed unsigned integer triples.
 */
std::unique_ptr<VectorArrayBuffer<uint32_t> > extractNormalsToUIntArrayBuffer(
  const vtkSmartPointer<vtkPolyData> polyData);

/**
 * @brief Extract VTK texture coordinates as tightly packed unsigned integer tuples.
 */
std::unique_ptr<VectorArrayBuffer<uint32_t> > extractTexCoordsToUIntArrayBuffer(
  const vtkSmartPointer<vtkPolyData> polyData);

/**
 * @brief Extract VTK texture coordinates as tightly packed float tuples.
 */
std::unique_ptr<VectorArrayBuffer<float> > extractTexCoordsToFloatArrayBuffer(
  const vtkSmartPointer<vtkPolyData> polyData);

/**
 * @brief Extract polygon connectivity as unsigned integer indices.
 */
std::unique_ptr<VectorArrayBuffer<uint32_t> > extractIndicesToUIntArrayBuffer(
  const vtkSmartPointer<vtkPolyData> polyData);

} // namespace vtkconvert
