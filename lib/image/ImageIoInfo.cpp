#include "image/ImageIoInfo.h"
#include "internal/ImageUtilityItk.h"

#include "common/Exception.hpp"

#include <spdlog/spdlog.h>

#include <itkIOCommon.h> // for itk::SpatialOrientation
#include <itkMetaDataDictionary.h>
#include <itkMetaDataObject.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <numeric>
#include <optional>
#include <sstream>

namespace
{
namespace so = ::itk::SpatialOrientation;

std::unordered_map<so::ValidCoordinateOrientationFlags, std::string> spiralCodeMap = {
  {so::ITK_COORDINATE_ORIENTATION_RIP, "RIP"}, {so::ITK_COORDINATE_ORIENTATION_LIP, "LIP"},
  {so::ITK_COORDINATE_ORIENTATION_RSP, "RSP"}, {so::ITK_COORDINATE_ORIENTATION_LSP, "LSP"},
  {so::ITK_COORDINATE_ORIENTATION_RIA, "RIA"}, {so::ITK_COORDINATE_ORIENTATION_LIA, "LIA"},
  {so::ITK_COORDINATE_ORIENTATION_RSA, "RSA"}, {so::ITK_COORDINATE_ORIENTATION_LSA, "LSA"},
  {so::ITK_COORDINATE_ORIENTATION_IRP, "IRP"}, {so::ITK_COORDINATE_ORIENTATION_ILP, "ILP"},
  {so::ITK_COORDINATE_ORIENTATION_SRP, "SRP"}, {so::ITK_COORDINATE_ORIENTATION_SLP, "SLP"},
  {so::ITK_COORDINATE_ORIENTATION_IRA, "IRA"}, {so::ITK_COORDINATE_ORIENTATION_ILA, "ILA"},
  {so::ITK_COORDINATE_ORIENTATION_SRA, "SRA"}, {so::ITK_COORDINATE_ORIENTATION_SLA, "SLA"},
  {so::ITK_COORDINATE_ORIENTATION_RPI, "RPI"}, {so::ITK_COORDINATE_ORIENTATION_LPI, "LPI"},
  {so::ITK_COORDINATE_ORIENTATION_RAI, "RAI"}, {so::ITK_COORDINATE_ORIENTATION_LAI, "LAI"},
  {so::ITK_COORDINATE_ORIENTATION_RPS, "RPS"}, {so::ITK_COORDINATE_ORIENTATION_LPS, "LPS"},
  {so::ITK_COORDINATE_ORIENTATION_RAS, "RAS"}, {so::ITK_COORDINATE_ORIENTATION_LAS, "LAS"},
  {so::ITK_COORDINATE_ORIENTATION_PRI, "PRI"}, {so::ITK_COORDINATE_ORIENTATION_PLI, "PLI"},
  {so::ITK_COORDINATE_ORIENTATION_ARI, "ARI"}, {so::ITK_COORDINATE_ORIENTATION_ALI, "ALI"},
  {so::ITK_COORDINATE_ORIENTATION_PRS, "PRS"}, {so::ITK_COORDINATE_ORIENTATION_PLS, "PLS"},
  {so::ITK_COORDINATE_ORIENTATION_ARS, "ARS"}, {so::ITK_COORDINATE_ORIENTATION_ALS, "ALS"},
  {so::ITK_COORDINATE_ORIENTATION_IPR, "IPR"}, {so::ITK_COORDINATE_ORIENTATION_SPR, "SPR"},
  {so::ITK_COORDINATE_ORIENTATION_IAR, "IAR"}, {so::ITK_COORDINATE_ORIENTATION_SAR, "SAR"},
  {so::ITK_COORDINATE_ORIENTATION_IPL, "IPL"}, {so::ITK_COORDINATE_ORIENTATION_SPL, "SPL"},
  {so::ITK_COORDINATE_ORIENTATION_IAL, "IAL"}, {so::ITK_COORDINATE_ORIENTATION_SAL, "SAL"},
  {so::ITK_COORDINATE_ORIENTATION_PIR, "PIR"}, {so::ITK_COORDINATE_ORIENTATION_PSR, "PSR"},
  {so::ITK_COORDINATE_ORIENTATION_AIR, "AIR"}, {so::ITK_COORDINATE_ORIENTATION_ASR, "ASR"},
  {so::ITK_COORDINATE_ORIENTATION_PIL, "PIL"}, {so::ITK_COORDINATE_ORIENTATION_PSL, "PSL"},
  {so::ITK_COORDINATE_ORIENTATION_AIL, "AIL"}, {so::ITK_COORDINATE_ORIENTATION_ASL, "ASL"}};

template<class T>
bool setMetaDataEntry(MetaDataMap& metaDataMap, const itk::MetaDataDictionary& dictionary, const std::string& key)
{
  T value;

  if (itk::ExposeMetaData<T>(dictionary, key, value)) {
    metaDataMap[key] = value;
    return true;
  }

  return false;
}

template<class T>
bool setArrayMetaDataEntry(MetaDataMap& metaDataMap, const itk::MetaDataDictionary& dictionary, const std::string& key)
{
  itk::Array<T> value;

  if (itk::ExposeMetaData<itk::Array<T>>(dictionary, key, value)) {
    metaDataMap[key] = std::vector<T>(value.begin(), value.end());
    return true;
  }

  return false;
}

template<class T>
bool setMatrixMetaDataEntry(MetaDataMap& metaDataMap, const itk::MetaDataDictionary& dictionary, const std::string& key)
{
  itk::Matrix<T, 4, 4> value;

  if (itk::ExposeMetaData<itk::Matrix<T, 4, 4>>(dictionary, key, value)) {
    std::vector<std::vector<T>> rows(4, std::vector<T>(4));
    for (unsigned int r = 0; r < 4; ++r) {
      for (unsigned int c = 0; c < 4; ++c) {
        rows[r][c] = value(r, c);
      }
    }
    metaDataMap[key] = std::move(rows);
    return true;
  }

  return false;
}

MetaDataMap getMetaDataMap(const ::itk::ImageIOBase::Pointer imageIo)
{
  MetaDataMap metaDataMap;

  if (imageIo.IsNull()) {
    return metaDataMap;
  }

  const itk::MetaDataDictionary& dictionary = imageIo->GetMetaDataDictionary();

  for (auto itr = dictionary.Begin(); itr != dictionary.End(); ++itr) {
    const std::string key = itr->first;
    std::string value;

    auto orientFlagsValue =
      itk::SpatialOrientationEnums::ValidCoordinateOrientations::ITK_COORDINATE_ORIENTATION_INVALID;

    if (itk::ExposeMetaData<std::string>(dictionary, key, value)) {
      std::ostringstream sout("");
      for (std::size_t i = 0; i < value.length(); ++i) {
        if (value[i] >= ' ') {
          sout << value[i];
        }
      }

      metaDataMap[key] = sout.str();
    }
    else if (itk::ExposeMetaData(dictionary, key, orientFlagsValue)) {
      metaDataMap[key] = spiralCodeMap[orientFlagsValue];
    }
    else {
      if (setMetaDataEntry<bool>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMetaDataEntry<unsigned char>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMetaDataEntry<char>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMetaDataEntry<signed char>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMetaDataEntry<unsigned short>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMetaDataEntry<short>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMetaDataEntry<unsigned int>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMetaDataEntry<int>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMetaDataEntry<unsigned long>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMetaDataEntry<long>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMetaDataEntry<unsigned long long>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMetaDataEntry<long long>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMetaDataEntry<float>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMetaDataEntry<double>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMetaDataEntry<std::vector<float>>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMetaDataEntry<std::vector<double>>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMetaDataEntry<std::vector<std::vector<float>>>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMetaDataEntry<std::vector<std::vector<double>>>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setArrayMetaDataEntry<char>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setArrayMetaDataEntry<int>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setArrayMetaDataEntry<float>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setArrayMetaDataEntry<double>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMatrixMetaDataEntry<float>(metaDataMap, dictionary, key)) {
        continue;
      }
      if (setMatrixMetaDataEntry<double>(metaDataMap, dictionary, key)) {
        continue;
      }

      spdlog::info("Key {} is of unsupported type {}", key, itr->second->GetMetaDataObjectTypeName());
    }
  }

  return metaDataMap;
}

bool setFileInfoFromItk(FileInfo& info, const itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || imageIo.IsNull()) {
    return false;
  }

