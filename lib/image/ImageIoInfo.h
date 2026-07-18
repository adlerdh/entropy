#pragma once

#include "common/Types.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

/**
 * @brief File-level metadata reported by image IO.
 */
class FileInfo
{
public:
  /// @brief Return true when required file metadata is populated.
  bool validate() const;

  std::filesystem::path m_fileName; //!< Source file name

  std::string m_byteOrderString{"OrderNotApplicable"}; //!< Byte order as reported by IO
  bool m_useCompression{false};                        //!< Whether the source format uses compression

  std::string m_fileTypeString{"TypeNotApplicable"}; //!< File type as reported by IO

  std::vector<std::string> m_supportedReadExtensions;  //!< Extensions readable by the IO backend
  std::vector<std::string> m_supportedWriteExtensions; //!< Extensions writable by the IO backend
};

/**
 * @brief Pixel component metadata reported by image IO.
 */
class ComponentInfo
{
public:
  /// @brief Return true when the component type and byte size are known.
  bool validate() const;

  ComponentType m_componentType{ComponentType::Undefined}; //!< Entropy component type
  std::string m_componentTypeString{"Undefined"};          //!< Backend component type string
  uint32_t m_componentSizeInBytes{0u};                     //!< Bytes per component value
};

/**
 * @brief Pixel-level metadata reported by image IO.
 */
class PixelInfo
{
public:
  /// @brief Return true when pixel type, component count, and stride are known.
  bool validate() const;

  PixelType m_pixelType{PixelType::Undefined}; //!< Entropy pixel type
  std::string m_pixelTypeString{"Undefined"};  //!< Backend pixel type string
  uint32_t m_numComponents{0u};                //!< Components per pixel
  uint32_t m_pixelStrideInBytes{0u};           //!< Bytes per pixel
};

/**
 * @brief Image buffer size metadata reported by image IO.
 */
class SizeInfo
{
public:
  /// @brief Return true when image size in pixels, components, and bytes is nonzero.
  bool validate() const;

  std::size_t m_imageSizeInComponents{0u}; //!< Total component values in the image
  std::size_t m_imageSizeInPixels{0u};     //!< Total pixels in the image
  std::size_t m_imageSizeInBytes{0u};      //!< Total encoded image bytes
};

/**
 * @brief Image spatial metadata reported by image IO.
 */
class SpaceInfo
{
public:
  /// @brief Return true when dimensions, origin, spacing, and directions are internally consistent.
  bool validate() const;

  uint32_t m_numDimensions{0u};                  //!< Number of dimensions reported by IO
  std::vector<std::size_t> m_dimensions;         //!< Pixel dimensions for each axis
  std::vector<double> m_origin;                  //!< Physical origin for each axis
  std::vector<double> m_spacing;                 //!< Physical spacing for each axis
  std::vector<std::vector<double>> m_directions; //!< Direction matrix rows/columns as reported by IO
};

/**
 * @brief Time-axis metadata derived from image IO.
 */
class TimeInfo
{
public:
  /// @brief Return true when the time axis metadata is usable.
  bool validate() const;

  uint32_t m_numTimePoints{1u}; //!< Number of frames along the time axis
  double m_origin{0.0};         //!< Time value of the first frame
  double m_spacing{1.0};        //!< Time spacing between adjacent frames
  std::string m_units{"frame"}; //!< Display units for time values
};

/// @brief Supported metadata value types copied from image IO dictionaries.
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

/**
 * @brief Complete image IO metadata snapshot.
 */
class ImageIoInfo
{
public:
  /// @brief Return true when all required metadata groups validate.
  bool validate() const;

  FileInfo m_fileInfo;           //!< File-level metadata
  ComponentInfo m_componentInfo; //!< Component-level metadata
  PixelInfo m_pixelInfo;         //!< Pixel-level metadata
  SizeInfo m_sizeInfo;           //!< Buffer size metadata
  SpaceInfo m_spaceInfo;         //!< Spatial metadata
  TimeInfo m_timeInfo;           //!< Time-axis metadata
  MetaDataMap m_metaData;        //!< Additional backend metadata
};

/**
 * @brief Normalize format-specific axis metadata into Entropy's image model.
 * @param info Image IO metadata to update in-place.
 * @param fileName Source image path, used for file-format metadata that ITK does not expose uniformly.
 *
 * NRRD files may encode vector components as an explicit axis. Canonical 4D images use the fourth
 * axis as time. This function converts those conventions into component and time metadata while
 * preserving the logical spatial axes.
 */
void normalizeImageIoAxesForEntropy(ImageIoInfo& info, const std::filesystem::path& fileName);

/**
 * @brief Convert on-disk metadata to the spatial metadata used by Entropy in memory.
 * @param source Normalized on-disk image IO metadata.
 * @return Metadata with time removed from the spatial dimensions.
 */
ImageIoInfo spatializedImageIoInfoForEntropy(const ImageIoInfo& source);
