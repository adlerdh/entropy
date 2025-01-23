#include "logic/data/DataLoading.h"
#include "logic/data/details/DataLoadingDetails.h"

#include "logic/managers/DataManager.h"
#include "logic/records/ImageColorMapRecord.h"
#include "logic/records/LabelTableRecord.h"

#include "imageio/util/CreateParcellationImage.h"
#include "mesh/vtkdetails/MeshGeneration.hpp"
#include "rendering/utility/CreateGLObjects.h"
#include "slideio/SlideHelper.h"

#include <iostream>
#include <limits>
#include <numeric>

namespace
{

// Only load the first (0th) component of multi-component images
static constexpr uint32_t sk_compToLoad = 0;

// Default 3D image opacity
static constexpr double sk_image3dOpacity = 1.0;

// Default 3D parcellation opacity
static constexpr double sk_parcel3dOpacity = 0.5;

} // namespace

namespace data
{

std::optional<UID> loadImage(
  DataManager& dataManager,
  const std::string& filename,
  const std::optional<std::string>& dicomSeriesUid
)
{
  auto cpuRecord = details::generateImageCpuRecord(
    filename, dicomSeriesUid, imageio::ComponentNormalizationPolicy::None
  );

  if (!cpuRecord)
  {
    std::cerr << "Error loading image from file '" << filename << "'" << std::endl;
    return std::nullopt;
  }

  // Use linear interpolation for images with floating point data type. Linear interpolation
  // (tex::MinificationFilter::Linear and tex::MagnificationFilter::Linear) only work with
  // floating point data type

  const bool isFloatBuffer = imageio::isFloatingType(cpuRecord->header().m_bufferComponentType);

  const tex::MinificationFilter minifFilter = (isFloatBuffer) ? tex::MinificationFilter::Linear
                                                              : tex::MinificationFilter::Nearest;

  const tex::MagnificationFilter magnifFilter = (isFloatBuffer) ? tex::MagnificationFilter::Linear
                                                                : tex::MagnificationFilter::Nearest;

  static constexpr bool sk_useNormalizedIntegers = true;

  auto gpuRecord = gpuhelper::createImageGpuRecord(
    cpuRecord.get(), sk_compToLoad, minifFilter, magnifFilter, sk_useNormalizedIntegers
  );

  if (!gpuRecord)
  {
    std::cerr << "Error creating image GPU record for file '" << filename << "'" << std::endl;
    return std::nullopt;
  }

  std::ostringstream ss;
  ss << "Loaded image from '" << filename << "':" << std::endl
     << "Header:\n"
     << cpuRecord->header() << std::endl
     << "Transformation:\n"
     << cpuRecord->transformations() << std::ends;

  std::cout << ss.str() << std::endl;

  ///// @test Set low threshold to 50% quantile value
  //    if ( auto stats = cpuRecord->imageBaseData()->pixelStatistics( 0 ) )
  //    {
  //        cpuRecord->setThresholdLow( 0, stats->m_quantiles[ 50 ] );
  //        cpuRecord->setThresholdHigh( 0, stats->m_quantiles[ 100 ] );
  //    }

  cpuRecord->setOpacity(0, sk_image3dOpacity);

  //    cpuRecord->setWorldSubjectOrigin( glm::vec3{10,0,0} );

  auto imageUid = dataManager.insertImageRecord(
    std::make_shared<ImageRecord>(std::move(cpuRecord), std::move(gpuRecord))
  );

  if (!imageUid)
  {
    std::cerr << "Error loading image from file '" << filename << "' "
              << "into DataManager." << std::endl;
    return std::nullopt;
  }

  std::cout << "Image UID is " << *imageUid << std::endl;

  // Make this the active image
  dataManager.setActiveImageUid(imageUid);

  // Associate the default color map with the image:
  auto defaultMapUid = dataManager.defaultImageColorMapUid();
  if (!defaultMapUid)
  {
    std::ostringstream ss;
    ss << "Default image color map does not exist" << std::ends;
    std::cerr << ss.str() << std::ends;
    return std::nullopt;
  }

  if (!dataManager.associateColorMapWithImage(*imageUid, *defaultMapUid))
  {
    std::ostringstream ss;
    ss << "Error associating default color map with image " << *imageUid << std::ends;
    std::cerr << ss.str() << std::ends;
    return std::nullopt;
  }

  return imageUid;
}

std::optional<UID> loadParcellation(
  DataManager& dataManager,
  const std::string& filename,
  const std::optional<std::string>& dicomSeriesUid
)
{
  // Step 1) Load parcellation image
  auto imageCpuRecord = details::generateImageCpuRecord(
    filename, dicomSeriesUid, imageio::ComponentNormalizationPolicy::None
  );

  if (!imageCpuRecord || !imageCpuRecord->imageBaseData())
  {
    std::cerr << "Error loading parcellation image from file '" << filename << "'" << std::endl;
    return std::nullopt;
  }

  std::ostringstream ss;
  ss << "Loaded image from '" << filename << "'" << std::endl
     << std::endl
     << "Header:\n"
     << imageCpuRecord->header() << std::endl
     << std::endl
     << "Transformation:\n"
     << imageCpuRecord->transformations() << std::ends;
  std::cout << ss.str() << std::endl;

  if (imageio::isFloatingType(imageCpuRecord->header().m_bufferComponentType))
  {
    std::cerr << "Cannot load parcellation image: only integer "
              << "pixel component types are valid." << std::endl;
    return std::nullopt;
  }

  // Step 2) Convert image record to parcellation record. This function "squashes"
  // empty space between label values
  auto parcelCpuRecord = imageio::createParcellationCpuRecord(*imageCpuRecord);
  if (!parcelCpuRecord || !parcelCpuRecord->imageBaseData())
  {
    std::cerr << "Error creating parcellation CPU record for '" << filename << "'" << std::endl;
    return std::nullopt;
  }

  ss.str(std::string());
  ss << "Generated parcellation from '" << filename << "'" << std::endl
     << std::endl
     << "Header:\n"
     << parcelCpuRecord->header() << std::endl
     << std::endl
     << "Transformation:\n"
     << parcelCpuRecord->transformations() << std::ends;
  std::cout << ss.str() << std::endl;

  parcelCpuRecord->setOpacity(0, sk_parcel3dOpacity);

  // Create the GPU texture for the parcellation image. The parcellation image must use
  // nearest-neighbor resampling, since its voxels represent segmentation labels.

  static constexpr bool sk_useNormalizedIntegers = false;

  auto gpuRecord = gpuhelper::createImageGpuRecord(
    parcelCpuRecord.get(),
    sk_compToLoad,
    tex::MinificationFilter::Nearest,
    tex::MagnificationFilter::Nearest,
    sk_useNormalizedIntegers
  );

  // Note: grab the min/max label values before the parcellation record is moved into DataManager
  const auto minMaxLabelValues = parcelCpuRecord->minMaxLabelValues();

  auto parcelUid = dataManager.insertParcellationRecord(
    std::make_shared<ParcellationRecord>(std::move(parcelCpuRecord), std::move(gpuRecord))
  );

  if (!parcelUid)
  {
    std::cerr << "Error loading parcellation from file '" << filename << "' "
              << "into DataManager." << std::endl;
    return std::nullopt;
  }

  std::cout << "Parcellation UID is " << *parcelUid << std::endl;

  // Set this as the active parcellation
  dataManager.setActiveParcellationUid(*parcelUid);

  // Create a label table for the parcellation image: Set the size of the table
  // (number of labels) to equal the number of label values.
  /// @todo Create ability to load label table from file

  if (minMaxLabelValues.second < minMaxLabelValues.first)
  {
    return std::nullopt; // Something has gone wrong
  }

  const size_t tableSize = static_cast<size_t>(
    minMaxLabelValues.second - minMaxLabelValues.first + 1
  );

  auto labelTableRecord = details::createLabelTableRecord(tableSize);
  if (!labelTableRecord)
  {
    std::ostringstream ss;
    ss << "Error creating label table for parcellation " << *parcelUid << std::ends;
    std::cerr << ss.str() << std::endl;

    dataManager.unloadParcellation(*parcelUid);
    return std::nullopt;
  }

  auto labelTableUid = dataManager.insertLabelTableRecord(labelTableRecord);
  if (!labelTableUid)
  {
    std::ostringstream ss;
    ss << "Error loading label table for parcellation " << *parcelUid << std::ends;
    std::cerr << ss.str() << std::endl;

    dataManager.unloadParcellation(*parcelUid);
    return std::nullopt;
  }

  if (!dataManager.associateLabelTableWithParcellation(*parcelUid, *labelTableUid))
  {
    std::ostringstream ss;
    ss << "Error associating label table " << *labelTableUid << " with parcellation " << *parcelUid
       << std::ends;
    std::cerr << ss.str() << std::endl;

    dataManager.unloadParcellation(*parcelUid);
    return std::nullopt;
  }

  return *parcelUid;
}

std::optional<UID> loadSlide(
  DataManager& dataManager, const std::string& filename, bool translateToTopOfStack
)
{
  auto cpuRecord = details::generateSlideCpuRecord(filename);
  if (!cpuRecord)
  {
    std::ostringstream ss;
    ss << "Unable to load slide from file '" << filename << "'" << std::ends;
    std::cerr << ss.str() << std::endl;
    return std::nullopt;
  }

  float stackTranslation = 0.0f;

  if (translateToTopOfStack)
  {
    // Adjust translation along stack's Z axis such that this slide is on top of the stack
    stackTranslation = slideio::slideStackHeight(dataManager.slideRecords())
                       + 2.0f * cpuRecord->header().thickness();
  }

  cpuRecord->transformation().setStackTranslationZ(stackTranslation);

  auto gpuRecord = gpuhelper::createSlideGpuRecord(cpuRecord.get());
  if (!gpuRecord)
  {
    std::ostringstream ss;
    ss << "Unable to generate texture for slide file '" << filename << "'" << std::ends;
    std::cerr << ss.str() << std::endl;
    return std::nullopt;
  }

  auto record = std::make_shared<SlideRecord>(std::move(cpuRecord), std::move(gpuRecord));

  const auto slideUid = dataManager.insertSlideRecord(record);
  if (!slideUid)
  {
    std::ostringstream ss;
    ss << "Unable to load slide from file '" << filename << "'" << std::ends;
    std::cerr << ss.str() << std::endl;
    return std::nullopt;
  }

  // If no slide is active, make this the active one
  if (!dataManager.activeSlideUid())
  {
    dataManager.setActiveSlideUid(*slideUid);
  }

  return *slideUid;
}

std::optional<UID> getActiveParcellation(DataManager& dataManager, const UID& imageUid)
{
  // Return the active parcellation, if one exists
  if (auto parcelUid = dataManager.activeParcellationUid())
  {
    return *parcelUid;
  }

  // Check whether the image has a default parcellation.
  // If so, set it as active and return it.
  if (auto parcelUid = dataManager.defaultParcellationUid_of_image(imageUid))
  {
    dataManager.setActiveParcellationUid(*parcelUid);
    return *parcelUid;
  }

  std::cerr << "No active parcellation found: generating default one for image " << imageUid
            << std::endl;

  // Generate a blank parcellation for the image and set it as active
  if (auto blankParcelUid = details::createBlankParcellation(dataManager, imageUid))
  {
    dataManager.setActiveParcellationUid(*blankParcelUid);
    return *blankParcelUid;
  }

  return std::nullopt;
}

std::optional<UID> generateIsoSurfaceMesh(
  DataManager& dataManager, const UID& imageUid, double isoValue
)
{
  auto meshCpuRecord = details::generateIsoSurfaceMeshCpuRecord(dataManager, imageUid, isoValue);

  if (!meshCpuRecord)
  {
    std::ostringstream ss;
    ss << "Error generating iso-surface mesh for image " << imageUid << " at value " << isoValue
       << std::ends;
    std::cerr << ss.str() << std::endl;
    return std::nullopt;
  }

  auto meshGpuRecord = gpuhelper::createMeshGpuRecordFromVtkPolyData(
    meshCpuRecord->polyData(),
    meshCpuRecord->meshInfo().primitiveType(),
    BufferUsagePattern::StreamDraw
  );

  if (!meshGpuRecord)
  {
    std::ostringstream ss;
    ss << "Error converting PolyData to MeshGpuRecord: "
       << "Could not generate mesh record for image " << imageUid << " at isosurface value "
       << isoValue << std::ends;
    std::cerr << ss.str() << std::endl;
    return std::nullopt;
  }

  return dataManager.insertIsoMeshRecord(
    imageUid, std::make_shared<MeshRecord>(std::move(meshCpuRecord), std::move(meshGpuRecord))
  );
}

std::vector<UID> generateLabelMeshes(
  DataManager& dataManager, const UID& parcelUid, const std::set<uint32_t>& labelIndices
)
{
  std::vector<UID> generatedMeshUids;

  auto parcelRecord = dataManager.parcellationRecord(parcelUid).lock();

  if (!parcelRecord || !parcelRecord->cpuData() || !parcelRecord->cpuData()->imageBaseData())
  {
    std::ostringstream ss;
    ss << "Null parcellation " << parcelUid << std::ends;
    std::cerr << ss.str() << std::endl;
    return generatedMeshUids;
  }

  // Attempt to generate meshes from all label indices, ignoring indices for which
  // label meshes have already been generated.

  // Map of all existing label meshes for this parcellation:
  const std::map<uint32_t, UID> labelMeshUids = dataManager.labelMeshUids_of_parcellation(parcelUid);

  for (const uint32_t labelIndex : labelIndices)
  {
    auto it = labelMeshUids.find(labelIndex);
    if (std::end(labelMeshUids) != it)
    {
      // This label mesh already exists, so do not re-generate it.
      continue;
    }

    // Convert label index to label value:
    const auto labelValue = parcelRecord->cpuData()->labelValue(labelIndex);
    if (!labelValue)
    {
      // Ignore label index that maps to no label value in parcellation
      continue;
    }

    if (0 == *labelValue)
    {
      // Ignore the zero label value. Do not generate a mesh for it.
      continue;
    }

    if (auto meshUid = details::generateLabelMeshRecord(dataManager, parcelUid, labelIndex))
    {
      generatedMeshUids.push_back(*meshUid);
    }
  }

  return generatedMeshUids;
}

std::vector<UID> generateAllLabelMeshes(DataManager& dataManager, const UID& parcelUid)
{
  std::vector<UID> generatedMeshUids;

  auto parcelRecord = dataManager.parcellationRecord(parcelUid).lock();

  if (!parcelRecord || !parcelRecord->cpuData() || !parcelRecord->cpuData()->imageBaseData())
  {
    std::ostringstream ss;
    ss << "Null data in parcellation " << parcelUid << std::ends;
    std::cerr << ss.str() << std::endl;
    return generatedMeshUids;
  }

  auto baseData = parcelRecord->cpuData()->imageBaseData();

  // Parcellation pixels are indices into labels values
  const vtkSmartPointer<vtkImageData> labelsIndexVtkData = baseData->asVTKImageData(sk_compToLoad);

  if (!labelsIndexVtkData)
  {
    std::ostringstream ss;
    ss << "Parcellation " << parcelUid << " has null VTK data" << std::ends;
    std::cerr << ss.str() << std::endl;
    return generatedMeshUids;
  }

  // Create a set of all label indices in the image. The function
  // vtkdetails::generateIntegerImageHistogram (and its underlying VTK function)
  // require values of type int32_t for building a histogram. Therefore,
  // indices are cast to int32_t and it is assumed that there are less than 2^31 - 1
  // label values. (Hopefully a safe assumption!)

  if (parcelRecord->cpuData()->maxLabelIndex() >= std::numeric_limits<int32_t>::max())
  {
    std::ostringstream ss;
    ss << "Warning: the parcellation contains " << parcelRecord->cpuData()->maxLabelIndex()
       << " labels, which is more than the maximum number allowed." << std::ends;
    std::cerr << ss.str() << std::endl;
  }

  const int32_t maxLabelIndex = static_cast<int32_t>(std::min(
    parcelRecord->cpuData()->maxLabelIndex(),
    static_cast<size_t>(std::numeric_limits<int32_t>::max())
  ));

  // Set of all label indices
  std::set<int32_t> labelIndices;

  auto it = std::end(labelIndices);
  for (int32_t index = 0; index <= maxLabelIndex; ++index)
  {
    it = labelIndices.insert(it, index);
  }

  // Generate histogram of label indices in parcellation
  /// @todo Move this function from vtkdetails to meshgen
  const std::map<int32_t, double> histogram
    = vtkdetails::generateIntegerImageHistogram(labelsIndexVtkData, labelIndices);

  for (const auto& bin : histogram)
  {
    const uint32_t labelIndex = static_cast<uint32_t>(bin.first);
    const double labelFrequency = bin.second;

    // Convert label index to label value:
    const auto labelValue = parcelRecord->cpuData()->labelValue(labelIndex);

    if (!labelValue)
    {
      // Skip invalid label index that maps to no label value
      continue;
    }

    if (0 == *labelValue)
    {
      // Ignore label value 0: Do not generate a mesh from the background label
      continue;
    }

    if (0.0 < labelFrequency)
    {
      // This label value occurs in the parcellation.
      // Do not attempt to generate mesh for label that has 0 frequency.
      if (auto meshUid = details::generateLabelMeshRecord(dataManager, parcelUid, labelIndex))
      {
        generatedMeshUids.push_back(*meshUid);
      }
    }
  }

  return generatedMeshUids;
}

std::vector<UID> loadImageColorMaps(DataManager& dataManager, const std::string& directoryPath)
{
  auto colorMapCpuRecords = details::loadImageColorMapsFromDirectory(directoryPath);

  std::vector<UID> colorMapUids;

  for (auto& mapCpuRecord : colorMapCpuRecords)
  {
    if (!mapCpuRecord)
    {
      continue;
    }

    auto mapGpuRecord = gpuhelper::createImageColorMapTexture(mapCpuRecord.get());
    if (!mapGpuRecord)
    {
      continue;
    }

    auto record
      = std::make_shared<ImageColorMapRecord>(std::move(mapCpuRecord), std::move(mapGpuRecord));

    if (const auto uid = dataManager.insertImageColorMapRecord(record))
    {
      colorMapUids.push_back(*uid);
    }
    else
    {
      std::cerr << "Error loading image color map! Skipping it." << std::endl;
    }
  }

  return colorMapUids;
}

std::optional<UID> loadImageColorMap(DataManager& dataManager, const std::string& filePath)
{
  auto mapCpuRecord = details::loadImageColorMapWithQt(filePath);
  if (!mapCpuRecord)
  {
    std::cerr << "Error loading image color map from file '" << filePath << "'" << std::endl;
    return std::nullopt;
  }

  auto mapGpuRecord = gpuhelper::createImageColorMapTexture(mapCpuRecord.get());
  if (!mapGpuRecord)
  {
    std::cerr << "Error creating image color map GPU record from file '" << filePath << "'"
              << std::endl;
    return std::nullopt;
  }

  auto record
    = std::make_shared<ImageColorMapRecord>(std::move(mapCpuRecord), std::move(mapGpuRecord));

  return dataManager.insertImageColorMapRecord(record);
}

std::optional<UID> loadDefaultGreyscaleColorMap(DataManager& dataManager)
{
  if (auto cmapUid = dataManager.insertImageColorMapRecord(details::createDefaultGreyscaleImageColorMapRecord()))
  {
    if (dataManager.setDefaultImageColorMapUid(*cmapUid))
    {
      return *cmapUid;
    }
  }

  return std::nullopt;
}

} // namespace data