  info.m_fileName = imageIo->GetFileName();
  info.m_byteOrderString = imageIo->GetByteOrderAsString(imageIo->GetByteOrder());
  info.m_useCompression = imageIo->GetUseCompression();
  info.m_fileTypeString = imageIo->GetFileTypeAsString(imageIo->GetFileType());
  info.m_supportedReadExtensions = imageIo->GetSupportedReadExtensions();
  info.m_supportedWriteExtensions = imageIo->GetSupportedWriteExtensions();

  return true;
}

bool setComponentInfoFromItk(ComponentInfo& info, const itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || imageIo.IsNull()) {
    return false;
  }

  const itk::IOComponentEnum componentType = imageIo->GetComponentType();
  info.m_componentType = fromItkComponentType(componentType);
  info.m_componentTypeString = itk::ImageIOBase::GetComponentTypeAsString(componentType);
  info.m_componentSizeInBytes = static_cast<uint32_t>(imageIo->GetComponentSize());
  return true;
}

bool setPixelInfoFromItk(PixelInfo& info, const itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || imageIo.IsNull()) {
    return false;
  }

  const itk::IOPixelEnum pixelType = imageIo->GetPixelType();
  info.m_pixelType = fromItkPixelType(pixelType);
  info.m_pixelTypeString = itk::ImageIOBase::GetPixelTypeAsString(pixelType);
  info.m_numComponents = static_cast<uint32_t>(imageIo->GetNumberOfComponents());
  info.m_pixelStrideInBytes = static_cast<uint32_t>(imageIo->GetPixelStride());
  if (info.m_pixelStrideInBytes == 0u) {
    info.m_pixelStrideInBytes = info.m_numComponents * static_cast<uint32_t>(imageIo->GetComponentSize());
  }
  return true;
}

