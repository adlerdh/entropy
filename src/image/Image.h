#ifndef IMAGE_H
#define IMAGE_H

#include "common/Types.h"
#include "common/filesystem.h"

#include "image/ImageHeader.h"
#include "image/ImageHeaderOverrides.h"
#include "image/ImageIoInfo.h"
#include "image/ImageSettings.h"
#include "image/ImageTransformations.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief Encapsulates a 3D medical image with one or more components per pixel
 */
class Image
{
public:
  /// @brief What does the image represent?
  enum class ImageRepresentation
  {
    Image,       //!< A scalar or vector image
    Segmentation //!< A segmentation
  };

  /// @brief How should Image hold data for multi-component images?
  enum class MultiComponentBufferType
  {
    SeparateImages,  //!< Each component is a separate image
    InterleavedImage //!< Interleave all components in a single image
  };

  /**
     * @brief Construct Image from a file on disk
     * @param[in] fileName Path to image file
     * @param[in] imageRep Indicates whether this is an image or a segmentation
     * @param[in] bufferType Indicates whether multi-component images are loaded as
     * multiple buffers or as a single buffer with interleaved pixel components
     */
  Image(
    const fs::path& fileName,
    const ImageRepresentation& imageRep,
    const MultiComponentBufferType& bufferType
  );

  /**
     * @brief Construct Image from a header and raw data
     * @param[in] header
     * @param[in] displayName
     * @param[in] imageRep Indicates whether this is an image or a segmentation
     * @param[in] bufferType Indicates whether multi-component images are loaded as
     * multiple buffers or as a single buffer with interleaved pixel components
     * @param[in] imageDataComponents Must match the format specified in \c bufferType.
     * If the components are interleaved, then component 0 holds all buffers
     */
  Image(
    const ImageHeader& header,
    const std::string& displayName,
    const ImageRepresentation& imageRep,
    const MultiComponentBufferType& bufferType,
    const std::vector<const void*>& imageDataComponents
  );

  Image(const Image&) = default;
  Image& operator=(const Image&) = default;

  Image(Image&&) = default;
  Image& operator=(Image&&) = default;

  ~Image() = default;

  /** @brief Save an image component to disk. If the image is successfully saved and a
     * new file name is provided, then the Image's file name is set to the new file name.
     * @param[in] component Component of the image to save
     * @param[in] newFileName Optional new file name at which to save the image
     * @return True iff the image was saved successfully
     */
  bool saveComponentToDisk(uint32_t component, const std::optional<fs::path>& newFileName);

  bool generateSortedBuffers();

  const ImageRepresentation& imageRep() const;
  const MultiComponentBufferType& bufferType() const;

  /** @brief Get a const void pointer to the raw buffer data of an image component.
     * @param[in] component Image component to get
     * @note If \c MultiComponentBufferType::InterleavedImage, then 0 is the only valid input component
     * @note The component must be in the range [0, header().numComponentsPerPixel() - 1].
     * To read the data, cast this buffer to the appropriate component type obtained via
     * header().componentType().
     * @note A scalar image has a single component (0)
     */
  const void* bufferAsVoid(uint32_t component) const;

  /// @brief Get a non-const void pointer to the raw buffer data of an image component.
  void* bufferAsVoid(uint32_t component);

  /** @brief Get a const void pointer to the sorted buffer data of an image component.
     *  @param[in] component Image component to get
     *  @note Ignores the \c MultiComponentBufferType setting, so that the
     *  component must be in the range [0, header().numComponentsPerPixel() - 1]
     */
  const void* bufferSortedAsVoid(uint32_t component) const;

  /// @brief Get a non-const void pointer to the sorted buffer data of an image component.
  void* bufferSortedAsVoid(uint32_t component);

