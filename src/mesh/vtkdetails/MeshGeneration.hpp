#ifndef MESH_GENERATION_H
#define MESH_GENERATION_H

#include "mesh/MeshTypes.h"

#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <map>
#include <set>
#include <utility>
#include <vector>


template< class T, uint32_t NumRows, uint32_t NumCols >
class vnl_matrix_fixed;


/// @todo do callback for progress

namespace vtkdetails
{

void progressFunction(
    vtkObject* caller,
    unsigned long /*eventId*/,
    void* /*clientData*/,
    void* /*callData*/ );


bool writePolyData( vtkPolyData* polyData, const char* fileName );

/**
  @note vtkImageData does not account for image directions

 * @brief generateIsoSurfaceMesh
 * @param imageData
 * @param imageDirections
 * @param isoValue
 * @return
 */
vtkSmartPointer< vtkPolyData > generateIsoSurfaceMesh(
    vtkImageData* imageData,
    const vnl_matrix_fixed< double, 3, 3 >& imageDirections,
    const double isoValue,
    const MeshPrimitiveType& primitiveType );

// Label images are stored as indices
vtkSmartPointer< vtkPolyData > generateLabelMesh(
    vtkImageData* imageData,
    const vnl_matrix_fixed< double, 3, 3 >& imageDirections,
    const uint32_t labelIndex,
    const MeshPrimitiveType& primitiveType );

std::map< int32_t, double > generateIntegerImageHistogram(
    vtkImageData* imageData,
    const std::set<int32_t>& imageValues );

} // namespace vtkdetails

#endif // MESH_GENERATION_H