bool setSizeInfoFromItk(SizeInfo& info, const itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || imageIo.IsNull()) {
    return false;
  }

  info.m_imageSizeInComponents = static_cast<std::size_t>(imageIo->GetImageSizeInComponents());
  info.m_imageSizeInPixels = static_cast<std::size_t>(imageIo->GetImageSizeInPixels());
  info.m_imageSizeInBytes = static_cast<std::size_t>(imageIo->GetImageSizeInBytes());
  return true;
}

bool setSpaceInfoFromItk(SpaceInfo& info, const itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || imageIo.IsNull()) {
    return false;
  }

  info.m_numDimensions = static_cast<uint32_t>(imageIo->GetNumberOfDimensions());

  if (5 < info.m_numDimensions) {
    spdlog::error("Image dimension ({}) greater than 5 is not supported", info.m_numDimensions);
    return false;
  }

  info.m_dimensions.resize(info.m_numDimensions);
  info.m_origin.resize(info.m_numDimensions);
  info.m_spacing.resize(info.m_numDimensions);
  info.m_directions.assign(info.m_numDimensions, std::vector<double>(info.m_numDimensions));

  for (uint32_t i = 0; i < info.m_numDimensions; ++i) {
    info.m_dimensions[i] = static_cast<uint64_t>(imageIo->GetDimensions(i));
    info.m_origin[i] = imageIo->GetOrigin(i);
    info.m_spacing[i] = imageIo->GetSpacing(i);
    info.m_directions[i] = imageIo->GetDirection(i);
  }

  return true;
}

bool setTimeInfoFromItk(TimeInfo& info, const itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || imageIo.IsNull()) {
    return false;
  }

  const auto numDimensions = static_cast<uint32_t>(imageIo->GetNumberOfDimensions());
  if (numDimensions < 4u) {
    info = TimeInfo{};
    return true;
  }

  info.m_numTimePoints = static_cast<uint32_t>(imageIo->GetDimensions(3));
  info.m_origin = imageIo->GetOrigin(3);
  info.m_spacing = imageIo->GetSpacing(3);
  info.m_units = "frame";
  return true;
}