  /// @brief Get the value of the buffer at image 1D index
  template<typename T>
  std::optional<T> value(uint32_t component, std::size_t index) const
  {
    if (index >= m_header.numPixels())
    {
      return std::nullopt;
    }

    const auto compAndOffset = getComponentAndOffsetForBuffer(component, index);
    if (!compAndOffset)
    {
      return std::nullopt;
    }

    const std::size_t c = compAndOffset->first;
    const std::size_t offset = compAndOffset->second;

    switch (m_header.memoryComponentType())
    {
    case ComponentType::Int8:
      return static_cast<T>(m_data_int8.at(c)[offset]);
    case ComponentType::UInt8:
      return static_cast<T>(m_data_uint8.at(c)[offset]);
    case ComponentType::Int16:
      return static_cast<T>(m_data_int16.at(c)[offset]);
    case ComponentType::UInt16:
      return static_cast<T>(m_data_uint16.at(c)[offset]);
    case ComponentType::Int32:
      return static_cast<T>(m_data_int32.at(c)[offset]);
    case ComponentType::UInt32:
      return static_cast<T>(m_data_uint32.at(c)[offset]);
    case ComponentType::Float32:
      return static_cast<T>(m_data_float32.at(c)[offset]);
    default:
      return std::nullopt;
    }
  }

  /// @brief Get the value of the buffer at image 3D index (i, j, k)
  template<typename T>
  std::optional<T> value(uint32_t component, int i, int j, int k) const
  {
    const glm::u64vec3& dims = m_header.pixelDimensions();

    if (i < 0 || j < 0 || k < 0 ||
            i >= static_cast<int64_t>(dims.x) ||
            j >= static_cast<int64_t>(dims.y) ||
            k >= static_cast<int64_t>(dims.z))
    {
      return std::nullopt;
    }

    const std::size_t index = dims.x * dims.y * static_cast<std::size_t>(k)
                              + dims.x * static_cast<std::size_t>(j) + static_cast<std::size_t>(i);

    return value<T>(component, index);
  }

  /// @brief Get the linearly interpolated value of the buffer at continuous image 3D index (i, j, k)
  template<typename T>
  std::optional<T> valueLinear(uint32_t comp, double i, double j, double k) const
  {
    static const glm::dvec3 ZERO{0.0};
    static const glm::dvec3 ONE{1.0};

    const glm::u64vec3& dims = m_header.pixelDimensions();

    if (i < -0.5 || j < -0.5 || k < -0.5 || i > dims.x - 0.5 || j > dims.y - 0.5 || k > dims.z - 0.5)
    {
      return std::nullopt;
    }

    // Valid image coordinates are [-0.5, N-0.5]. However, we clamp coordinates to the edge samples,
    // which are at 0 and N - 1:
    const glm::dvec3 coordClamped = glm::clamp(glm::dvec3{i, j, k}, ZERO, glm::dvec3{dims} - ONE);
    const glm::i64vec3 f = glm::i64vec3{glm::floor(coordClamped)};

    // Get values of all 8 neighboring pixels. If a pixel outside the image is requested,
    // then its sampling location was outside the image and its returned value is std::none
    const auto c000 = value<double>(comp, f.x + 0, f.y + 0, f.z + 0);
    const auto c001 = value<double>(comp, f.x + 0, f.y + 0, f.z + 1);
    const auto c010 = value<double>(comp, f.x + 0, f.y + 1, f.z + 0);
    const auto c011 = value<double>(comp, f.x + 0, f.y + 1, f.z + 1);
    const auto c100 = value<double>(comp, f.x + 1, f.y + 0, f.z + 0);
    const auto c101 = value<double>(comp, f.x + 1, f.y + 0, f.z + 1);
    const auto c110 = value<double>(comp, f.x + 1, f.y + 1, f.z + 0);
    const auto c111 = value<double>(comp, f.x + 1, f.y + 1, f.z + 1);

    const glm::dvec3 diff = coordClamped - glm::floor(coordClamped);

    // Interpolate along x, ignoring invalid samples:
    std::optional<double> c00, c01, c10, c11;

    if (c000 && c100)
    {
      c00 = c000.value() * (1.0 - diff.x) + c100.value() * diff.x;
    }
    else if (c000)
    {
      c00 = c000.value();
    }
    else if (c100)
    {
      c00 = c100.value();
    }

    if (c001 && c101)
    {
      c01 = c001.value() * (1.0 - diff.x) + c101.value() * diff.x;
    }
    else if (c001)
    {
      c01 = c001.value();
    }
    else if (c101)
    {
      c01 = c101.value();
    }

    if (c010 && c110)
    {
      c10 = c010.value() * (1.0 - diff.x) + c110.value() * diff.x;
    }
    else if (c010)
    {
      c10 = c010.value();
    }
    else if (c110)
    {
      c10 = c110.value();
    }

    if (c011 && c111)
    {
      c11 = c011.value() * (1.0 - diff.x) + c111.value() * diff.x;
    }
    else if (c011)
    {
      c11 = c011.value();
    }
    else if (c111)
    {
      c11 = c111.value();
    }

    // Interpolate along y, ignoring invalid samples:
    std::optional<double> c0, c1;

    if (c00 && c10)
    {
      c0 = c00.value() * (1.0 - diff.y) + c10.value() * diff.y;
    }
    else if (c00)
    {
      c0 = c00.value();
    }
    else if (c10)
    {
      c0 = c10.value();
    }

    if (c01 && c11)
    {
      c1 = c01.value() * (1.0 - diff.y) + c11.value() * diff.y;
    }
    else if (c01)
    {
      c1 = c01.value();
    }
    else if (c11)
    {
      c1 = c11.value();
    }

    // Interpolate along z, ignoring invalid samples:
    std::optional<double> c;

    if (c0 && c1)
    {
      c = c0.value() * (1.0 - diff.z) + c1.value() * diff.z;
    }
    else if (c0)
    {
      c = c0.value();
    }
    else if (c1)
    {
      c = c1.value();
    }

    return c;
  }

