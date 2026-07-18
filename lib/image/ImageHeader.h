#pragma once

#include "common/Types.h"
#include "image/ImageHeaderOverrides.h"
#include "image/ImageIoInfo.h"
#include "image/ImageSpatialMetadata.h"

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <filesystem>
#include <ostream>
#include <string>

/**
 * @brief Logical image metadata derived from IO metadata and user header overrides.
 *
 * ImageHeader normalizes 1D/2D/3D IO metadata into Entropy's 3D image model, tracks file and
 * in-memory component representations, computes pixel and subject-space bounding boxes, and applies
 * optional spacing/origin/direction overrides.
 */
class ImageHeader
{
public:
  /// @brief Construct an empty header. Intended for containers before real IO metadata is known.
  explicit ImageHeader() = default;

  /**
   * @brief Construct a header from on-disk and in-memory IO metadata.
   * @param ioInfoOnDisk Metadata for the source file representation.
   * @param ioInfoInMemory Metadata for the loaded memory representation.
   * @param interleavedComponents Whether multi-component values are stored interleaved in memory.
   */
  ImageHeader(const ImageIoInfo& ioInfoOnDisk, const ImageIoInfo& ioInfoInMemory, bool interleavedComponents);

  ImageHeader(const ImageHeader&) = default;
  ImageHeader& operator=(const ImageHeader&) = default;

  ImageHeader(ImageHeader&&) = default;
  ImageHeader& operator=(ImageHeader&&) = default;

  ~ImageHeader() = default;

  /// @brief Apply overrides to the original header geometry and recompute derived geometry.
  void setHeaderOverrides(const ImageHeaderOverrides& overrides);
  /// @brief Get the current header override state.
  const ImageHeaderOverrides& getHeaderOverrides() const;

  /// @brief Apply user-provided physical geometry and recompute derived geometry.
  void setUserSpatialMetadata(const ImageSpatialMetadata& metadata);
  /// @brief Clear user-provided physical geometry and return to IO metadata plus normal header overrides.
  void clearUserSpatialMetadata();
  /// @brief Return user-provided physical geometry, when present.
  const std::optional<ImageSpatialMetadata>& userSpatialMetadata() const;

  /// @brief Change component type/count metadata and recompute pixel type and sizes.
  void adjustComponents(const ComponentType& componentType, uint32_t numComponents);

  /// @brief Return whether the source image exists on disk.
  bool existsOnDisk() const;
  /// @brief Set whether the source image exists on disk.
  void setExistsOnDisk(bool);

  /// @brief Get the source file name.
  const std::filesystem::path& fileName() const;
  /// @brief Set the source file name.
  void setFileName(std::filesystem::path fileName);

  /// @brief Get number of components per pixel in memory.
  uint32_t numComponentsPerPixel() const;
  /// @brief Get total number of pixels.
  uint64_t numPixels() const;

  /// @brief Set number of components per pixel and recompute pixel type and byte sizes.
  void setNumComponentsPerPixel(uint32_t numComponents);

  /// @brief Get image byte size in the source file representation.
  uint64_t fileImageSizeInBytes() const;
  /// @brief Get image byte size in the loaded memory representation.
  uint64_t memoryImageSizeInBytes() const;

  /// @brief Get logical pixel type.
  PixelType pixelType() const;
  /// @brief Get logical pixel type as a readable string.
  std::string pixelTypeAsString() const;

  /// @brief Get source file component type.
  ComponentType fileComponentType() const;
  /// @brief Get source file component type as a readable string.
  std::string fileComponentTypeAsString() const;
  /// @brief Get source file component size in bytes.
  uint32_t fileComponentSizeInBytes() const;

  /// @brief Get loaded memory component type.
  ComponentType memoryComponentType() const;
  /// @brief Get loaded memory component type as a readable string.
  std::string memoryComponentTypeAsString() const;
  /// @brief Get loaded memory component size in bytes.
  uint32_t memoryComponentSizeInBytes() const;

  /// @brief Get additional metadata read from the image IO backend.
  const MetaDataMap& metaData() const;

  /// @brief Get image dimensions in Entropy's 3D pixel model.
  const glm::uvec3& pixelDimensions() const;
  /// @brief Get origin in physical subject space after overrides.
  const glm::vec3& origin() const;
  /// @brief Get pixel spacing in physical subject space after overrides.
  const glm::vec3& spacing() const;
  /// @brief Get axis directions in physical subject space after overrides, stored column-major.
  const glm::mat3& directions() const;

  /// All corners of the image's AXIS-ALIGNED bounding box in Voxel space
  const std::array<glm::vec3, 8>& pixelBBoxCorners() const;

  /// All corners of the image's bounding box in physical Subject space
  /// @note The bounding box will NOT be axis-aligned when the image directions are oblique
  const std::array<glm::vec3, 8>& subjectBBoxCorners() const;

  /// Center of the image's bounding box in physical Subject space
  const glm::vec3& subjectBBoxCenter() const;

  /// Size of the image's bounding box in physical Subject space
  const glm::vec3& subjectBBoxSize() const;

  /// Three-character "SPIRAL" code defining the anatomical orientation of the image in physical
  /// Subject space, where positive X, Y, and Z axes correspond to the physical Left, Posterior, and
  /// Superior directions, respectively. The acronym stands for "Superior, Posterior, Inferior,
  /// Right, Anterior, Left".
  const std::string& spiralCode() const;

  /// Flag indicating whether the image directions are oblique (i.e. skew w.r.t. the physical X, Y,
  /// Z, axes)
  bool isOblique() const;

  /// Are the pixel components interleaved? This flag is always false for 1-component images
  bool interleavedComponents() const;

  friend std::ostream& operator<<(std::ostream&, const ImageHeader&);

private:
  void setSpace(const SpaceInfo& spaceInfo);
  void setBoundingBox();

  /// Hold onto the original image information, even though these get never be retrieved by the
  /// client
  ImageIoInfo m_ioInfoOnDisk;
  ImageIoInfo m_ioInfoInMemory;

  /// Are the pixel components interleaved? This flag is always false for 1-component images
  bool m_interleavedComponents = false;

  bool m_existsOnDisk = true;       //!< Flag that the image exists on disk
  std::filesystem::path m_fileName; //!< File name

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

  glm::uvec3 m_pixelDimensions; //!< Pixel matrix dimensions
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

  /// Three-character "SPIRAL" code defining the anatomical orientation of the image in Subject
  /// space, where positive X, Y, and Z axes correspond to the physical Left, Posterior, and
  /// Superior directions, respectively. The acronym stands for "Superior, Posterior, Inferior,
  /// Right, Anterior, Left"
  std::string m_spiralCode;

  /// Flag indicating whether the image directions are oblique (i.e. skew w.r.t. the physical X, Y,
  /// Z, axes)
  bool m_isOblique;

  /// Overrides to the original image header
  ImageHeaderOverrides m_headerOverrides;

  /// User-provided geometry for standard raster images without medical spatial metadata.
  std::optional<ImageSpatialMetadata> m_userSpatialMetadata = std::nullopt;
};

std::ostream& operator<<(std::ostream&, const ImageHeader&);

#include <spdlog/fmt/ostr.h>
#if FMT_VERSION >= 90000
template<>
struct fmt::formatter<ImageHeader> : ostream_formatter
{
};
#endif