std::string trim(std::string text)
{
  const auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); });
  const auto last =
    std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) { return std::isspace(c); }).base();
  return first < last ? std::string(first, last) : std::string{};
}

std::vector<std::string> splitWords(const std::string& text)
{
  std::istringstream stream(text);
  std::vector<std::string> words;
  std::string word;
  while (stream >> word) {
    words.push_back(word);
  }
  return words;
}

std::optional<std::pair<std::string, std::string>> parseHeaderKeyValue(const std::string& line)
{
  const auto separator = line.find_first_of(":=");
  if (separator == std::string::npos) {
    return std::nullopt;
  }

  std::string key = trim(line.substr(0, separator));
  std::string value = trim(line.substr(separator + 1));
  if (!value.empty() && value.front() == '=') {
    value = trim(value.substr(1));
  }

  if (key.empty()) {
    return std::nullopt;
  }

  return std::make_pair(std::move(key), std::move(value));
}

struct NrrdAxisMetadata
{
  bool hasVectorAxis = false;
  bool hasTimeAxis = false;
  std::string timeUnits = "frame";
};

struct EntropyTimeMetadata
{
  bool hasTimeAxis = false;
  std::string timeUnits = "frame";
};

EntropyTimeMetadata readEntropyTimeMetadata(const MetaDataMap& metaData)
{
  EntropyTimeMetadata metadata;

  if (const auto it = metaData.find("entropy_time_axis"); it != metaData.end()) {
    if (const auto timeAxis = std::get_if<std::string>(&it->second)) {
      metadata.hasTimeAxis = *timeAxis == "last";
    }
  }
  if (const auto it = metaData.find("entropy_time_units"); it != metaData.end()) {
    if (const auto timeUnits = std::get_if<std::string>(&it->second); timeUnits && !timeUnits->empty()) {
      metadata.timeUnits = *timeUnits;
    }
  }

  return metadata;
}

EntropyTimeMetadata readMetaImageTimeMetadata(const std::filesystem::path& fileName)
{
  std::string extension = fileName.extension().string();
  std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (extension != ".mha" && extension != ".mhd") {
    return {};
  }

  std::ifstream in(fileName, std::ios::binary);
  if (!in) {
    return {};
  }

  EntropyTimeMetadata metadata;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    const auto parsed = parseHeaderKeyValue(line);
    if (!parsed) {
      continue;
    }

    const auto& [key, value] = *parsed;
    if (key == "entropy_time_axis") {
      metadata.hasTimeAxis = value == "last";
    }
    else if (key == "entropy_time_units" && !value.empty()) {
      metadata.timeUnits = value;
    }
    else if (key == "ElementDataFile") {
      break;
    }
  }

  return metadata;
}

NrrdAxisMetadata readNrrdAxisMetadata(const std::filesystem::path& fileName)
{
  std::string extension = fileName.extension().string();
  std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (extension != ".nrrd" && extension != ".nhdr") {
    return {};
  }

  std::ifstream in(fileName, std::ios::binary);
  if (!in) {
    return {};
  }

  NrrdAxisMetadata metadata;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      break;
    }
    if (line.starts_with('#')) {
      continue;
    }

    const auto parsed = parseHeaderKeyValue(line);
    if (!parsed) {
      continue;
    }

    const auto& [key, value] = *parsed;
    if (key == "kinds") {
      const std::vector<std::string> kinds = splitWords(value);
      metadata.hasVectorAxis = !kinds.empty() && kinds.front() == "vector";
    }
    else if (key == "entropy_time_axis") {
      metadata.hasTimeAxis = value == "last";
    }
    else if (key == "entropy_time_units" && !value.empty()) {
      metadata.timeUnits = value;
    }
  }

  return metadata;
}

void updateSizeInfo(ImageIoInfo& info)
{
  const std::size_t imageSizeInPixels = std::accumulate(
    info.m_spaceInfo.m_dimensions.begin(),
    info.m_spaceInfo.m_dimensions.end(),
    std::size_t{1},
    std::multiplies<>());
  info.m_sizeInfo.m_imageSizeInPixels = imageSizeInPixels;
  info.m_sizeInfo.m_imageSizeInComponents = imageSizeInPixels * info.m_pixelInfo.m_numComponents;
  info.m_sizeInfo.m_imageSizeInBytes =
    info.m_sizeInfo.m_imageSizeInComponents * info.m_componentInfo.m_componentSizeInBytes;
}

