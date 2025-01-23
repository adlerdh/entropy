#ifndef IMAGE_IO_INFO_H
#define IMAGE_IO_INFO_H

#include "common/filesystem.h"

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
  FileInfo();
  FileInfo(const ::itk::ImageIOBase::Pointer imageIo);

  bool set(const ::itk::ImageIOBase::Pointer imageIo);
  bool validate() const;

  fs::path m_fileName;

  itk::IOByteOrderEnum m_byteOrder;
  std::string m_byteOrderString;
  bool m_useCompression;

  itk::IOFileEnum m_fileType;
  std::string m_fileTypeString;

  std::vector<std::string> m_supportedReadExtensions;
  std::vector<std::string> m_supportedWriteExtensions;
};

class ComponentInfo
{
public:
  ComponentInfo();
  ComponentInfo(const ::itk::ImageIOBase::Pointer imageIo);

  bool set(const ::itk::ImageIOBase::Pointer imageIo);
  bool validate() const;

  itk::IOComponentEnum m_componentType;
  std::string m_componentTypeString;
  uint32_t m_componentSizeInBytes;
};

class PixelInfo
{
public:
  PixelInfo();
  PixelInfo(const ::itk::ImageIOBase::Pointer imageIo);

  bool set(const ::itk::ImageIOBase::Pointer imageIo);
  bool validate() const;

  itk::IOPixelEnum m_pixelType;
  std::string m_pixelTypeString;
  uint32_t m_numComponents;
  uint32_t m_pixelStrideInBytes;
};

class SizeInfo
{
public:
  SizeInfo();
  SizeInfo(const ::itk::ImageIOBase::Pointer imageIo);

  bool set(const ::itk::ImageIOBase::Pointer imageIo);
  bool set(const typename ::itk::ImageBase<3>::Pointer imageBase, const size_t componentSizeInBytes);
  bool validate() const;

  size_t m_imageSizeInComponents;
  size_t m_imageSizeInPixels;
  size_t m_imageSizeInBytes;
};

class SpaceInfo
{
public:
  SpaceInfo();
  SpaceInfo(const ::itk::ImageIOBase::Pointer imageIo);

  bool set(const ::itk::ImageIOBase::Pointer imageIo);
  bool set(const typename ::itk::ImageBase<3>::Pointer imageBase);
  bool validate() const;

  uint32_t m_numDimensions;
  std::vector<size_t> m_dimensions;
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
  ImageIoInfo(const ::itk::ImageIOBase::Pointer imageIo);

  bool set(const ::itk::ImageIOBase::Pointer imageIo);
  bool validate() const;

  FileInfo m_fileInfo;
  ComponentInfo m_componentInfo;
  PixelInfo m_pixelInfo;
  SizeInfo m_sizeInfo;
  SpaceInfo m_spaceInfo;
  MetaDataMap m_metaData;
};

#endif // IMAGE_IO_INFO_H