  /// @brief Set the value of the buffer at image index (i, j, k)
  template<typename T>
  bool setValue(uint32_t component, int i, int j, int k, T value)
  {
    const glm::u64vec3& dims = m_header.pixelDimensions();

    if (i < 0 || j < 0 || k < 0 ||
            i >= static_cast<int64_t>(dims.x) ||
            j >= static_cast<int64_t>(dims.y) ||
            k >= static_cast<int64_t>(dims.z))
    {
      return false;
    }

    const auto compAndOffset = getComponentAndOffsetForBuffer(component, i, j, k);
    if (!compAndOffset)
    {
      return false;
    }

    const std::size_t c = compAndOffset->first;
    const std::size_t offset = compAndOffset->second;

    switch (m_header.memoryComponentType())
    {
    case ComponentType::Int8:
      m_data_int8.at(c)[offset] = static_cast<int8_t>(value);
      return true;
    case ComponentType::UInt8:
      m_data_uint8.at(c)[offset] = static_cast<uint8_t>(value);
      return true;
    case ComponentType::Int16:
      m_data_int16.at(c)[offset] = static_cast<int16_t>(value);
      return true;
    case ComponentType::UInt16:
      m_data_uint16.at(c)[offset] = static_cast<uint16_t>(value);
      return true;
    case ComponentType::Int32:
      m_data_int32.at(c)[offset] = static_cast<int32_t>(value);
      return true;
    case ComponentType::UInt32:
      m_data_uint32.at(c)[offset] = static_cast<uint32_t>(value);
      return true;
    case ComponentType::Float32:
      m_data_float32.at(c)[offset] = static_cast<float>(value);
      return true;
    default:
      return false;
    }

    return false;
  }

  template<typename T>
  void setAllValues(T v)
  {
    switch (m_header.memoryComponentType())
    {
    case ComponentType::Int8:
    {
      for (auto& C : m_data_int8)
      {
        std::fill(std::begin(C), std::end(C), v);
      }
      return;
    }
    case ComponentType::UInt8:
    {
      for (auto& C : m_data_uint8)
      {
        std::fill(std::begin(C), std::end(C), v);
      }
      return;
    }
    case ComponentType::Int16:
    {
      for (auto& C : m_data_int16)
      {
        std::fill(std::begin(C), std::end(C), v);
      }
      return;
    }
    case ComponentType::UInt16:
    {
      for (auto& C : m_data_uint16)
      {
        std::fill(std::begin(C), std::end(C), v);
      }
      return;
    }
    case ComponentType::Int32:
    {
      for (auto& C : m_data_int32)
      {
        std::fill(std::begin(C), std::end(C), v);
      }
      return;
    }
    case ComponentType::UInt32:
    {
      for (auto& C : m_data_uint32)
      {
        std::fill(std::begin(C), std::end(C), v);
      }
      return;
    }
    case ComponentType::Float32:
    {
      for (auto& C : m_data_float32)
      {
        std::fill(std::begin(C), std::end(C), v);
      }
      return;
    }
    default:
      return;
    }
  }

  QuantileOfValue valueToQuantile(uint32_t component, int64_t value) const;
  QuantileOfValue valueToQuantile(uint32_t component, double value) const;

  double quantileToValue(uint32_t comp, double quantile) const;