void normalizeNrrdAxes(ImageIoInfo& info, const std::filesystem::path& fileName)
{
  const NrrdAxisMetadata metadata = readNrrdAxisMetadata(fileName);
  const bool normalizeVectorAxis = metadata.hasVectorAxis && info.m_pixelInfo.m_numComponents <= 1u;
  if (!normalizeVectorAxis && !metadata.hasTimeAxis) {
    return;
  }

  const uint32_t originalDimensions = info.m_spaceInfo.m_numDimensions;
  if (originalDimensions == 0u) {
    return;
  }

  const std::optional<uint32_t> vectorAxis = normalizeVectorAxis ? std::optional<uint32_t>{0u} : std::nullopt;
  const std::optional<uint32_t> timeAxis = metadata.hasTimeAxis && originalDimensions > (vectorAxis ? 2u : 1u)
                                             ? std::optional<uint32_t>{originalDimensions - 1u}
                                             : std::nullopt;

  std::vector<uint32_t> spatialAxes;
  spatialAxes.reserve(originalDimensions);
  for (uint32_t axis = 0; axis < originalDimensions; ++axis) {
    if ((vectorAxis && axis == *vectorAxis) || (timeAxis && axis == *timeAxis)) {
      continue;
    }
    spatialAxes.push_back(axis);
  }

  if (spatialAxes.empty() || spatialAxes.size() > 3u) {
    return;
  }

  if (vectorAxis) {
    info.m_pixelInfo.m_pixelType = PixelType::VariableLengthVector;
    info.m_pixelInfo.m_pixelTypeString = "variable length vector";
    info.m_pixelInfo.m_numComponents = static_cast<uint32_t>(info.m_spaceInfo.m_dimensions.at(*vectorAxis));
    info.m_pixelInfo.m_pixelStrideInBytes =
      info.m_pixelInfo.m_numComponents * info.m_componentInfo.m_componentSizeInBytes;
  }

  if (timeAxis) {
    info.m_timeInfo.m_numTimePoints = static_cast<uint32_t>(info.m_spaceInfo.m_dimensions.at(*timeAxis));
    info.m_timeInfo.m_origin = info.m_spaceInfo.m_origin.at(*timeAxis);
    info.m_timeInfo.m_spacing = info.m_spaceInfo.m_spacing.at(*timeAxis);
    info.m_timeInfo.m_units = metadata.timeUnits;
  }
  else {
    info.m_timeInfo = TimeInfo{};
  }

  std::vector<uint32_t> logicalAxes = spatialAxes;
  if (timeAxis) {
    logicalAxes.push_back(*timeAxis);
  }

  SpaceInfo normalizedSpace;
  normalizedSpace.m_numDimensions = static_cast<uint32_t>(logicalAxes.size());
  normalizedSpace.m_dimensions.reserve(logicalAxes.size());
  normalizedSpace.m_origin.reserve(logicalAxes.size());
  normalizedSpace.m_spacing.reserve(logicalAxes.size());
  normalizedSpace.m_directions.assign(logicalAxes.size(), std::vector<double>(logicalAxes.size(), 0.0));

  for (const uint32_t axis : logicalAxes) {
    normalizedSpace.m_dimensions.push_back(info.m_spaceInfo.m_dimensions.at(axis));
    normalizedSpace.m_origin.push_back(info.m_spaceInfo.m_origin.at(axis));
    normalizedSpace.m_spacing.push_back(info.m_spaceInfo.m_spacing.at(axis));
  }

  for (std::size_t row = 0; row < logicalAxes.size(); ++row) {
    const uint32_t sourceRow = logicalAxes[row];
    if (sourceRow >= info.m_spaceInfo.m_directions.size()) {
      continue;
    }
    for (std::size_t col = 0; col < logicalAxes.size(); ++col) {
      const uint32_t sourceCol = logicalAxes[col];
      if (sourceCol < info.m_spaceInfo.m_directions[sourceRow].size()) {
        normalizedSpace.m_directions[row][col] = info.m_spaceInfo.m_directions[sourceRow][sourceCol];
      }
    }
  }

  info.m_spaceInfo = std::move(normalizedSpace);
  updateSizeInfo(info);
}

