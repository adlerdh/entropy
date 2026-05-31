#include "ImageIoInfo.h"
#include "internal/ImageUtilityItk.h"

#include "Exception.hpp"

#include <spdlog/spdlog.h>

#include <itkIOCommon.h> // for itk::SpatialOrientation
#include <itkMetaDataDictionary.h>
#include <itkMetaDataObject.h>

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
      sout << std::ends;

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

  if (3 < info.m_numDimensions) {
    spdlog::error("Image dimension ({}) greater than 3 is not supported", info.m_numDimensions);
    return false;
  }

  info.m_dimensions.resize(info.m_numDimensions);
  info.m_origin.resize(info.m_numDimensions);
  info.m_spacing.resize(info.m_numDimensions);
  info.m_directions.resize(info.m_numDimensions);

  for (uint32_t i = 0; i < info.m_numDimensions; ++i) {
    info.m_dimensions[i] = static_cast<uint64_t>(imageIo->GetDimensions(i));
    info.m_origin[i] = imageIo->GetOrigin(i);
    info.m_spacing[i] = imageIo->GetSpacing(i);
    info.m_directions[i] = imageIo->GetDirection(i);
  }

  return true;
}
} // anonymous namespace

bool FileInfo::validate() const
{
  return true;
}

bool ComponentInfo::validate() const
{
  return true;
}

bool PixelInfo::validate() const
{
  return true;
}

bool SizeInfo::validate() const
{
  return true;
}

bool SpaceInfo::validate() const
{
  return true;
}

bool ImageIoInfo::validate() const
{
  return (
    m_fileInfo.validate() && m_componentInfo.validate() && m_pixelInfo.validate() && m_sizeInfo.validate() &&
    m_spaceInfo.validate());
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
    setSpaceInfoFromItk(info.m_spaceInfo, imageIo));
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
  info.m_directions.resize(info.m_numDimensions);

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
