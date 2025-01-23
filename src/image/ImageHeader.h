#ifndef IMAGE_HEADER_H
#define IMAGE_HEADER_H

#include "common/Types.h"
#include "common/filesystem.h"
#include "image/ImageHeaderOverrides.h"
#include "image/ImageIoInfo.h"

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <ostream>
#include <string>

/**
 * @brief Image header with data set upon creation or loading of image.
 */
class ImageHeader
{
public:
  explicit ImageHeader() = default;

  ImageHeader(
    const ImageIoInfo& ioInfoOnDisk, const ImageIoInfo& ioInfoInMemory, bool interleavedComponents
  );

  ImageHeader(const ImageHeader&) = default;
  ImageHeader& operator=(const ImageHeader&) = default;

  ImageHeader(ImageHeader&&) = default;
  ImageHeader& operator=(ImageHeader&&) = default;

  ~ImageHeader() = default;

  /// Set overrides to the original image header
  void setHeaderOverrides(const ImageHeaderOverrides& overrides);
  const ImageHeaderOverrides& getHeaderOverrides() const;

  void adjustComponents(const ComponentType& componentType, uint32_t numComponents);

  bool existsOnDisk() const;
  void setExistsOnDisk(bool);

  const fs::path& fileName() const; //!< File name
  void setFileName(fs::path fileName);

  uint32_t numComponentsPerPixel() const; //!< Number of components per pixel
  uint64_t numPixels() const;             //!< Number of pixels in the image

  void setNumComponentsPerPixel(uint32_t numComponents);

  uint64_t fileImageSizeInBytes() const;   //!< Image size in bytes (in file)
  uint64_t memoryImageSizeInBytes() const; //!< Image size in bytes (in memory)

  PixelType pixelType() const; //!< Pixel type
  std::string pixelTypeAsString() const;

  ComponentType fileComponentType() const; //!< Pixel component type
  std::string fileComponentTypeAsString() const;
  uint32_t fileComponentSizeInBytes() const; //!< Size of component in bytes

  ComponentType memoryComponentType() const; //!< Pixel component type in memory
  std::string memoryComponentTypeAsString() const;
  uint32_t memoryComponentSizeInBytes() const; //!< Size of component in bytes in memory

  const glm::uvec3& pixelDimensions() const; //!< Pixel dimensions (i.e. pixel matrix size)
  const glm::vec3& origin() const;           //!< Origin in physical Subject space
  const glm::vec3& spacing() const;          //!< Pixel spacing in physical Subject space
  const glm::mat3& directions(
  ) const; //!< Axis directions in physical Subject space, stored column-wise

  /// All corners of the image's AXIS-ALIGNED bounding box in Voxel space
  const std::array<glm::vec3, 8>& pixelBBoxCorners() const;

  /// All corners of the image's bounding box in physical Subject space
  /// @note The bounding box will NOT be axis-aligned when the image directions are oblique
  const std::array<glm::vec3, 8>& subjectBBoxCorners() const;

  /// Center of the image's bounding box in physical Subject space
  const glm::vec3& subjectBBoxCenter() const;

  /// Size of the image's bounding box in physical Subject space
  const glm::vec3& subjectBBoxSize() const;

  /// Three-character "SPIRAL" code defining the anatomical orientation of the image in physical Subject space,
  /// where positive X, Y, and Z axes correspond to the physical Left, Posterior, and Superior directions,
  /// respectively. The acroynm stands for "Superior, Posterior, Inferior, Right, Anterior, Left".
  const std::string& spiralCode() const;

  /// Flag indicating whether the image directions are oblique (i.e. skew w.r.t. the physical X, Y, Z, axes)
  bool isOblique() const;

  /// Are the pixel components interleaved? This flag is always false for 1-component images
  bool interleavedComponents() const;

  friend std::ostream& operator<<(std::ostream&, const ImageHeader&);

private:
  void setSpace(const SpaceInfo& spaceInfo);
  void setBoundingBox();

  /// Hold onto the original image information, even though these get never be retrieved by the client
  ImageIoInfo m_ioInfoOnDisk;
  ImageIoInfo m_ioInfoInMemory;

  /// Are the pixel components interleaved? This flag is always false for 1-component images
  bool m_interleavedComponents = false;

  bool m_existsOnDisk = true; //!< Flag that the image exists on disk
  fs::path m_fileName;        //!< File name

  uint32_t m_numComponentsPerPixel; //!< Number of components per pixel
  uint64_t m_numPixels;             //!< Number of pixels in the image

  uint64_t m_fileImageSizeInBytes;   //!< Image size in bytes (in file on disk)
  uint64_t m_memoryImageSizeInBytes; //!< Image size in bytes (in memory)

  PixelType m_pixelType; //!< Pixel type
  std::string m_pixelTypeAsString;

  ComponentType m_fileComponentType; //!< Original file pixel component type
  std::string m_fileComponentTypeAsString;
  uint32_t m_fileComponentSizeInBytes; //!< Size of original fie pixel component in bytes

  ComponentType m_memoryComponentType; //!< Pixel component type, as loaded in memory buffer
  std::string m_memoryComponentTypeAsString;
  uint32_t m_memoryComponentSizeInBytes; //!< Size of component in bytes, as loaded in memory buffer

  glm::uvec3 m_pixelDimensions; //!< Pixel dimensions (i.e. pixel matrix size)
  glm::vec3 m_origin;           //!< Origin in physical Subject space
  glm::vec3 m_spacing;          /// Pixel spacing in physical Subject space
  glm::mat3 m_directions;       /// Axis directions in physical Subject space, stored column-wise

  /// All corners of the image's AXIS-ALIGNED bounding box in Pixel space
  std::array<glm::vec3, 8> m_pixelBBoxCorners;

  /// All corners of the image's bounding box in physical Subject space
  /// @note The bounding box will NOT be axis-aligned when the image directions are oblique
  std::array<glm::vec3, 8> m_subjectBBoxCorners;

  /// Center of the image's bounding box in physical Subject space
  glm::vec3 m_subjectBBoxCenter;

  /// Size of the image's bounding box in physical Subject space
  glm::vec3 m_subjectBBoxSize;

  /// Three-character "SPIRAL" code defining the anatomical orientation of the image in Subject space,
  /// where positive X, Y, and Z axes correspond to the physical Left, Posterior, and Superior directions,
  /// respectively. The acroynm stands for "Superior, Posterior, Inferior, Right, Anterior, Left".
  std::string m_spiralCode;

  /// Flag indicating whether the image directions are oblique (i.e. skew w.r.t. the physical X, Y, Z, axes)
  bool m_isOblique;

  /// Overrides to the original image header
  ImageHeaderOverrides m_headerOverrides;
};

std::ostream& operator<<(std::ostream&, const ImageHeader&);

#include <spdlog/fmt/ostr.h>
#if FMT_VERSION >= 90000
template<>
struct fmt::formatter<ImageHeader> : ostream_formatter
{
};
#endif

#endif // IMAGE_HEADER_H