void normalizeCanonicalTimeAxis(ImageIoInfo& info, const EntropyTimeMetadata& metadata)
{
  if (
    info.m_spaceInfo.m_numDimensions < 4u || info.m_spaceInfo.m_dimensions.size() < 4u ||
    info.m_spaceInfo.m_origin.size() < 4u || info.m_spaceInfo.m_spacing.size() < 4u)
  {
    return;
  }

  if (metadata.hasTimeAxis && info.m_timeInfo.m_numTimePoints > 1u) {
    info.m_timeInfo.m_units = metadata.timeUnits;
    return;
  }

  if (info.m_timeInfo.m_numTimePoints <= 1u && info.m_spaceInfo.m_dimensions[3] > 1u) {
    info.m_timeInfo.m_numTimePoints = static_cast<uint32_t>(info.m_spaceInfo.m_dimensions[3]);
    info.m_timeInfo.m_origin = info.m_spaceInfo.m_origin[3];
    info.m_timeInfo.m_spacing = info.m_spaceInfo.m_spacing[3];
    info.m_timeInfo.m_units = metadata.hasTimeAxis ? metadata.timeUnits : "frame";
  }
}
} // anonymous namespace

bool FileInfo::validate() const
{
  return !m_fileName.empty();
}

bool ComponentInfo::validate() const
{
  return ComponentType::Undefined != m_componentType && 0u < m_componentSizeInBytes;
}

bool PixelInfo::validate() const
{
  return PixelType::Undefined != m_pixelType && 0u < m_numComponents && 0u < m_pixelStrideInBytes;
}

bool SizeInfo::validate() const
{
  return 0u < m_imageSizeInPixels && 0u < m_imageSizeInComponents && 0u < m_imageSizeInBytes;
}

bool SpaceInfo::validate() const
{
  const bool topLevelValid = 0u < m_numDimensions && m_numDimensions <= 5u && m_dimensions.size() == m_numDimensions &&
                             m_origin.size() == m_numDimensions && m_spacing.size() == m_numDimensions &&
                             m_directions.size() == m_numDimensions;

  if (!topLevelValid) {
    return false;
  }

  return std::all_of(m_directions.begin(), m_directions.end(), [this](const auto& row) {
    return row.size() == m_numDimensions;
  });
}

bool TimeInfo::validate() const
{
  return 0u < m_numTimePoints && !m_units.empty();
}

bool ImageIoInfo::validate() const
{
  return (
    m_fileInfo.validate() && m_componentInfo.validate() && m_pixelInfo.validate() && m_sizeInfo.validate() &&
    m_spaceInfo.validate() && m_timeInfo.validate());
}

bool setImageIoInfoFromItk(ImageIoInfo& info, const itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || imageIo.IsNull()) {
    return false;
  }

  info.m_metaData = getMetaDataMap(imageIo);

  return (
    setFileInfoFromItk(info.m_fileInfo, imageIo) && setComponentInfoFromItk(info.m_componentInfo, imageIo) &&
    setPixelInfoFromItk(info.m_pixelInfo, imageIo) && setSizeInfoFromItk(info.m_sizeInfo, imageIo) &&
    setSpaceInfoFromItk(info.m_spaceInfo, imageIo) && setTimeInfoFromItk(info.m_timeInfo, imageIo));
}

void normalizeImageIoAxesForEntropy(ImageIoInfo& info, const std::filesystem::path& fileName)
{
  EntropyTimeMetadata metadata = readEntropyTimeMetadata(info.m_metaData);
  if (!metadata.hasTimeAxis) {
    metadata = readMetaImageTimeMetadata(fileName);
  }
  normalizeNrrdAxes(info, fileName);
  normalizeCanonicalTimeAxis(info, metadata);
}

