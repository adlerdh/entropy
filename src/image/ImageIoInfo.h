#pragma once

#include "common/Filesystem.h"

#include <itkCommonEnums.h>
#include <itkImageBase.h>
#include <itkImageIOBase.h>

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

class FileInfo
{
public:
  FileInfo() = default;
  FileInfo(const itk::ImageIOBase::Pointer imageIo);

  bool set(const itk::ImageIOBase::Pointer imageIo);
  bool validate() const;

  fs::path m_fileName;

  itk::IOByteOrderEnum m_byteOrder{itk::IOByteOrderEnum::OrderNotApplicable};
  std::string m_byteOrderString{"OrderNotApplicable"};
  bool m_useCompression{false};

  itk::IOFileEnum m_fileType{itk::IOFileEnum::TypeNotApplicable};
  std::string m_fileTypeString{"TypeNotApplicable"};

  std::vector<std::string> m_supportedReadExtensions;
  std::vector<std::string> m_supportedWriteExtensions;
};

class ComponentInfo
{
public:
  ComponentInfo() = default;
  ComponentInfo(const ::itk::ImageIOBase::Pointer imageIo);

  bool set(const ::itk::ImageIOBase::Pointer imageIo);
  bool validate() const;

  itk::IOComponentEnum m_componentType{itk::IOComponentEnum::UNKNOWNCOMPONENTTYPE};
  std::string m_componentTypeString{"UNKNOWNCOMPONENTTYPE"};
  uint32_t m_componentSizeInBytes{0u};
};

class PixelInfo
{
public:
  PixelInfo() = default;
  PixelInfo(const itk::ImageIOBase::Pointer imageIo);

  bool set(const itk::ImageIOBase::Pointer imageIo);
  bool validate() const;

  itk::IOPixelEnum m_pixelType{itk::IOPixelEnum::UNKNOWNPIXELTYPE};
  std::string m_pixelTypeString{"UNKNOWNPIXELTYPE"};
  uint32_t m_numComponents{0u};
  uint32_t m_pixelStrideInBytes{0u};
};

class SizeInfo
{
public:
  SizeInfo() = default;
  SizeInfo(const itk::ImageIOBase::Pointer imageIo);

  bool set(const itk::ImageIOBase::Pointer imageIo);
  bool set(const typename itk::ImageBase<3>::Pointer imageBase, const size_t componentSizeInBytes);
  bool validate() const;

  std::size_t m_imageSizeInComponents{0u};
  std::size_t m_imageSizeInPixels{0u};
  std::size_t m_imageSizeInBytes{0u};
};

class SpaceInfo
{
public:
  SpaceInfo() = default;
  SpaceInfo(const ::itk::ImageIOBase::Pointer imageIo);

  bool set(const ::itk::ImageIOBase::Pointer imageIo);
  bool set(const typename ::itk::ImageBase<3>::Pointer imageBase);
  bool validate() const;

  uint32_t m_numDimensions{0u};
  std::vector<std::size_t> m_dimensions;
  std::vector<double> m_origin;
  std::vector<double> m_spacing;
  std::vector<std::vector<double>> m_directions;
};

using MetaDataMap = std::unordered_map<
  std::string,
  std::variant<
    bool,
    unsigned char,
    char,
    signed char,
    unsigned short,
    short,
    unsigned int,
    int,
    unsigned long,
    long,
    unsigned long long,
    long long,
    float,
    double,
    std::string,
    std::vector<float>,
    std::vector<double>,
    std::vector<std::vector<float>>,
    std::vector<std::vector<double>>,
    itk::Array<char>,
    itk::Array<int>,
    itk::Array<float>,
    itk::Array<double>,
    itk::Matrix<float, 4, 4>,
    itk::Matrix<double>>>;

class ImageIoInfo
{
public:
  ImageIoInfo() = default;
  ImageIoInfo(const itk::ImageIOBase::Pointer imageIo);

  bool set(const itk::ImageIOBase::Pointer imageIo);
  bool validate() const;

  FileInfo m_fileInfo;
  ComponentInfo m_componentInfo;
  PixelInfo m_pixelInfo;
  SizeInfo m_sizeInfo;
  SpaceInfo m_spaceInfo;
  MetaDataMap m_metaData;
};
