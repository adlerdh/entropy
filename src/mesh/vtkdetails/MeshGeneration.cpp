#include "mesh/vtkdetails/MeshGeneration.hpp"
#include "mesh/vtkdetails/ErrorObserver.hpp"

#include "common/MathFuncs.h"

#include "image/ImageIoInfo.h"
#include "image/ImageUtility.h"

#include <vtkCallbackCommand.h>
#include <vtkCleanPolyData.h>
#include <vtkDecimatePro.h>
#include <vtkGeometryFilter.h>
#include <vtkImageAccumulate.h>
#include <vtkImageCast.h>
#include <vtkImageGaussianSmooth.h>
#include <vtkImageThreshold.h>
#include <vtkMarchingCubes.h>
#include <vtkMaskFields.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyDataNormals.h>
#include <vtkPolyDataWriter.h>
#include <vtkReverseSense.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkStripper.h>
#include <vtkThreshold.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkTriangleFilter.h>
#include <vtkWeakPointer.h>
#include <vtkWindowedSincPolyDataFilter.h>

#include <vnl/vnl_matrix.h>
#include <vnl/vnl_matrix_fixed.h>
#include <vnl/vnl_vector_fixed.h>

#include <algorithm>

namespace
{

/// @todo We need to verify that this is same as image's subject_O_pixels matrix!!!
vnl_matrix_fixed<double, 4, 4> constructVoxelToSubjectMatrix(
  const vnl_matrix_fixed<double, 3, 3>& directions,
  const vnl_vector_fixed<double, 3>& origin,
  const vnl_vector_fixed<double, 3>& spacing
)
{
  const vnl_matrix<double> d(directions.data_block(), 3, 3);

  const vnl_matrix<double> rotateScaleMatrix = d * vnl_diag_matrix<double>(spacing.as_vector());

  vnl_vector_fixed<double, 4> translationColumn(1.0);
  translationColumn.update(origin.as_vector());

  vnl_matrix_fixed<double, 4, 4> subject_O_voxels;
  subject_O_voxels.set_identity();
  subject_O_voxels.update(rotateScaleMatrix);
  subject_O_voxels.set_column(3, translationColumn);

  return subject_O_voxels;
}

vnl_matrix_fixed<double, 4, 4> constructVTKImageToVoxelsMatrix(
  const vnl_vector<double>& origin, const vnl_vector<double>& spacing
)
{
  vnl_matrix_fixed<double, 4, 4> voxels_O_VTK;
  voxels_O_VTK.set_identity();

  for (uint32_t i = 0; i < 3; ++i)
  {
    voxels_O_VTK(i, i) = 1.0 / spacing[i];
    voxels_O_VTK(i, 3) = -origin[i] / spacing[i];
  }

  return voxels_O_VTK;
}

vnl_matrix_fixed<double, 4, 4> constructVTKImageToSubjectMatrix(
  const vnl_matrix_fixed<double, 3, 3>& directions,
  const vnl_vector<double>& origin,
  const vnl_vector<double>& spacing
)
{
  const vnl_matrix_fixed<double, 4, 4> voxels_O_VTK
    = constructVTKImageToVoxelsMatrix(origin, spacing);

  const vnl_matrix_fixed<double, 4, 4> subject_O_voxels
    = constructVoxelToSubjectMatrix(directions, origin, spacing);

  return subject_O_voxels * voxels_O_VTK;
}

} // namespace

