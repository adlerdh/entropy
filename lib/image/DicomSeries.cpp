#include "image/DicomSeries.h"

#include "common/MathFuncs.h"

#include "image/internal/ImageUtility.tpp"
#include "image/internal/ImageUtilityItk.h"

#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImageFileReader.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageSeriesReader.h>
#include <itkMetaDataObject.h>

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <limits>
#include <set>
#include <sstream>
#include <type_traits>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace
{
using MetadataDictionary = itk::MetaDataDictionary;

constexpr std::array<const char*, 9> sk_phiTags{
  "0010|0010", // PatientName
  "0010|0020", // PatientID
  "0010|0030", // PatientBirthDate
  "0010|0040", // PatientSex
  "0008|0050", // AccessionNumber
  "0008|0080", // InstitutionName
  "0008|0090", // ReferringPhysicianName
  "0008|1048", // PhysiciansOfRecord
  "0008|1050"  // PerformingPhysicianName
};

constexpr std::array<const char*, 18> sk_summaryTags{
  "0008|0020", // StudyDate
  "0008|0030", // StudyTime
  "0008|0060", // Modality
  "0008|0070", // Manufacturer
  "0008|1030", // StudyDescription
  "0008|103e", // SeriesDescription
  "0018|1030", // ProtocolName
  "0018|0050", // SliceThickness
  "0018|0088", // SpacingBetweenSlices
  "0020|000d", // StudyInstanceUID
  "0020|000e", // SeriesInstanceUID
  "0020|0011", // SeriesNumber
  "0020|0032", // ImagePositionPatient
  "0020|0037", // ImageOrientationPatient
  "0028|0008", // NumberOfFrames
  "0018|0040", // CineRate
  "0018|1063", // FrameTime
  "0018|1065"  // FrameTimeVector
};

const std::unordered_map<std::string, std::string>& metadataTagNames()
{
  static const std::unordered_map<std::string, std::string> names{
    {"0008|0008", "Image Type"},
    {"0008|0016", "SOP Class UID"},
    {"0008|0018", "SOP Instance UID"},
    {"0008|0020", "Study Date"},
    {"0008|0021", "Series Date"},
    {"0008|0022", "Acquisition Date"},
    {"0008|0023", "Content Date"},
    {"0008|0030", "Study Time"},
    {"0008|0031", "Series Time"},
    {"0008|0032", "Acquisition Time"},
    {"0008|0033", "Content Time"},
    {"0008|0050", "Accession Number"},
    {"0008|0060", "Modality"},
    {"0008|0070", "Manufacturer"},
    {"0008|0080", "Institution Name"},
    {"0008|0090", "Referring Physician Name"},
    {"0008|1030", "Study Description"},
    {"0008|103e", "Series Description"},
    {"0008|1048", "Physicians of Record"},
    {"0008|1050", "Performing Physician Name"},
    {"0010|0010", "Patient Name"},
    {"0010|0020", "Patient ID"},
    {"0010|0030", "Patient Birth Date"},
    {"0010|0040", "Patient Sex"},
    {"0018|0015", "Body Part Examined"},
    {"0018|0020", "Scanning Sequence"},
    {"0018|0021", "Sequence Variant"},
    {"0018|0022", "Scan Options"},
    {"0018|0023", "MR Acquisition Type"},
    {"0018|0040", "Cine Rate"},
    {"0018|0050", "Slice Thickness"},
    {"0018|0080", "Repetition Time"},
    {"0018|0081", "Echo Time"},
    {"0018|0082", "Inversion Time"},
    {"0018|0083", "Number of Averages"},
    {"0018|0084", "Imaging Frequency"},
    {"0018|0085", "Imaged Nucleus"},
    {"0018|0086", "Echo Numbers"},
    {"0018|0087", "Magnetic Field Strength"},
    {"0018|0088", "Spacing Between Slices"},
    {"0018|0091", "Echo Train Length"},
    {"0018|1020", "Software Versions"},
    {"0018|1030", "Protocol Name"},
    {"0018|1063", "Frame Time"},
    {"0018|1065", "Frame Time Vector"},
    {"0018|1310", "Acquisition Matrix"},
    {"0018|1312", "In-plane Phase Encoding Direction"},
    {"0018|1314", "Flip Angle"},
    {"0020|000d", "Study Instance UID"},
    {"0020|000e", "Series Instance UID"},
    {"0020|0010", "Study ID"},
    {"0020|0011", "Series Number"},
    {"0020|0012", "Acquisition Number"},
    {"0020|0013", "Instance Number"},
    {"0020|0032", "Image Position Patient"},
    {"0020|0037", "Image Orientation Patient"},
    {"0020|0052", "Frame of Reference UID"},
    {"0020|1040", "Position Reference Indicator"},
    {"0020|1041", "Slice Location"},
    {"0028|0002", "Samples per Pixel"},
    {"0028|0004", "Photometric Interpretation"},
    {"0028|0008", "Number of Frames"},
    {"0028|0010", "Rows"},
    {"0028|0011", "Columns"},
    {"0028|0030", "Pixel Spacing"},
    {"0028|0100", "Bits Allocated"},
    {"0028|0101", "Bits Stored"},
    {"0028|0102", "High Bit"},
    {"0028|0103", "Pixel Representation"},
    {"0028|1050", "Window Center"},
    {"0028|1051", "Window Width"},
    {"0028|1052", "Rescale Intercept"},
    {"0028|1053", "Rescale Slope"},
    {"0028|1054", "Rescale Type"}};
  return names;
}

std::string normalizeTag(std::string tag)
{
  std::transform(tag.begin(), tag.end(), tag.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return tag;
}

std::optional<std::string> exposeString(const MetadataDictionary& dict, const std::string& tag)
{
  std::string value;
  if (itk::ExposeMetaData<std::string>(dict, tag, value)) {
    return value;
  }
  return std::nullopt;
}

std::string metadataValue(const MetadataDictionary& dict, const std::string& tag)
{
  return exposeString(dict, tag).value_or(std::string{});
}

std::optional<glm::vec3> parseDicomVec3(std::string value)
{
  std::replace(value.begin(), value.end(), static_cast<char>(92), ' ');
  std::istringstream stream(value);

  glm::vec3 result{0.0f};
  if (stream >> result.x >> result.y >> result.z) {
    return result;
  }
  return std::nullopt;
}

std::optional<double> parseDicomDouble(std::string value)
{
  std::replace(value.begin(), value.end(), static_cast<char>(92), ' ');
  std::istringstream stream(value);
  double result = 0.0;
  if (stream >> result) {
    return result;
  }
  return std::nullopt;
}

std::optional<uint32_t> parseDicomUInt(std::string value)
{
  if (const auto parsed = parseDicomDouble(std::move(value)); parsed && *parsed >= 0.0) {
    return static_cast<uint32_t>(*parsed);
  }
  return std::nullopt;
}

std::vector<dicom::RawMetadataEntry> rawMetadataEntries(const MetadataDictionary& dict)
{
  std::vector<dicom::RawMetadataEntry> entries;

  auto addEntry = [&](const std::string& rawTag) {
    if (const auto value = exposeString(dict, rawTag); value && !value->empty()) {
      entries.push_back(dicom::RawMetadataEntry{rawTag, *value});
    }
  };

  for (const char* rawTag : sk_summaryTags) {
    addEntry(rawTag);
  }

  for (const auto& rawKey : dict.GetKeys()) {
    addEntry(rawKey);
  }

  return entries;
}

dicom::SeriesMetadata readSeriesMetadata(const MetadataDictionary& dict)
{
  dicom::SeriesMetadata metadata;
  metadata.studyInstanceUid = metadataValue(dict, "0020|000d");
  metadata.seriesInstanceUid = metadataValue(dict, "0020|000e");
  metadata.studyDescription = metadataValue(dict, "0008|1030");
  metadata.seriesDescription = metadataValue(dict, "0008|103e");
  metadata.protocolName = metadataValue(dict, "0018|1030");
  metadata.modality = metadataValue(dict, "0008|0060");
  metadata.manufacturer = metadataValue(dict, "0008|0070");
  metadata.seriesNumber = metadataValue(dict, "0020|0011");
  metadata.acquisitionDate = metadataValue(dict, "0008|0020");
  return metadata;
}

dicom::SeriesTemporalInfo readTemporalInfo(const MetadataDictionary& dict)
{
  dicom::SeriesTemporalInfo temporal;
  if (const auto frames = parseDicomUInt(metadataValue(dict, "0028|0008")); frames && *frames > 1u) {
    temporal.numTimePoints = *frames;
    temporal.multiframe = true;
  }

  if (const auto frameTimeMs = parseDicomDouble(metadataValue(dict, "0018|1063")); frameTimeMs && *frameTimeMs > 0.0) {
    temporal.spacing = *frameTimeMs;
    temporal.units = "ms";
  }
  else if (const auto cineRate = parseDicomDouble(metadataValue(dict, "0018|0040")); cineRate && *cineRate > 0.0) {
    temporal.spacing = 1.0 / *cineRate;
    temporal.units = "sec";
  }
  else if (temporal.multiframe) {
    temporal.spacing = 1.0;
    temporal.units = "frame";
  }

  return temporal;
}

std::string sliceOrientationFromDirections(const glm::mat3& directions)
{
  const auto spiral = math::computeSpiralCodeFromDirectionMatrix(glm::dmat3{directions});
  const glm::vec3 normal = glm::abs(glm::normalize(directions[2]));

  std::string plane = "axial";
  float maxDot = normal.z;
  if (normal.y > maxDot) {
    plane = "coronal";
    maxDot = normal.y;
  }
  if (normal.x > maxDot) {
    plane = "sagittal";
    maxDot = normal.x;
  }

  if (spiral.second || maxDot < 0.999f) {
    return "Oblique " + plane;
  }
  plane[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(plane[0])));
  return plane;
}

template<typename T>
dicom::SeriesGeometry geometryFromImage(const typename itk::Image<T, 3>::Pointer& image)
{
  dicom::SeriesGeometry geometry;
  if (!image) {
    return geometry;
  }

  const auto region = image->GetLargestPossibleRegion();
  const auto size = region.GetSize();
  const auto spacing = image->GetSpacing();
  const auto origin = image->GetOrigin();
  const auto direction = image->GetDirection();

  geometry.dimensions = glm::uvec3{
    static_cast<unsigned int>(size[0]),
    static_cast<unsigned int>(size[1]),
    static_cast<unsigned int>(size[2])};
  geometry.spacing = glm::vec3{spacing[0], spacing[1], spacing[2]};
  geometry.origin = glm::vec3{origin[0], origin[1], origin[2]};
  geometry.directions = glm::mat3{
    direction[0][0],
    direction[0][1],
    direction[0][2],
    direction[1][0],
    direction[1][1],
    direction[1][2],
    direction[2][0],
    direction[2][1],
    direction[2][2]};
  geometry.sliceOrientation = sliceOrientationFromDirections(geometry.directions);
  return geometry;
}

std::vector<std::string> geometryWarnings(
  const dicom::SeriesGeometry& geometry,
  std::size_t fileCount,
  const dicom::SeriesTemporalInfo& temporal)
{
  std::vector<std::string> warnings;
  if (geometry.dimensions.x == 0 || geometry.dimensions.y == 0 || geometry.dimensions.z == 0) {
    warnings.emplace_back("Image dimensions are incomplete");
  }
  if (geometry.spacing.x <= 0.0f || geometry.spacing.y <= 0.0f || geometry.spacing.z <= 0.0f) {
    warnings.emplace_back("Image spacing is incomplete or non-positive");
  }
  if (!temporal.multiframe && fileCount != 0 && geometry.dimensions.z != 0 && geometry.dimensions.z != fileCount) {
    warnings.emplace_back("Slice count does not match file count; this may be a multi-frame or irregular series");
  }
  return warnings;
}

std::vector<std::string> sliceSpacingWarnings(const std::vector<fs::path>& files, const dicom::SeriesGeometry& geometry)
{
  std::vector<std::string> warnings;
  if (files.size() < 3 || geometry.spacing.z <= 0.0f) {
    return warnings;
  }

  std::vector<float> positions;
  positions.reserve(files.size());
  const glm::vec3 sliceNormal = glm::normalize(geometry.directions[2]);

  for (const auto& file : files) {
    try {
      auto imageIo = itk::GDCMImageIO::New();
      imageIo->SetFileName(file.string());
      imageIo->ReadImageInformation();
      const auto position = parseDicomVec3(metadataValue(imageIo->GetMetaDataDictionary(), "0020|0032"));
      if (!position) {
        return warnings;
      }
      positions.push_back(glm::dot(*position, sliceNormal));
    }
    catch (const std::exception& e) {
      spdlog::debug("Could not inspect DICOM slice position {}: {}", file, e.what());
      return warnings;
    }
  }

  std::sort(positions.begin(), positions.end());

  std::vector<float> gaps;
  gaps.reserve(positions.size() - 1);
  for (std::size_t i = 1; i < positions.size(); ++i) {
    gaps.push_back(std::abs(positions.at(i) - positions.at(i - 1)));
  }

  const float expectedGap = geometry.spacing.z;
  const float tolerance = std::max(0.01f, expectedGap * 0.20f);
  const auto largeGapIt =
    std::find_if(gaps.begin(), gaps.end(), [&](float gap) { return gap > expectedGap + tolerance; });

  if (largeGapIt != gaps.end()) {
    std::ostringstream message;
    message << "Irregular slice spacing detected; expected approximately " << expectedGap << " mm but found a "
            << *largeGapIt << " mm gap. One or more slices may be missing.";
    warnings.push_back(message.str());
  }

  return warnings;
}

template<typename T>
std::optional<dicom::SeriesGeometry> readSeriesGeometry(
  const std::vector<fs::path>& files,
  const itk::GDCMImageIO::Pointer& imageIo)
{
  using ImageType = itk::Image<T, 3>;
  using ReaderType = itk::ImageSeriesReader<ImageType>;

  if (files.empty()) {
    return std::nullopt;
  }

  std::vector<std::string> fileNames;
  fileNames.reserve(files.size());
  for (const auto& file : files) {
    fileNames.push_back(file.string());
  }

  try {
    auto reader = ReaderType::New();
    reader->SetImageIO(imageIo);
    reader->SetFileNames(fileNames);
    reader->UpdateOutputInformation();
    return geometryFromImage<T>(reader->GetOutput());
  }
  catch (const std::exception& e) {
    spdlog::warn("Could not read DICOM series geometry from {} files: {}", files.size(), e.what());
    return std::nullopt;
  }
}

std::vector<fs::path> canonicalInputRoots(const std::vector<fs::path>& inputPaths)
{
  std::vector<fs::path> roots;
  std::unordered_set<std::string> seen;

  for (const auto& inputPath : inputPaths) {
    if (inputPath.empty()) {
      continue;
    }

    std::error_code ec;
    fs::path root = inputPath;
    if (fs::is_regular_file(inputPath, ec)) {
      root = inputPath.parent_path();
    }
    if (root.empty()) {
      root = fs::current_path(ec);
    }
    root = fs::weakly_canonical(root, ec);
    if (ec) {
      root = inputPath;
    }

    const std::string key = root.string();
    if (seen.insert(key).second) {
      roots.push_back(root);
    }
  }

  return roots;
}

std::vector<fs::path> toPaths(const std::vector<std::string>& fileNames)
{
  std::vector<fs::path> paths;
  paths.reserve(fileNames.size());
  for (const auto& fileName : fileNames) {
    paths.emplace_back(fileName);
  }
  return paths;
}

std::string uidTail(const std::string& uid)
{
  const std::size_t lastSeparator = uid.rfind('.');
  if (lastSeparator == std::string::npos || lastSeparator == 0) {
    return uid;
  }

  const std::size_t previousSeparator = uid.rfind('.', lastSeparator - 1);
  if (previousSeparator == std::string::npos) {
    return uid.substr(lastSeparator + 1);
  }
  return uid.substr(previousSeparator + 1);
}

bool isImageLikeModality(const std::string& modality)
{
  static const std::set<std::string> unsupported{"RTSTRUCT", "RTPLAN", "RTDOSE", "SEG", "SR", "PR"};
  if (modality.empty()) {
    return true;
  }
  return unsupported.find(modality) == unsupported.end();
}

template<typename T>
ComponentType dicomComponentTypeFor()
{
  if constexpr (std::is_same_v<T, int8_t>) {
    return ComponentType::Int8;
  }
  else if constexpr (std::is_same_v<T, uint8_t>) {
    return ComponentType::UInt8;
  }
  else if constexpr (std::is_same_v<T, int16_t>) {
    return ComponentType::Int16;
  }
  else if constexpr (std::is_same_v<T, uint16_t>) {
    return ComponentType::UInt16;
  }
  else if constexpr (std::is_same_v<T, int32_t>) {
    return ComponentType::Int32;
  }
  else if constexpr (std::is_same_v<T, uint32_t>) {
    return ComponentType::UInt32;
  }
  else if constexpr (std::is_same_v<T, float>) {
    return ComponentType::Float32;
  }
  else {
    return ComponentType::Undefined;
  }
}

template<typename T>
ImageIoInfo dicomCineIoInfo(const typename itk::Image<T, 3>::Pointer& image, const fs::path& fileName)
{
  const auto region = image->GetLargestPossibleRegion();
  const auto size = region.GetSize();
  const auto spacing = image->GetSpacing();
  const auto origin = image->GetOrigin();
  const auto direction = image->GetDirection();
  const ComponentType componentType = dicomComponentTypeFor<T>();

  ImageIoInfo info;
  info.m_fileInfo.m_fileName = fileName;
  info.m_componentInfo.m_componentType = componentType;
  info.m_componentInfo.m_componentTypeString = componentTypeString(componentType);
  info.m_componentInfo.m_componentSizeInBytes = sizeof(T);
  info.m_pixelInfo.m_pixelType = PixelType::Scalar;
  info.m_pixelInfo.m_pixelTypeString = "scalar";
  info.m_pixelInfo.m_numComponents = 1u;
  info.m_pixelInfo.m_pixelStrideInBytes = sizeof(T);

  const std::size_t spatialPixels = static_cast<std::size_t>(size[0]) * static_cast<std::size_t>(size[1]);
  info.m_sizeInfo.m_imageSizeInPixels = spatialPixels;
  info.m_sizeInfo.m_imageSizeInComponents = spatialPixels;
  info.m_sizeInfo.m_imageSizeInBytes = spatialPixels * sizeof(T);

  info.m_spaceInfo.m_numDimensions = 3u;
  info.m_spaceInfo.m_dimensions = {size[0], size[1], 1u};
  info.m_spaceInfo.m_origin = {origin[0], origin[1], origin[2]};
  info.m_spaceInfo.m_spacing = {spacing[0], spacing[1], 1.0};
  info.m_spaceInfo.m_directions = {
    {direction[0][0], direction[1][0], direction[2][0]},
    {direction[0][1], direction[1][1], direction[2][1]},
    {direction[0][2], direction[1][2], direction[2][2]}};

  return info;
}

template<typename T>
std::optional<Image> loadScalarMultiframeCineImage(const dicom::SeriesInfo& series)
{
  using ImageType = itk::Image<T, 3>;
  using ReaderType = itk::ImageFileReader<ImageType>;

  if (series.files.size() != 1u) {
    return std::nullopt;
  }

  try {
    auto imageIo = itk::GDCMImageIO::New();
    auto reader = ReaderType::New();
    reader->SetImageIO(imageIo);
    reader->SetFileName(series.files.front().string());
    reader->Update();

    typename ImageType::Pointer itkImage = reader->GetOutput();
    if (!itkImage) {
      return std::nullopt;
    }

    const auto size = itkImage->GetLargestPossibleRegion().GetSize();
    const uint32_t numFrames = std::max<uint32_t>(1u, static_cast<uint32_t>(size[2]));
    ImageIoInfo info = dicomCineIoInfo<T>(itkImage, series.files.front());
    info.m_timeInfo.m_numTimePoints = numFrames;
    info.m_timeInfo.m_origin = 0.0;
    info.m_timeInfo.m_spacing = series.temporal.spacing;
    info.m_timeInfo.m_units = series.temporal.units;

    ImageHeader header(info, info, false);
    header.setFileName(series.files.front());
    header.setExistsOnDisk(true);

    Image image(
      header,
      series.displayName,
      Image::ImageRepresentation::Image,
      Image::MultiComponentBufferType::SeparateImages,
      std::vector<const void*>{itkImage->GetBufferPointer()},
      ImageTimeAxis(numFrames, 0.0, series.temporal.spacing, series.temporal.units));
    return image;
  }
  catch (const std::exception& e) {
    spdlog::error("Exception loading DICOM cine series {}: {}", series.seriesInstanceUid, e.what());
    return std::nullopt;
  }
}

template<typename T>
std::optional<Image> loadScalarSeriesImage(const dicom::SeriesInfo& series)
{
  if (series.temporal.multiframe && series.files.size() == 1u) {
    return loadScalarMultiframeCineImage<T>(series);
  }

  using ImageType = itk::Image<T, 3>;
  using ReaderType = itk::ImageSeriesReader<ImageType>;

  std::vector<std::string> fileNames;
  fileNames.reserve(series.files.size());
  for (const auto& file : series.files) {
    fileNames.push_back(file.string());
  }

  try {
    auto imageIo = itk::GDCMImageIO::New();
    auto reader = ReaderType::New();
    reader->SetImageIO(imageIo);
    reader->SetFileNames(fileNames);
    reader->Update();

    Image image = createImageFromItkImage<T>(reader->GetOutput(), series.displayName);
    if (!series.files.empty()) {
      image.header().setFileName(series.files.front());
      image.header().setExistsOnDisk(true);
    }
    return image;
  }
  catch (const std::exception& e) {
    spdlog::error("Exception loading DICOM series {}: {}", series.seriesInstanceUid, e.what());
    return std::nullopt;
  }
}

template<typename T>
std::optional<dicom::SlicePreview>
loadSlicePreviewImage(const fs::path& fileName, std::size_t sliceIndex, std::uint32_t maxDimension)
{
  using ImageType = itk::Image<T, 2>;
  using ReaderType = itk::ImageFileReader<ImageType>;

  if (fileName.empty() || maxDimension == 0) {
    return std::nullopt;
  }

  try {
    auto imageIo = itk::GDCMImageIO::New();
    auto reader = ReaderType::New();
    reader->SetImageIO(imageIo);
    reader->SetFileName(fileName.string());
    reader->Update();

    const auto image = reader->GetOutput();
    const auto region = image->GetLargestPossibleRegion();
    const auto size = region.GetSize();
    const std::uint32_t sourceWidth = static_cast<std::uint32_t>(size[0]);
    const std::uint32_t sourceHeight = static_cast<std::uint32_t>(size[1]);
    if (sourceWidth == 0 || sourceHeight == 0) {
      return std::nullopt;
    }

    double minValue = std::numeric_limits<double>::infinity();
    double maxValue = -std::numeric_limits<double>::infinity();
    itk::ImageRegionConstIterator<ImageType> it(image, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
      const double value = static_cast<double>(it.Get());
      if (!std::isfinite(value)) {
        continue;
      }
      minValue = std::min(minValue, value);
      maxValue = std::max(maxValue, value);
    }

    if (!std::isfinite(minValue) || !std::isfinite(maxValue)) {
      return std::nullopt;
    }

    const std::uint32_t largestSourceDimension = std::max(sourceWidth, sourceHeight);
    const double scale =
      largestSourceDimension > maxDimension ? static_cast<double>(maxDimension) / largestSourceDimension : 1.0;
    const std::uint32_t previewWidth =
      std::max<std::uint32_t>(1, static_cast<std::uint32_t>(std::round(sourceWidth * scale)));
    const std::uint32_t previewHeight =
      std::max<std::uint32_t>(1, static_cast<std::uint32_t>(std::round(sourceHeight * scale)));
    const double range = maxValue - minValue;

    dicom::SlicePreview preview;
    preview.width = previewWidth;
    preview.height = previewHeight;
    preview.sliceIndex = sliceIndex;
    preview.fileName = fileName;
    preview.pixels.resize(static_cast<std::size_t>(previewWidth) * previewHeight);

    for (std::uint32_t y = 0; y < previewHeight; ++y) {
      const auto sourceY = static_cast<typename ImageType::IndexType::IndexValueType>(std::min<std::uint32_t>(
        sourceHeight - 1,
        static_cast<std::uint32_t>((static_cast<std::uint64_t>(y) * sourceHeight) / previewHeight)));
      for (std::uint32_t x = 0; x < previewWidth; ++x) {
        const auto sourceX = static_cast<typename ImageType::IndexType::IndexValueType>(std::min<std::uint32_t>(
          sourceWidth - 1,
          static_cast<std::uint32_t>((static_cast<std::uint64_t>(x) * sourceWidth) / previewWidth)));
        const typename ImageType::IndexType index{{sourceX, sourceY}};
        const double value = static_cast<double>(image->GetPixel(index));
        double normalized = 0.5;
        if (range > 0.0 && std::isfinite(value)) {
          normalized = (value - minValue) / range;
        }
        normalized = std::clamp(normalized, 0.0, 1.0);
        preview.pixels.at(static_cast<std::size_t>(y) * previewWidth + x) =
          static_cast<std::uint8_t>(std::round(normalized * 255.0));
      }
    }

    return preview;
  }
  catch (const std::exception& e) {
    spdlog::warn("Could not load DICOM preview slice {}: {}", fileName, e.what());
    return std::nullopt;
  }
}

} // namespace