  void setUseIdentityPixelSpacings(bool identitySpacings);
  bool getUseIdentityPixelSpacings() const;

  void setUseZeroPixelOrigin(bool zeroOrigin);
  bool getUseZeroPixelOrigin() const;

  void setUseIdentityPixelDirections(bool identityDirections);
  bool getUseIdentityPixelDirections() const;

  void setSnapToClosestOrthogonalPixelDirections(bool snap);
  bool getSnapToClosestOrthogonalPixelDirections() const;

  void setHeaderOverrides(const ImageHeaderOverrides& overrides);
  const ImageHeaderOverrides& getHeaderOverrides() const;

  /// @brief Get the image header
  const ImageHeader& header() const;
  ImageHeader& header();

  /// @brief Get the image transformations
  const ImageTransformations& transformations() const;
  ImageTransformations& transformations();

  /// @brief Get the image settings
  const ImageSettings& settings() const;
  ImageSettings& settings();

  /// @brief Get the image meta data
  std::ostream& metaData(std::ostream& os) const;

  void updateComponentStats();

private:
  /// Load a buffer as an image component
  bool loadImageBuffer(
    const void* buffer,
    std::size_t numElements,
    const itk::IOComponentEnum& srcComponentType,
    const itk::IOComponentEnum& dstComponentType
  );

  /// Load a buffer as a segmentation component
  bool loadSegBuffer(
    const void* buffer,
    std::size_t numElements,
    const itk::IOComponentEnum& srcComponentType,
    const itk::IOComponentEnum& dstComponentType
  );

  /// For a given image component and 3D pixel indices, return a pair consisting of:
  /// 1) component buffer to index
  /// 2) offset into that buffer
  std::optional<std::pair<std::size_t, std::size_t>> getComponentAndOffsetForBuffer(
    uint32_t comp, int i, int j, int k
  ) const;

  /// For a given image component and 1D pixel index, return a pair consisting of:
  /// 1) component buffer to index
  /// 2) offset into that buffer
  std::optional<std::pair<std::size_t, std::size_t>> getComponentAndOffsetForBuffer(
    uint32_t comp, std::size_t index
  ) const;

  /**
     * @remark If the image has a multi-component pixels and m_bufferType == MultiComponentBufferType::SeparateImages,
     * then its components are separated and stored in a vector of images. This is so that the buffer to each image
     * component can be retrieved independently of the others, as required when setting an OpenGL texture.
     * If the components were not separated, then the original buffer would be accessed as a 1-D array with
     * interleaved components: buffer[c + numComponents * (x + xSize * (y + ySize * z))];
     * where c is the desired component.
     *
     * @remark if m_bufferType == MultiComponentBufferType::InterleavedImage then only the 0th component is used to
     * hold all components
    */

  std::vector<std::vector<int8_t>> m_data_int8;
  std::vector<std::vector<uint8_t>> m_data_uint8;
  std::vector<std::vector<int16_t>> m_data_int16;
  std::vector<std::vector<uint16_t>> m_data_uint16;
  std::vector<std::vector<int32_t>> m_data_int32;
  std::vector<std::vector<uint32_t>> m_data_uint32;
  std::vector<std::vector<float>> m_data_float32;

  /// @note These vectors separate out interleaved pixels into separate vectors for multi-component images
  /// (regardless of m_bufferType)
  std::vector<std::vector<int8_t>> m_dataSorted_int8;
  std::vector<std::vector<uint8_t>> m_dataSorted_uint8;
  std::vector<std::vector<int16_t>> m_dataSorted_int16;
  std::vector<std::vector<uint16_t>> m_dataSorted_uint16;
  std::vector<std::vector<int32_t>> m_dataSorted_int32;
  std::vector<std::vector<uint32_t>> m_dataSorted_uint32;
  std::vector<std::vector<float>> m_dataSorted_float32;

  ImageRepresentation m_imageRep;        //!< Is this an image or a segmentation?
  MultiComponentBufferType m_bufferType; //!< How are multi-component images represented?

  ImageIoInfo m_ioInfoOnDisk;   //!< Info about image as stored on disk
  ImageIoInfo m_ioInfoInMemory; //!< Info about image as loaded into memory

  ImageHeader m_header;
  ImageHeaderOverrides m_headerOverrides;
  ImageTransformations m_tx;
  ImageSettings m_settings;
};

#endif // IMAGE_H
