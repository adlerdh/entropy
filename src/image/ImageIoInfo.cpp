#include "image/ImageIoInfo.h"
#include "common/Exception.hpp"

#include <spdlog/spdlog.h>

#include <itkIOCommon.h> // for itk::SpatialOrientation
#include <itkMetaDataDictionary.h>
#include <itkMetaDataObject.h>

namespace
{

namespace so = ::itk::SpatialOrientation;

std::unordered_map<so::ValidCoordinateOrientationFlags, std::string> spiralCodeMap
  = {{so::ITK_COORDINATE_ORIENTATION_RIP, "RIP"}, {so::ITK_COORDINATE_ORIENTATION_LIP, "LIP"},
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
bool setMetaDataEntry(
  MetaDataMap& metaDataMap, const itk::MetaDataDictionary& dictionary, const std::string& key
)
{
  T value;

  if (itk::ExposeMetaData<T>(dictionary, key, value))
  {
    metaDataMap[key] = value;
    return true;
  }
  else
  {
    return false;
  }
}

MetaDataMap getMetaDataMap(const ::itk::ImageIOBase::Pointer imageIo)
{
  MetaDataMap metaDataMap;

  if (imageIo.IsNull())
  {
    return metaDataMap;
  }

  const itk::MetaDataDictionary& dictionary = imageIo->GetMetaDataDictionary();
  itk::MetaDataDictionary::ConstIterator itr;

  for (itr = dictionary.Begin(); itr != dictionary.End(); ++itr)
  {
    const std::string key = itr->first;
    std::string value;

    itk::SpatialOrientation::ValidCoordinateOrientationFlags orientFlagsValue
      = itk::SpatialOrientationEnums::ValidCoordinateOrientations::ITK_COORDINATE_ORIENTATION_INVALID;

    if (itk::ExposeMetaData<std::string>(dictionary, key, value))
    {
      /// @note For some weird reason, some of the strings returned by this method
      /// contain '\0' characters. We replace them by spaces:
      std::ostringstream sout("");
      for (std::size_t i = 0; i < value.length(); ++i)
      {
        if (value[i] >= ' ')
        {
          sout << value[i];
        }
      }
      sout << std::ends;

      metaDataMap[key] = sout.str();
    }
    else if (itk::ExposeMetaData(dictionary, key, orientFlagsValue))
    {
      metaDataMap[key] = spiralCodeMap[orientFlagsValue];
    }
    else
    {
      if (setMetaDataEntry<bool>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<unsigned char>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<char>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<signed char>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<unsigned short>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<short>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<unsigned int>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<int>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<unsigned long>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<long>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<unsigned long long>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<long long>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<float>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<double>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<std::vector<float>>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<std::vector<double>>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<std::vector<std::vector<float>>>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<std::vector<std::vector<double>>>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<itk::Array<char>>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<itk::Array<int>>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<itk::Array<float>>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<itk::Array<double>>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<itk::Matrix<float, 4, 4>>(metaDataMap, dictionary, key))
      {
        continue;
      }
      if (setMetaDataEntry<itk::Matrix<double>>(metaDataMap, dictionary, key))
      {
        continue;
      }

      spdlog::info("Key {} is of unsupported type {}", key, itr->second->GetMetaDataObjectTypeName());
    }
  }

  return metaDataMap;
}

} // anonymous namespace

FileInfo::FileInfo()
  : m_fileName()
  , m_byteOrder(itk::IOByteOrderEnum::OrderNotApplicable)
  , m_byteOrderString("OrderNotApplicable")
  , m_useCompression(false)
  , m_fileType(itk::IOFileEnum::TypeNotApplicable)
  , m_fileTypeString("TypeNotApplicable")
  , m_supportedReadExtensions()
  , m_supportedWriteExtensions()
{
}

FileInfo::FileInfo(const ::itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || !set(imageIo))
  {
    spdlog::error("Error while instantiating FileInfo");
    throw_debug("Error while instantiating FileInfo")
  }
}