namespace dicom
{
bool SeriesTemporalInfo::isTimeSeries() const
{
  return numTimePoints > 1u;
}

bool SeriesInfo::loadable() const
{
  return !files.empty();
}

bool SlicePreview::empty() const
{
  return width == 0 || height == 0 || pixels.empty();
}

bool canReadDicomHeader(const fs::path& fileName)
{
  if (fileName.empty()) {
    return false;
  }

  std::error_code ec;
  if (!fs::is_regular_file(fileName, ec)) {
    return false;
  }

  try {
    auto imageIo = itk::GDCMImageIO::New();
    return imageIo && imageIo->CanReadFile(fileName.string().c_str());
  }
  catch (const std::exception&) {
    return false;
  }
}

std::string metadataTagName(const std::string& tag)
{
  const std::string normalized = normalizeTag(tag);
  const auto it = metadataTagNames().find(normalized);
  return it == metadataTagNames().end() ? std::string{"Unknown"} : it->second;
}

bool isPhiTag(const std::string& tag)
{
  const std::string normalized = normalizeTag(tag);
  return std::find(sk_phiTags.begin(), sk_phiTags.end(), normalized) != sk_phiTags.end();
}

bool isPrivateTag(const std::string& tag)
{
  const std::string normalized = normalizeTag(tag);
  const std::size_t separator = normalized.find('|');
  if (separator == std::string::npos || separator < 4) {
    return false;
  }

  unsigned int group = 0;
  std::stringstream ss;
  ss << std::hex << normalized.substr(0, separator);
  ss >> group;
  return (group % 2u) == 1u;
}

bool includeMetadataTag(const std::string& tag, bool includePrivateMetadata)
{
  if (isPhiTag(tag)) {
    return false;
  }
  if (!includePrivateMetadata && isPrivateTag(tag)) {
    return false;
  }
  return true;
}

std::vector<MetadataEntry> filteredMetadataEntries(
  const std::vector<RawMetadataEntry>& entries,
  bool includePrivateMetadata)
{
  std::vector<MetadataEntry> filtered;
  std::unordered_set<std::string> seen;

  for (const auto& entry : entries) {
    if (entry.value.empty()) {
      continue;
    }

    const std::string tag = normalizeTag(entry.tag);
    if (!includeMetadataTag(tag, includePrivateMetadata) || !seen.insert(tag).second) {
      continue;
    }

    filtered.push_back(MetadataEntry{tag, metadataTagName(tag), entry.value});
  }

  return filtered;
}

std::string displayNameForSeries(const SeriesMetadata& metadata, const std::string& seriesInstanceUid)
{
  if (!metadata.seriesDescription.empty()) {
    return metadata.seriesDescription;
  }
  if (!metadata.protocolName.empty()) {
    return metadata.protocolName;
  }

  std::string name;
  if (!metadata.modality.empty()) {
    name += metadata.modality;
  }
  if (!metadata.seriesNumber.empty()) {
    if (!name.empty()) {
      name += " ";
    }
    name += "Series " + metadata.seriesNumber;
  }
  if (!name.empty()) {
    return name;
  }
  return seriesInstanceUid.empty() ? "DICOM series" : "DICOM " + uidTail(seriesInstanceUid);
}

DiscoverResult discoverSeries(const std::vector<fs::path>& inputPaths, const DiscoverOptions& options)
{
  DiscoverResult result;
  const std::vector<fs::path> roots = canonicalInputRoots(inputPaths);

  for (const auto& root : roots) {
    std::error_code ec;
    if (!fs::exists(root, ec)) {
      result.warnings.push_back("DICOM input path does not exist: " + root.string());
      continue;
    }

    try {
      auto names = itk::GDCMSeriesFileNames::New();
      names->SetUseSeriesDetails(true);
      names->SetRecursive(options.recursive);
      names->SetInputDirectory(root.string());

      const auto seriesUids = names->GetSeriesUIDs();
      for (const auto& seriesUid : seriesUids) {
        const auto rawFiles = names->GetFileNames(seriesUid);
        SeriesInfo info;
        info.rootPath = root;
        info.seriesInstanceUid = seriesUid;
        info.files = toPaths(rawFiles);

        if (info.files.empty()) {
          info.warnings.emplace_back("Series contains no files");
          result.series.push_back(std::move(info));
          continue;
        }

        auto imageIo = itk::GDCMImageIO::New();
        imageIo->SetFileName(info.files.front().string());
        imageIo->ReadImageInformation();
        const auto& dict = imageIo->GetMetaDataDictionary();
        info.metadata = readSeriesMetadata(dict);
        if (info.metadata.seriesInstanceUid.empty()) {
          info.metadata.seriesInstanceUid = seriesUid;
        }
        info.temporal = readTemporalInfo(dict);
        info.metadataSummary = filteredMetadataEntries(rawMetadataEntries(dict), options.includePrivateMetadata);
        info.displayName = displayNameForSeries(info.metadata, seriesUid);

        if (!isImageLikeModality(info.metadata.modality)) {
          info.warnings.emplace_back("Series modality is not an image volume: " + info.metadata.modality);
        }

        const std::optional<SeriesGeometry> geometry = readSeriesGeometry<float>(info.files, imageIo);
        if (geometry) {
          info.geometry = *geometry;
          auto warnings = geometryWarnings(info.geometry, info.files.size(), info.temporal);
          info.warnings.insert(info.warnings.end(), warnings.begin(), warnings.end());
          if (!info.temporal.multiframe) {
            auto spacingWarnings = sliceSpacingWarnings(info.files, info.geometry);
            info.warnings.insert(info.warnings.end(), spacingWarnings.begin(), spacingWarnings.end());
          }
        }
        else {
          info.warnings.emplace_back("Could not read series geometry");
        }

        result.series.push_back(std::move(info));
      }
    }
    catch (const std::exception& e) {
      result.warnings.push_back("Could not discover DICOM series in " + root.string() + ": " + e.what());
    }
  }

  std::sort(result.series.begin(), result.series.end(), [](const SeriesInfo& a, const SeriesInfo& b) {
    if (a.metadata.seriesNumber != b.metadata.seriesNumber) {
      return a.metadata.seriesNumber < b.metadata.seriesNumber;
    }
    return a.displayName < b.displayName;
  });

  return result;
}

std::optional<Image> loadSeriesImage(
  const SeriesInfo& series,
  Image::ImageRepresentation imageRep,
  Image::MultiComponentBufferType bufferType)
{
  if (imageRep != Image::ImageRepresentation::Image || bufferType != Image::MultiComponentBufferType::SeparateImages) {
    spdlog::error("DICOM series loading currently supports scalar image volumes only");
    return std::nullopt;
  }
  if (series.files.empty()) {
    spdlog::error("Cannot load DICOM series {} because it has no files", series.seriesInstanceUid);
    return std::nullopt;
  }

  try {
    auto imageIo = itk::GDCMImageIO::New();
    imageIo->SetFileName(series.files.front().string());
    imageIo->ReadImageInformation();

    switch (fromItkComponentType(imageIo->GetComponentType())) {
      case ComponentType::Int8:
        return loadScalarSeriesImage<int8_t>(series);
      case ComponentType::UInt8:
        return loadScalarSeriesImage<uint8_t>(series);
      case ComponentType::Int16:
        return loadScalarSeriesImage<int16_t>(series);
      case ComponentType::UInt16:
        return loadScalarSeriesImage<uint16_t>(series);
      case ComponentType::Int32:
        return loadScalarSeriesImage<int32_t>(series);
      case ComponentType::UInt32:
        return loadScalarSeriesImage<uint32_t>(series);
      case ComponentType::Float32:
      case ComponentType::Float64:
      case ComponentType::LongDouble:
        return loadScalarSeriesImage<float>(series);
      case ComponentType::Long:
      case ComponentType::ULong:
      case ComponentType::LongLong:
      case ComponentType::ULongLong:
      case ComponentType::Undefined:
        spdlog::warn(
          "DICOM series {} has unsupported component type {}; loading as signed 32-bit integer",
          series.seriesInstanceUid,
          componentTypeString(fromItkComponentType(imageIo->GetComponentType())));
        return loadScalarSeriesImage<int32_t>(series);
    }

    return std::nullopt;
  }
  catch (const std::exception& e) {
    spdlog::error("Could not inspect DICOM series {} before load: {}", series.seriesInstanceUid, e.what());
    return std::nullopt;
  }
}

std::vector<SlicePreview> loadSlicePreviews(const SeriesInfo& series, std::uint32_t maxDimension, std::size_t maxSlices)
{
  std::vector<SlicePreview> previews;
  if (series.files.empty() || maxSlices == 0) {
    if (series.files.empty()) {
      spdlog::warn("Cannot load DICOM preview for series {} because it has no files", series.seriesInstanceUid);
    }
    return previews;
  }

  const std::size_t sliceCount = std::min(maxSlices, series.files.size());
  previews.reserve(sliceCount);

  for (std::size_t n = 0; n < sliceCount; ++n) {
    const std::size_t sliceIndex =
      sliceCount == 1 ? series.files.size() / 2 : (n * (series.files.size() - 1)) / (sliceCount - 1);
    const fs::path& fileName = series.files.at(sliceIndex);

    try {
      auto imageIo = itk::GDCMImageIO::New();
      imageIo->SetFileName(fileName.string());
      imageIo->ReadImageInformation();

      std::optional<SlicePreview> preview;
      switch (fromItkComponentType(imageIo->GetComponentType())) {
        case ComponentType::Int8:
          preview = loadSlicePreviewImage<int8_t>(fileName, sliceIndex, maxDimension);
          break;
        case ComponentType::UInt8:
          preview = loadSlicePreviewImage<uint8_t>(fileName, sliceIndex, maxDimension);
          break;
        case ComponentType::Int16:
          preview = loadSlicePreviewImage<int16_t>(fileName, sliceIndex, maxDimension);
          break;
        case ComponentType::UInt16:
          preview = loadSlicePreviewImage<uint16_t>(fileName, sliceIndex, maxDimension);
          break;
        case ComponentType::Int32:
        case ComponentType::Long:
        case ComponentType::LongLong:
          preview = loadSlicePreviewImage<int32_t>(fileName, sliceIndex, maxDimension);
          break;
        case ComponentType::UInt32:
        case ComponentType::ULong:
        case ComponentType::ULongLong:
          preview = loadSlicePreviewImage<uint32_t>(fileName, sliceIndex, maxDimension);
          break;
        case ComponentType::Float32:
        case ComponentType::Float64:
        case ComponentType::LongDouble:
          preview = loadSlicePreviewImage<float>(fileName, sliceIndex, maxDimension);
          break;
        case ComponentType::Undefined:
          spdlog::warn(
            "Cannot load DICOM preview for series {} with undefined component type",
            series.seriesInstanceUid);
          break;
      }
      if (preview) {
        previews.push_back(std::move(*preview));
      }
    }
    catch (const std::exception& e) {
      spdlog::warn("Could not inspect DICOM preview slice {}: {}", fileName, e.what());
    }
  }

  return previews;
}

std::optional<SlicePreview> loadMiddleSlicePreview(const SeriesInfo& series, std::uint32_t maxDimension)
{
  if (series.files.empty()) {
    spdlog::warn("Cannot load DICOM preview for series {} because it has no files", series.seriesInstanceUid);
    return std::nullopt;
  }

  const std::size_t sliceIndex = series.files.size() / 2;
  const fs::path& fileName = series.files.at(sliceIndex);

  try {
    auto imageIo = itk::GDCMImageIO::New();
    imageIo->SetFileName(fileName.string());
    imageIo->ReadImageInformation();

    switch (fromItkComponentType(imageIo->GetComponentType())) {
      case ComponentType::Int8:
        return loadSlicePreviewImage<int8_t>(fileName, sliceIndex, maxDimension);
      case ComponentType::UInt8:
        return loadSlicePreviewImage<uint8_t>(fileName, sliceIndex, maxDimension);
      case ComponentType::Int16:
        return loadSlicePreviewImage<int16_t>(fileName, sliceIndex, maxDimension);
      case ComponentType::UInt16:
        return loadSlicePreviewImage<uint16_t>(fileName, sliceIndex, maxDimension);
      case ComponentType::Int32:
      case ComponentType::Long:
      case ComponentType::LongLong:
        return loadSlicePreviewImage<int32_t>(fileName, sliceIndex, maxDimension);
      case ComponentType::UInt32:
      case ComponentType::ULong:
      case ComponentType::ULongLong:
        return loadSlicePreviewImage<uint32_t>(fileName, sliceIndex, maxDimension);
      case ComponentType::Float32:
      case ComponentType::Float64:
      case ComponentType::LongDouble:
        return loadSlicePreviewImage<float>(fileName, sliceIndex, maxDimension);
      case ComponentType::Undefined:
        spdlog::warn("Cannot load DICOM preview for series {} with undefined component type", series.seriesInstanceUid);
        return std::nullopt;
    }
  }
  catch (const std::exception& e) {
    spdlog::warn("Could not inspect DICOM preview slice {}: {}", fileName, e.what());
  }

  return std::nullopt;
}

} // namespace dicom