ImageIoInfo spatializedImageIoInfoForEntropy(const ImageIoInfo& source)
{
  ImageIoInfo info = source;
  if (source.m_timeInfo.m_numTimePoints <= 1u && source.m_spaceInfo.m_numDimensions <= 3u) {
    return info;
  }

  const bool hasTimeAxis = source.m_timeInfo.m_numTimePoints > 1u && source.m_spaceInfo.m_numDimensions >= 2u;
  const uint32_t spatialDimensions = hasTimeAxis ? source.m_spaceInfo.m_numDimensions - 1u : 3u;

  info.m_spaceInfo.m_numDimensions = spatialDimensions;
  info.m_spaceInfo.m_dimensions.assign(
    source.m_spaceInfo.m_dimensions.begin(),
    source.m_spaceInfo.m_dimensions.begin() + spatialDimensions);
  info.m_spaceInfo.m_origin.assign(
    source.m_spaceInfo.m_origin.begin(),
    source.m_spaceInfo.m_origin.begin() + spatialDimensions);
  info.m_spaceInfo.m_spacing.assign(
    source.m_spaceInfo.m_spacing.begin(),
    source.m_spaceInfo.m_spacing.begin() + spatialDimensions);

  info.m_spaceInfo.m_directions.assign(spatialDimensions, std::vector<double>(spatialDimensions, 0.0));
  for (uint32_t row = 0; row < spatialDimensions; ++row) {
    for (uint32_t col = 0; col < spatialDimensions; ++col) {
      info.m_spaceInfo.m_directions[row][col] = source.m_spaceInfo.m_directions[row][col];
    }
  }

  const std::size_t spatialPixels = std::accumulate(
    info.m_spaceInfo.m_dimensions.begin(),
    info.m_spaceInfo.m_dimensions.end(),
    std::size_t{1},
    std::multiplies<>());
  const uint32_t numTimePoints = hasTimeAxis ? source.m_timeInfo.m_numTimePoints : 1u;
  info.m_sizeInfo.m_imageSizeInPixels = spatialPixels;
  info.m_sizeInfo.m_imageSizeInComponents = spatialPixels * info.m_pixelInfo.m_numComponents * numTimePoints;
  info.m_sizeInfo.m_imageSizeInBytes =
    info.m_sizeInfo.m_imageSizeInComponents * info.m_componentInfo.m_componentSizeInBytes;
  return info;
}

bool setSizeInfoFromItkImageBase(
  SizeInfo& info,
  const typename itk::ImageBase<3>::Pointer imageBase,
  std::size_t componentSizeInBytes)
{
  if (!imageBase || imageBase.IsNull()) {
    return false;
  }

  info.m_imageSizeInPixels = imageBase->GetLargestPossibleRegion().GetNumberOfPixels();
  info.m_imageSizeInComponents = info.m_imageSizeInPixels * imageBase->GetNumberOfComponentsPerPixel();
  info.m_imageSizeInBytes = info.m_imageSizeInComponents * componentSizeInBytes;
  return true;
}

bool setSpaceInfoFromItkImageBase(SpaceInfo& info, const typename itk::ImageBase<3>::Pointer imageBase)
{
  if (!imageBase || imageBase.IsNull()) {
    return false;
  }

  info.m_numDimensions = 3;
  info.m_dimensions.resize(info.m_numDimensions);
  info.m_origin.resize(info.m_numDimensions);
  info.m_spacing.resize(info.m_numDimensions);
  info.m_directions.assign(info.m_numDimensions, std::vector<double>(info.m_numDimensions));

  const auto region = imageBase->GetLargestPossibleRegion();
  const auto size = region.GetSize();
  const auto origin = imageBase->GetOrigin();
  const auto spacing = imageBase->GetSpacing();
  const auto direction = imageBase->GetDirection();

  for (uint32_t i = 0; i < info.m_numDimensions; ++i) {
    info.m_dimensions[i] = static_cast<uint64_t>(size[i]);
    info.m_origin[i] = origin[i];
    info.m_spacing[i] = spacing[i];

    for (uint32_t j = 0; j < info.m_numDimensions; ++j) {
      info.m_directions[i][j] = direction(j, i);
    }
  }

  return true;
}
