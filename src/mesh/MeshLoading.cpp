#include "mesh/MeshLoading.h"
#include "mesh/MeshCpuRecord.h"
#include "mesh/vtkdetails/MeshGeneration.hpp"

#include "common/MathFuncs.h"
#include "common/UuidUtility.h"

#include "image/Image.h"
#include "image/ImageHeader.h"
#include "image/ImageUtility.tpp"

#include "rendering/utility/CreateGLObjects.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vnl/vnl_matrix_fixed.h>

#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <limits>
#include <utility>

namespace
{

std::unique_ptr<MeshCpuRecord> _generateIsosurfaceMeshCpuRecord(
  vtkImageData* imageData, const ImageHeader& imageHeader, double isoValue
)
{
  // Note: triangle strips offer no speed advantage over indexed triangles on modern hardware
  static const MeshPrimitiveType sk_primitiveType = MeshPrimitiveType::Triangles;

  if (!imageData)
  {
    spdlog::error("Error generating iso-surface mesh: Image data is null.");
    return nullptr;
  }

  const vnl_matrix_fixed<double, 3, 3> imageDirections = math::convert::toVnlMatrixFixed(
    glm::dmat3{imageHeader.directions()}
  );

  vtkSmartPointer<vtkPolyData> polyData = nullptr;

  try
  {
    polyData
      = ::vtkdetails::generateIsoSurfaceMesh(imageData, imageDirections, isoValue, sk_primitiveType);
  }
  catch (const std::exception& e)
  {
    spdlog::error("Error generating iso-surface mesh: {}", e.what());
    return nullptr;
  }
  catch (...)
  {
    spdlog::error("Error generating iso-surface mesh");
    return nullptr;
  }

  if (!polyData)
  {
    spdlog::error("Error generating iso-surface mesh: vtkPolyData is null.");
    return nullptr;
  }

  return std::make_unique<
    MeshCpuRecord>(polyData, MeshInfo(MeshSource::IsoSurface, sk_primitiveType, isoValue));
}

/*
std::unique_ptr<MeshCpuRecord> generateLabelMesh(
    vtkImageData* imageData,
    const ImageHeader& imageHeader,
    uint32_t labelIndex )
{
    // Note: triangle strips offer no speed advantage over indexed triangles on modern hardware
    static const MeshPrimitiveType sk_primitiveType = MeshPrimitiveType::Triangles;

    // Parcellation image must have exactly one scalar component
    if ( 1 != imageHeader.numComponentsPerPixel() ||
        PixelType::Scalar != imageHeader.pixelType() )
    {
        spdlog::error( "Error computing mesh at label index {}: "
                      "Pixel type must be single-component scalar.", labelIndex );
        return nullptr;
    }

    // Parcellation pixels are indices that must be of unsigned integer type
    if ( ! isUnsignedIntegerType( imageHeader.memoryComponentType() ) )
    {
        spdlog::error( "Error computing mesh at label index {}: "
                      "Parcellation component type must be unsigned integral.", labelIndex );
        return nullptr;
    }

    if ( ! imageData )
    {
        spdlog::error( "Parcellation image data is null" );
        return nullptr;
    }

    const vnl_matrix_fixed< double, 3, 3 > imageDirections =
        math::convert::toVnlMatrixFixed( glm::dmat3{ imageHeader.directions() } );

    try
    {
        auto polyData = ::vtkdetails::generateLabelMesh(
            imageData, imageDirections, labelIndex, sk_primitiveType );

        if ( ! polyData )
        {
            spdlog::error( "Error computing mesh at label index {} ", labelIndex );
            return nullptr;
        }

        return std::make_unique<MeshCpuRecord>(
            polyData, MeshInfo( MeshSource::Label, sk_primitiveType, labelIndex ) );
    }
    catch ( const std::exception& e )
    {
        spdlog::error( "Error generating label mesh at index {}: {}", labelIndex, e.what() );
        return nullptr;
    }
    catch ( ... )
    {
        spdlog::error( "Error generating label mesh at index {}", labelIndex);
        return nullptr;
    }
}
*/

} // namespace

std::future<AsyncTaskDetails> generateIsosurfaceMeshCpuRecord(
  const Image& image,
  const uuids::uuid& imageUid,
  uint32_t component,
  double isoValue,
  const uuids::uuid& isosurfaceUid,
  std::function<bool(const uuids::uuid& isosurfaceUid, std::unique_ptr<MeshCpuRecord>)>
    meshCpuRecordUpdater,
  std::function<void()> addTaskToIsosurfaceGpuMeshGenerationQueue
)
{
  // Lambda to generate the CPU mesh record using VTK's marching cubes.
  // Need to capture by value, since the function is executed asynchronously.
  auto generateMesh =
    [=](const std::function<void(bool success, std::unique_ptr<MeshCpuRecord>)>& onGenerateDone)
  {
    spdlog::info(
      "Start generating mesh for isosurface {} at value {} of image {}",
      isosurfaceUid,
      isoValue,
      imageUid
    );

    AsyncTaskDetails retval;
    retval.task = AsyncTasks::IsosurfaceMeshGeneration;
    retval.description = std::string("Generate mesh at image isovalue " + std::to_string(isoValue));
    retval.imageUid = imageUid;
    retval.imageComponent = component;
    retval.objectUid = isosurfaceUid;
    retval.success = false;

    // Cast image component to float prior to mesh generation
    using ImageCompType = float;
    const auto itkImage = createItkImageFromImageComponent<ImageCompType>(image, component);
    const auto vtkImageData = convertItkImageToVtkImageData<ImageCompType>(itkImage);

    if (!vtkImageData)
    {
      spdlog::error("Image {} has null vtkImageData when generating isosurface", imageUid);
      onGenerateDone(false, nullptr);
      return retval;
    }

    auto cpuRecord = _generateIsosurfaceMeshCpuRecord(vtkImageData.Get(), image.header(), isoValue);

    if (!cpuRecord)
    {
      spdlog::error("Error generating isosurface CPU mesh record for image {}", imageUid);
      onGenerateDone(false, nullptr);
      return retval;
    }

    spdlog::info(
      "Done generating mesh for isosurface {} at value {} of image", isosurfaceUid, isoValue, imageUid
    );

    onGenerateDone(true, std::move(cpuRecord));

    retval.success = true;
    return retval;
  };

  // Called when mesh generation is done
  auto generateDone = [=](bool success, std::unique_ptr<MeshCpuRecord> cpuMeshRecord)
  {
    if (!success || !cpuMeshRecord)
    {
      spdlog::error("CPU mesh record for isosurface was not generated successfully");
      return false;
    }

    if (meshCpuRecordUpdater(isosurfaceUid, std::move(cpuMeshRecord)))
    {
      spdlog::debug("Updated mesh CPU record for isosurface {}", isosurfaceUid);
      addTaskToIsosurfaceGpuMeshGenerationQueue();
      return true;
    }
    else
    {
      spdlog::error("Error updating mesh CPU record for isosurface {}", isosurfaceUid);
      return false;
    }
  };

  return std::async(std::launch::async, generateMesh, generateDone);
}

bool writeMeshToFile(const MeshCpuRecord& record, const std::string& fileName)
{
  if (record.polyData().GetPointer())
  {
    return (::vtkdetails::writePolyData(record.polyData(), fileName.c_str()));
  }

  return false;
}