namespace vtkdetails
{

/// @todo Place in separate function
/// @todo Combine multiple progress into one
template<class FilterType>
void progressFunction(vtkObject* caller, unsigned long /*eventId*/, void* /*clientData*/, void* /*callData*/)

{
  constexpr double kInc = 0.01;
  static double progress = 0.0;

  FilterType* filter = static_cast<FilterType*>(caller);

  if (filter->GetProgress() > progress + kInc)
  {
    progress += kInc;
  }
}

bool writePolyData(vtkPolyData* polyData, const char* fileName)
{
  if (!polyData)
  {
    return false;
  }

  /// @todo check file extension (case insensitive) and choose correct writer:
  /// byu, stl, vtk
  vtkNew<vtkPolyDataWriter> polyDataWriter;

  polyDataWriter->SetInputData(polyData);
  polyDataWriter->SetFileName(fileName);
  polyDataWriter->Write();

  return true;
}

vtkSmartPointer<vtkPolyData> generateIsoSurfaceMesh(
  vtkImageData* imageData,
  const vnl_matrix_fixed<double, 3, 3>& imageDirections,
  const double isoValue,
  const MeshPrimitiveType& primitiveType
)
{
  if (!imageData)
  {
    return nullptr;
  }

  if (MeshPrimitiveType::TriangleFan == primitiveType)
  {
    // Not supported
    return nullptr;
  }

  vtkNew<vtkMarchingCubes> marchingCubes;
  vtkNew<vtkTriangleFilter> triangleFilter;
  vtkNew<vtkCleanPolyData> cleanFilter;
  vtkNew<vtkStripper> triangleStripper;
  vtkNew<vtkTransformPolyDataFilter> transformToSubjectFilter;
  vtkNew<vtkReverseSense> reverseNormalsSense;

  // Transformation from VTK coordinates to subject (ITK/LPS) space
  const vnl_matrix_fixed<double, 4, 4> subject_O_VTK = constructVTKImageToSubjectMatrix(
    imageDirections,
    vnl_vector<double>{imageData->GetOrigin(), 3},
    vnl_vector<double>{imageData->GetSpacing(), 3}
  );

  vtkNew<vtkTransform> tx_subject_O_VTK;
  tx_subject_O_VTK->SetMatrix(subject_O_VTK.data_block());

  // Set up mesh generation and processing pipeline
  vtkWeakPointer<vtkPolyDataAlgorithm> pipelineTail = nullptr;

  // Generate isosurfaces and point normal vectors
  marchingCubes->SetInputData(imageData);
  marchingCubes->ComputeNormalsOn();
  marchingCubes->SetComputeScalars(true);
  marchingCubes->ComputeGradientsOff();
  marchingCubes->SetNumberOfContours(1);
  marchingCubes->SetValue(0, isoValue);
  pipelineTail = marchingCubes.GetPointer();

  // Convert the mesh to triangles
  triangleFilter->SetInputConnection(pipelineTail->GetOutputPort());
  pipelineTail = triangleFilter.GetPointer();

  if (MeshPrimitiveType::TriangleStrip == primitiveType)
  {
    // Generate triangle strips
    if (!pipelineTail)
    {
      return nullptr;
    }
    triangleStripper->SetInputConnection(pipelineTail->GetOutputPort());
    pipelineTail = triangleStripper.GetPointer();
  }

  // Clean the mesh
  cleanFilter->SetInputConnection(pipelineTail->GetOutputPort());
  pipelineTail = cleanFilter.GetPointer();

  // Transform to subject space
  transformToSubjectFilter->SetInputConnection(pipelineTail->GetOutputPort());
  transformToSubjectFilter->SetTransform(tx_subject_O_VTK.GetPointer());
  pipelineTail = transformToSubjectFilter.GetPointer();

  // Reverse the normals if the transformation Jacobian is negative
  if (tx_subject_O_VTK->GetMatrix()->Determinant() < 0.0)
  {
    if (!pipelineTail)
    {
      return nullptr;
    }

    reverseNormalsSense->SetInputConnection(pipelineTail->GetOutputPort());
    reverseNormalsSense->ReverseNormalsOn();
    reverseNormalsSense->ReverseCellsOff();
    pipelineTail = reverseNormalsSense.GetPointer();
  }

  if (!pipelineTail)
  {
    return nullptr;
  }

  pipelineTail->Update();

  vtkSmartPointer<vtkPolyData> surfacePolyData = pipelineTail->GetOutput();
  //    writePolyData( surfacePolyData.Get(), "isosurface.vtk" );

  return surfacePolyData;
}

std::map<int32_t, double> generateIntegerImageHistogram(
  vtkImageData* imageData, const std::set<int32_t>& imageValues
)
{
  vtkNew<vtkImageAccumulate> imageHistogram;

  const auto minMax = std::minmax_element(std::begin(imageValues), std::end(imageValues));

  imageHistogram->SetInputData(imageData);
  imageHistogram->SetComponentOrigin(0, 0, 0);
  imageHistogram->SetComponentSpacing(1, 1, 1);
  imageHistogram->SetComponentExtent(*(minMax.first), *(minMax.second), 0, 0, 0, 0);
  imageHistogram->Update();

  std::map<int32_t, double> frequencies;

  for (const int32_t i : imageValues)
  {
    frequencies[i] = imageHistogram->GetOutput()->GetPointData()->GetScalars()->GetTuple1(i);
  }

  return frequencies;
}

vtkSmartPointer<vtkPolyData> generateLabelMesh(
  vtkImageData* labelData,
  const vnl_matrix_fixed<double, 3, 3>& imageDirections,
  const uint32_t labelIndex,
  const MeshPrimitiveType& primitiveType
)
{
  // This flag controls whether the label is smoothed prior to meshing:
  static constexpr bool sk_smoothImage = false;
  static constexpr double sk_imageGaussianStdev = 1.0;
  static constexpr double sk_imageGaussianRadius = 3.0;

  static constexpr bool sk_stripScalars = false;

  static constexpr bool sk_smoothMesh = true;
  static constexpr uint32_t sk_smoothingIterations = 25;
  static constexpr double sk_passBand = 0.1;
  static constexpr double sk_featureAngle = 120.0;

  if (!labelData)
  {
    return nullptr;
  }

  if (MeshPrimitiveType::TriangleFan == primitiveType)
  {
    // Not supported
    return nullptr;
  }

  vtkNew<vtkImageThreshold> imageThresholder;
  vtkNew<vtkImageCast> imageCaster;
  vtkNew<vtkImageGaussianSmooth> imageSmoother;

  vtkNew<vtkMarchingCubes> marchingCubes;
  //    vtkNew< vtkDecimatePro > decimator;
  //    vtkNew< vtkSmoothPolyDataFilter > meshSmoother;
  vtkNew<vtkTriangleFilter> triangleFilter;
  vtkNew<vtkCleanPolyData> cleanFilter;
  vtkNew<vtkStripper> triangleStripper;
  vtkNew<vtkWindowedSincPolyDataFilter> windowedSincSmoother;
  vtkNew<vtkMaskFields> scalarsMask;
  vtkNew<vtkGeometryFilter> geometryFilter;
  vtkNew<vtkTransformPolyDataFilter> transformToSubjectFilter;
  vtkNew<vtkPolyDataNormals> normalsGenerator;

  /// @todo ITK-SNAP has a nice way of combining progress of multiple functions
  vtkNew<vtkCallbackCommand> mcCallback;
  mcCallback->SetCallback(progressFunction<vtkMarchingCubes>);

  // Transformation from VTK coordinates to subject (ITK/LPS) space
  vtkNew<vtkTransform> tx_subject_O_VTK;

  const vnl_matrix_fixed<double, 4, 4> subject_O_VTK = constructVTKImageToSubjectMatrix(
    imageDirections,
    vnl_vector<double>{labelData->GetOrigin(), 3},
    vnl_vector<double>{labelData->GetSpacing(), 3}
  );

  tx_subject_O_VTK->SetMatrix(subject_O_VTK.data_block());

  // Set up label image processing pipeline
  vtkWeakPointer<vtkImageAlgorithm> imagePipelineTail = nullptr;

  // Threshold the image at a given label value
  imageThresholder->SetInputData(labelData);
  imageThresholder->SetInValue(1.0);
  imageThresholder->SetOutValue(0.0);
  imagePipelineTail = imageThresholder.GetPointer();

  // Cast to floating point, as required by the subsequent Gaussian smoothing filter
  if (!imagePipelineTail)
  {
    return nullptr;
  }
  imageCaster->SetInputConnection(imagePipelineTail->GetOutputPort());
  imageCaster->SetOutputScalarTypeToFloat();
  imagePipelineTail = imageCaster.GetPointer();

  // Approximate Gaussian smoothing
  if (sk_smoothImage)
  {
    if (!imagePipelineTail)
    {
      return nullptr;
    }
    imageSmoother->SetInputConnection(imagePipelineTail->GetOutputPort());
    imageSmoother->SetDimensionality(3);
    imageSmoother->SetStandardDeviation(sk_imageGaussianStdev);
    imageSmoother->SetRadiusFactor(sk_imageGaussianRadius);
    imagePipelineTail = imageSmoother.GetPointer();
  }

  // Set up mesh generation and processing pipeline
  vtkWeakPointer<vtkPolyDataAlgorithm> meshPipelineTail = nullptr;

  // Generate surface at half iso-surface
  marchingCubes->SetInputConnection(imagePipelineTail->GetOutputPort());
  marchingCubes->ComputeNormalsOn(); // turn off and compute below?
  marchingCubes->ComputeScalarsOff();
  marchingCubes->ComputeGradientsOff();
  marchingCubes->SetValue(0, 0.5);
  meshPipelineTail = marchingCubes.GetPointer();

  // Convert the mesh to triangles
  triangleFilter->SetInputConnection(meshPipelineTail->GetOutputPort());
  meshPipelineTail = triangleFilter.GetPointer();

  if (MeshPrimitiveType::TriangleStrip == primitiveType)
  {
    // Generate triangle strips
    if (!meshPipelineTail)
    {
      return nullptr;
    }
    triangleStripper->SetInputConnection(meshPipelineTail->GetOutputPort());
    meshPipelineTail = triangleStripper.GetPointer();
  }

  // Clean the mesh
  cleanFilter->SetInputConnection(meshPipelineTail->GetOutputPort());
  meshPipelineTail = cleanFilter.GetPointer();

  // MC observer
  //    meshPipelineTail->AddObserver( vtkCommand::ProgressEvent, mcCallback );
  //    meshPipelineTail->Update();

  // Smooth the surface
  if (sk_smoothMesh)
  {
    if (!meshPipelineTail)
    {
      return nullptr;
    }
    windowedSincSmoother->SetInputConnection(meshPipelineTail->GetOutputPort());
    windowedSincSmoother->SetNumberOfIterations(sk_smoothingIterations);
    windowedSincSmoother->SetFeatureEdgeSmoothing(1);
    windowedSincSmoother->SetFeatureAngle(sk_featureAngle);
    windowedSincSmoother->SetPassBand(sk_passBand);
    windowedSincSmoother->BoundarySmoothingOff();
    windowedSincSmoother->NonManifoldSmoothingOn();
    windowedSincSmoother->NormalizeCoordinatesOn();
    meshPipelineTail = windowedSincSmoother.GetPointer();
  }

  if (!meshPipelineTail)
  {
    return nullptr;
  }

  vtkAlgorithmOutput* algOutput = nullptr;

  if (sk_stripScalars)
  {
    // Strip scalars from the points and cells
    scalarsMask->SetInputConnection(meshPipelineTail->GetOutputPort());
    scalarsMask->CopyAttributeOff(vtkMaskFields::POINT_DATA, vtkDataSetAttributes::SCALARS);
    scalarsMask->CopyAttributeOff(vtkMaskFields::CELL_DATA, vtkDataSetAttributes::SCALARS);
    algOutput = scalarsMask->GetOutputPort();
  }
  else
  {
    algOutput = meshPipelineTail->GetOutputPort();
  }

  // Convert to polyData
  geometryFilter->SetInputConnection(algOutput);
  meshPipelineTail = geometryFilter.GetPointer();

  // Transform to subject space
  if (!meshPipelineTail)
  {
    return nullptr;
  }
  transformToSubjectFilter->SetInputConnection(meshPipelineTail->GetOutputPort());
  transformToSubjectFilter->SetTransform(tx_subject_O_VTK.GetPointer());
  meshPipelineTail = transformToSubjectFilter.GetPointer();

  // Generate vertex normal vectors
  if (!meshPipelineTail)
  {
    return nullptr;
  }
  normalsGenerator->SetInputConnection(meshPipelineTail->GetOutputPort());
  normalsGenerator->ComputePointNormalsOn();
  normalsGenerator->ComputeCellNormalsOff();
  normalsGenerator->SetFeatureAngle(sk_featureAngle);
  normalsGenerator->FlipNormalsOff();
  normalsGenerator->SplittingOn();
  normalsGenerator->ConsistencyOff();
  normalsGenerator->AutoOrientNormalsOn();
  //    normalsGenerator->SetNonManifoldTraversal(1);
  meshPipelineTail = normalsGenerator.GetPointer();

  vtkSmartPointer<ErrorObserver> errorObserver = vtkSmartPointer<ErrorObserver>::New();

  // Select data for a label
  imageThresholder
    ->ThresholdBetween(static_cast<double>(labelIndex), static_cast<double>(labelIndex));

  if (!meshPipelineTail)
  {
    return nullptr;
  }

  // Run pipeline
  meshPipelineTail->Update();

  //        if ( errorObserver->GetError() )
  //        {
  //            std::cout << "Caught error! " << errorObserver->GetErrorMessage();
  //        }

  //        if ( errorObserver->GetWarning() )
  //        {
  //            std::cout << "Caught warning! " << errorObserver->GetWarningMessage();
  //        }

  //        static const std::string filePrefix = "label_";
  //        std::stringstream ss;
  //        ss << filePrefix << label << ".vtk" << std::ends;
  //        writePolyData( meshPipelineTail->GetOutput(), ss.str().c_str() );

  //    vtkSmartPointer<vtkPolyData> polyDataCopy = vtkSmartPointer<vtkPolyData>::New();
  //    polyDataCopy->DeepCopy( meshPipelineTail->GetOutput() );
  //    return polyDataCopy;

  return meshPipelineTail->GetOutput();
}

} // namespace vtkdetails