bool FileInfo::set(const itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || imageIo.IsNull())
  {
    return false;
  }

  m_fileName = imageIo->GetFileName();

  m_byteOrder = imageIo->GetByteOrder();
  m_byteOrderString = imageIo->GetByteOrderAsString(m_byteOrder);
  m_useCompression = imageIo->GetUseCompression();

  m_fileType = imageIo->GetFileType();
  m_fileTypeString = imageIo->GetFileTypeAsString(m_fileType);

  m_supportedReadExtensions = imageIo->GetSupportedReadExtensions();
  m_supportedWriteExtensions = imageIo->GetSupportedWriteExtensions();

  return true;
}

bool FileInfo::validate() const
{
  return true;
}

ComponentInfo::ComponentInfo()
  : m_componentType(itk::IOComponentEnum::UNKNOWNCOMPONENTTYPE)
  , m_componentTypeString("UNKNOWNCOMPONENTTYPE")
  , m_componentSizeInBytes(0u)
{
}

ComponentInfo::ComponentInfo(const ::itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || !set(imageIo))
  {
    spdlog::error("Error while instantiating ComponentInfo");
    throw_debug("Error while instantiating ComponentInfo")
  }
}

bool ComponentInfo::set(const itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || imageIo.IsNull())
  {
    return false;
  }

  m_componentType = imageIo->GetComponentType();
  m_componentTypeString = itk::ImageIOBase::GetComponentTypeAsString(m_componentType);
  m_componentSizeInBytes = static_cast<uint32_t>(imageIo->GetComponentSize());

  return true;
}

bool ComponentInfo::validate() const
{
  return true;
}

PixelInfo::PixelInfo()
  : m_pixelType(itk::IOPixelEnum::UNKNOWNPIXELTYPE)
  , m_pixelTypeString("UNKNOWNPIXELTYPE")
  , m_numComponents(0u)
  , m_pixelStrideInBytes(0)
{
}

PixelInfo::PixelInfo(const ::itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || !set(imageIo))
  {
    spdlog::error("Error while instantiating PixelInfo");
    throw_debug("Error while instantiating PixelInfo")
  }
}

bool PixelInfo::set(const itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || imageIo.IsNull())
  {
    return false;
  }

  m_pixelType = imageIo->GetPixelType();
  m_pixelTypeString = itk::ImageIOBase::GetPixelTypeAsString(m_pixelType);
  m_numComponents = static_cast<uint32_t>(imageIo->GetNumberOfComponents());
  m_pixelStrideInBytes = static_cast<uint32_t>(imageIo->GetPixelStride());

  return true;
}

bool PixelInfo::validate() const
{
  return true;
}

SizeInfo::SizeInfo()
  : m_imageSizeInComponents(0)
  , m_imageSizeInPixels(0)
  , m_imageSizeInBytes(0)
{
}

SizeInfo::SizeInfo(const ::itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || !set(imageIo))
  {
    spdlog::error("Error while instantiating SizeInfo");
    throw_debug("Error while instantiating SizeInfo")
  }
}

bool SizeInfo::set(const itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || imageIo.IsNull())
  {
    return false;
  }

  m_imageSizeInComponents = static_cast<size_t>(imageIo->GetImageSizeInComponents());
  m_imageSizeInPixels = static_cast<size_t>(imageIo->GetImageSizeInPixels());
  m_imageSizeInBytes = static_cast<size_t>(imageIo->GetImageSizeInBytes());

  return true;
}

bool SizeInfo::set(
  const typename ::itk::ImageBase<3>::Pointer imageBase, const size_t componentSizeInBytes
)
{
  if (!imageBase || imageBase.IsNull())
  {
    return false;
  }

  m_imageSizeInPixels = imageBase->GetLargestPossibleRegion().GetNumberOfPixels();
  m_imageSizeInComponents = m_imageSizeInPixels * imageBase->GetNumberOfComponentsPerPixel();
  m_imageSizeInBytes = m_imageSizeInComponents * componentSizeInBytes;

  return true;
}

