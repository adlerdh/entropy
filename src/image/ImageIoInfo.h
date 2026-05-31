#pragma once

#include "Filesystem.h"
#include "Types.h"

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

class FileInfo
{
public:
  bool validate() const;

  fs::path m_fileName;

  std::string m_byteOrderString{"OrderNotApplicable"};
  bool m_useCompression{false};

  std::string m_fileTypeString{"TypeNotApplicable"};

  std::vector<std::string> m_supportedReadExtensions;
  std::vector<std::string> m_supportedWriteExtensions;
};

class ComponentInfo
{
public:
  bool validate() const;

  ComponentType m_componentType{ComponentType::Undefined};
  std::string m_componentTypeString{"Undefined"};
  uint32_t m_componentSizeInBytes{0u};
};

class PixelInfo
{
public:
  bool validate() const;

  PixelType m_pixelType{PixelType::Undefined};
  std::string m_pixelTypeString{"Undefined"};
  uint32_t m_numComponents{0u};
  uint32_t m_pixelStrideInBytes{0u};
};

class SizeInfo
{
public:
  bool validate() const;

  std::size_t m_imageSizeInComponents{0u};
  std::size_t m_imageSizeInPixels{0u};
  std::size_t m_imageSizeInBytes{0u};
};

class SpaceInfo
{
public:
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
    std::vector<char>,
    std::vector<int>,
    std::vector<float>,
    std::vector<double>,
    std::vector<std::vector<float>>,
    std::vector<std::vector<double>>>>;

class ImageIoInfo
{
public:
  bool validate() const;

  FileInfo m_fileInfo;
  ComponentInfo m_componentInfo;
  PixelInfo m_pixelInfo;
  SizeInfo m_sizeInfo;
  SpaceInfo m_spaceInfo;
  MetaDataMap m_metaData;
};