bool SizeInfo::validate() const
{
  return true;
}

SpaceInfo::SpaceInfo()
  : m_numDimensions(0u)
  , m_dimensions()
  , m_origin()
  , m_spacing()
  , m_directions()
{
}

SpaceInfo::SpaceInfo(const ::itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || !set(imageIo))
  {
    spdlog::error("Error while instantiating SpaceInfo");
    throw_debug("Error while instantiating SpaceInfo")
  }
}

bool SpaceInfo::set(const itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || imageIo.IsNull())
  {
    return false;
  }

  m_numDimensions = static_cast<uint32_t>(imageIo->GetNumberOfDimensions());

  if (3 < m_numDimensions)
  {
    return false;
  }

  m_dimensions.resize(m_numDimensions);
  m_origin.resize(m_numDimensions);
  m_spacing.resize(m_numDimensions);
  m_directions.resize(m_numDimensions);

  for (uint32_t i = 0; i < m_numDimensions; ++i)
  {
    m_dimensions[i] = static_cast<uint64_t>(imageIo->GetDimensions(i));
    m_origin[i] = imageIo->GetOrigin(i);
    m_spacing[i] = imageIo->GetSpacing(i);
    m_directions[i] = imageIo->GetDirection(i);
  }

  return true;
}

bool SpaceInfo::set(const typename ::itk::ImageBase<3>::Pointer imageBase)
{
  if (!imageBase || imageBase.IsNull())
  {
    return false;
  }

  m_numDimensions = 3;

  m_dimensions.resize(m_numDimensions);
  m_origin.resize(m_numDimensions);
  m_spacing.resize(m_numDimensions);
  m_directions.resize(m_numDimensions);

  const typename ::itk::ImageBase<3>::RegionType region = imageBase->GetLargestPossibleRegion();
  const typename ::itk::ImageBase<3>::RegionType::SizeType size = region.GetSize();
  const typename ::itk::ImageBase<3>::PointType origin = imageBase->GetOrigin();
  const typename ::itk::ImageBase<3>::SpacingType spacing = imageBase->GetSpacing();
  const typename ::itk::ImageBase<3>::DirectionType direction = imageBase->GetDirection();

  for (uint32_t i = 0; i < m_numDimensions; ++i)
  {
    m_dimensions[i] = static_cast<uint64_t>(size[i]);
    m_origin[i] = origin[i];
    m_spacing[i] = spacing[i];

    for (uint32_t j = 0; j < m_numDimensions; ++j)
    {
      /// @internal The j'th component of i'th direction vector is the
      /// direction matrix element at row j and column i
      m_directions[i][j] = direction(j, i);
    }
  }

  return true;
}

bool SpaceInfo::validate() const
{
  return true;
}

ImageIoInfo::ImageIoInfo(const ::itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || !set(imageIo))
  {
    spdlog::error("Error while instantiating ImageIoInfo");
    throw_debug("Error while instantiating ImageIoInfo")
  }
}

bool ImageIoInfo::set(const itk::ImageIOBase::Pointer imageIo)
{
  if (!imageIo || imageIo.IsNull())
  {
    return false;
  }

  m_metaData = getMetaDataMap(imageIo);

  return (
    m_fileInfo.set(imageIo) && m_componentInfo.set(imageIo) && m_pixelInfo.set(imageIo)
    && m_sizeInfo.set(imageIo) && m_spaceInfo.set(imageIo)
  );
}

bool ImageIoInfo::validate() const
{
  return (
    m_fileInfo.validate() && m_componentInfo.validate() && m_pixelInfo.validate()
    && m_sizeInfo.validate() && m_spaceInfo.validate()
  );
}
