#pragma once

#include "common/Types.h"

#include "image/ImageHeader.h"
#include "image/ImageHeaderOverrides.h"
#include "image/ImageIoInfo.h"
#include "image/ImageSettings.h"
#include "image/ImageTimeAxis.h"
#include "image/ImageTransformations.h"
#include "image/ImageTypes.h"
#include "image/external/TDigest.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief Owns image metadata, pixel buffers, derived statistics, and per-image display state.
 *
 * Image supports 1D, 2D, and 3D scalar or multi-component medical images. Pixel buffers may be
 * stored as one buffer per component or as one interleaved buffer, depending on the selected
 * MultiComponentBufferType. The ImageHeader describes the logical image geometry and component
 * type, while ImageIoInfo records how the data was represented on disk and in memory.
 */
class Image
{
public:
  /// @brief Backwards-compatible alias for the top-level image load-state enum.
  using LoadState = ImageLoadState;

  /// @brief Backwards-compatible alias for the top-level image/segmentation representation enum.
  using ImageRepresentation = ::ImageRepresentation;

  /// @brief Backwards-compatible alias for the top-level multi-component buffer layout enum.
  using MultiComponentBufferType = ::MultiComponentBufferType;

  /**
   * @brief Construct Image from a file on disk
   * @param[in] fileName Path to image file
   * @param[in] imageRep Indicates whether this is an image or a segmentation
   * @param[in] bufferType Indicates whether multi-component images are loaded as
   * multiple buffers or as a single buffer with interleaved pixel components
   */
  Image(
    const std::filesystem::path& fileName,
    const ImageRepresentation& imageRep,
    const MultiComponentBufferType& bufferType);

  /**
   * @brief Construct a header-only image record without loading pixel data.
   */
  Image(
    const ImageHeader& header,
    const std::string& displayName,
    const ImageRepresentation& imageRep,
    const MultiComponentBufferType& bufferType);

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
    ImageHeader header,
    const std::string& displayName,
    const ImageRepresentation& imageRep,
    const MultiComponentBufferType& bufferType,
    const std::vector<const void*>& imageDataComponents,
    ImageTimeAxis timeAxis = {});

  Image(const Image&) = default;
  Image& operator=(const Image&) = default;

  Image(Image&&) = default;
  Image& operator=(Image&&) = default;

  ~Image() = default;

  /**
   * @brief Save an image component to disk. If the image is successfully saved and a
   * new file name is provided, then the Image's file name is set to the new file name.
   * @param[in] component Component of the image to save
   * @param[in] newFileName Optional new file name at which to save the image
   * @return True iff the image was saved successfully
   */
  bool saveComponentToDisk(uint32_t component, const std::optional<std::filesystem::path>& newFileName);

  /// @brief Get whether this object contains loaded pixels, only a header, or failed to load.
  LoadState loadState() const;

  /// @brief Update the load-state flag without changing pixel buffers or metadata.
  void setLoadState(LoadState state);

  /// @brief Return true when this image owns pixel buffers that can be sampled or saved.
  bool hasPixelData() const;

  /**
   * @brief Recompute the settings that this image would have immediately after loading.
   *
   * Project snapshots use these settings as the per-image baseline so image-derived defaults, such
   * as window/level and default multi-component rendering, do not have to be written to project JSON.
   */
  ImageSettings defaultSettings() const;

  /// @brief Build sorted per-component buffers used by quantile and histogram queries.
  /// @return True when sorted buffers were generated for the image's memory component type.
  bool generateSortedBuffers();

  /// @brief Return whether this object is interpreted as an intensity image or segmentation.
  const ImageRepresentation& imageRep() const;

  /// @brief Return the selected in-memory layout for multi-component pixel data.
  const MultiComponentBufferType& bufferType() const;

  /**
   * @brief Get a const void pointer to an owned raw buffer.
   * @param[in] component Raw buffer slot to get.
   * @param[in] timePoint Time frame whose first value should be returned.
   * @return Pointer to the raw buffer slot, or nullptr when the slot/time is invalid.
   *
   * For separated buffers, each logical component has its own raw buffer slot. For interleaved
   * buffers, raw slot 0 stores all logical components in pixel-major order, so slot 0 is the only
   * valid raw slot. Prefer value() for logical component access that is independent of buffer
   * layout.
   */
  const void* bufferAsVoid(uint32_t component, uint32_t timePoint = 0) const;

  /// @brief Get a non-const void pointer to the raw buffer data of an image component.
  void* bufferAsVoid(uint32_t component, uint32_t timePoint = 0);

  /**
   * @brief Get a const void pointer to the sorted buffer data of an image component.
   * @param[in] component Image component to get
   * @note Ignores the \c MultiComponentBufferType setting, so that the
   * component must be in the range [0, header().numComponentsPerPixel() - 1]
   */
  const void* bufferSortedAsVoid(uint32_t component) const;

  /// @brief Get a non-const void pointer to the sorted buffer data of an image component.
  void* bufferSortedAsVoid(uint32_t component);

  /**
   * @brief Get a component value at a linear pixel index.
   * @tparam T Requested return type. The stored component value is cast to this type.
   * @param component Logical component index.
   * @param index Linear pixel index in x-fastest order.
   * @return The converted value, or std::nullopt for an invalid index/component/type.
   */
  template<typename T>
  std::optional<T> value(uint32_t component, std::size_t index, uint32_t timePoint = 0) const
  {
    if (index >= m_header.numPixels()) {
      return std::nullopt;
    }

    const auto compAndOffset = getComponentAndOffsetForBuffer(component, index, timePoint);
    if (!compAndOffset) {
      return std::nullopt;
    }

    const std::size_t c = compAndOffset->first;
    const std::size_t offset = compAndOffset->second;

    auto valueFromBuffer = [c, offset](const auto& buffers) -> std::optional<T> {
      if (c >= buffers.size() || offset >= buffers[c].size()) {
        return std::nullopt;
      }
      return static_cast<T>(buffers[c][offset]);
    };

    switch (m_header.memoryComponentType()) {
      case ComponentType::Int8:
        return valueFromBuffer(m_data_int8);
      case ComponentType::UInt8:
        return valueFromBuffer(m_data_uint8);
      case ComponentType::Int16:
        return valueFromBuffer(m_data_int16);
      case ComponentType::UInt16:
        return valueFromBuffer(m_data_uint16);
      case ComponentType::Int32:
        return valueFromBuffer(m_data_int32);
      case ComponentType::UInt32:
        return valueFromBuffer(m_data_uint32);
      case ComponentType::Float32:
        return valueFromBuffer(m_data_float32);
      default:
        return std::nullopt;
    }
  }

  /**
   * @brief Get a component value at a 3D pixel index.
   * @tparam T Requested return type. The stored component value is cast to this type.
   * @return The converted value, or std::nullopt when the index is outside the image.
   */
  template<typename T>
  std::optional<T> value(uint32_t component, int i, int j, int k, uint32_t timePoint = 0) const
  {
    const glm::u64vec3& dims = m_header.pixelDimensions();

    if (
      i < 0 || j < 0 || k < 0 || i >= static_cast<int64_t>(dims.x) || j >= static_cast<int64_t>(dims.y) ||
      k >= static_cast<int64_t>(dims.z))
    {
      return std::nullopt;
    }

    const std::size_t index = dims.x * dims.y * static_cast<std::size_t>(k) + dims.x * static_cast<std::size_t>(j) +
                              static_cast<std::size_t>(i);

    return value<T>(component, index, timePoint);
  }

  /**
   * @brief Linearly sample a component at continuous 3D image coordinates.
   *
   * Coordinates are valid in the half-voxel-extended range [-0.5, N - 0.5]. Valid coordinates are
   * clamped to edge samples before interpolation so edge and corner samples remain well defined.
   *
   * @tparam T Requested return type.
   * @return The interpolated value, or std::nullopt when the coordinate/component cannot be read.
   */
  template<typename T>
  std::optional<T> valueLinear(uint32_t comp, double i, double j, double k, uint32_t timePoint = 0) const
  {
    const glm::dvec3 ZERO{0.0};
    const glm::dvec3 ONE{1.0};
    const glm::u64vec3& dims = m_header.pixelDimensions();

    if (i < -0.5 || j < -0.5 || k < -0.5 || i > dims.x - 0.5 || j > dims.y - 0.5 || k > dims.z - 0.5) {
      return std::nullopt;
    }

    // Valid image coordinates are [-0.5, N-0.5]. However, we clamp coordinates to the edge samples,
    // which are at 0 and N - 1:
    const glm::dvec3 coordClamped = glm::clamp(glm::dvec3{i, j, k}, ZERO, glm::dvec3{dims} - ONE);
    const glm::i64vec3 f = glm::i64vec3{glm::floor(coordClamped)};

    // Get values of all 8 neighboring pixels. If a pixel outside the image is requested,
    // then its sampling location was outside the image and its returned value is std::none
    const int fx = static_cast<int>(f.x);
    const int fy = static_cast<int>(f.y);
    const int fz = static_cast<int>(f.z);

    const auto c000 = value<double>(comp, fx + 0, fy + 0, fz + 0, timePoint);
    const auto c001 = value<double>(comp, fx + 0, fy + 0, fz + 1, timePoint);
    const auto c010 = value<double>(comp, fx + 0, fy + 1, fz + 0, timePoint);
    const auto c011 = value<double>(comp, fx + 0, fy + 1, fz + 1, timePoint);
    const auto c100 = value<double>(comp, fx + 1, fy + 0, fz + 0, timePoint);
    const auto c101 = value<double>(comp, fx + 1, fy + 0, fz + 1, timePoint);
    const auto c110 = value<double>(comp, fx + 1, fy + 1, fz + 0, timePoint);
    const auto c111 = value<double>(comp, fx + 1, fy + 1, fz + 1, timePoint);

    const glm::dvec3 diff = coordClamped - glm::floor(coordClamped);

    // Interpolate along x, ignoring invalid samples:
    std::optional<double> c00, c01, c10, c11;

    if (c000 && c100) {
      c00 = c000.value() * (1.0 - diff.x) + c100.value() * diff.x;
    }
    else if (c000) {
      c00 = c000.value();
    }
    else if (c100) {
      c00 = c100.value();
    }

    if (c001 && c101) {
      c01 = c001.value() * (1.0 - diff.x) + c101.value() * diff.x;
    }
    else if (c001) {
      c01 = c001.value();
    }
    else if (c101) {
      c01 = c101.value();
    }

    if (c010 && c110) {
      c10 = c010.value() * (1.0 - diff.x) + c110.value() * diff.x;
    }
    else if (c010) {
      c10 = c010.value();
    }
    else if (c110) {
      c10 = c110.value();
    }

    if (c011 && c111) {
      c11 = c011.value() * (1.0 - diff.x) + c111.value() * diff.x;
    }
    else if (c011) {
      c11 = c011.value();
    }
    else if (c111) {
      c11 = c111.value();
    }

    // Interpolate along y, ignoring invalid samples:
    std::optional<double> c0, c1;

    if (c00 && c10) {
      c0 = c00.value() * (1.0 - diff.y) + c10.value() * diff.y;
    }
    else if (c00) {
      c0 = c00.value();
    }
    else if (c10) {
      c0 = c10.value();
    }

    if (c01 && c11) {
      c1 = c01.value() * (1.0 - diff.y) + c11.value() * diff.y;
    }
    else if (c01) {
      c1 = c01.value();
    }
    else if (c11) {
      c1 = c11.value();
    }

    // Interpolate along z, ignoring invalid samples:
    std::optional<double> c;

    if (c0 && c1) {
      c = c0.value() * (1.0 - diff.z) + c1.value() * diff.z;
    }
    else if (c0) {
      c = c0.value();
    }
    else if (c1) {
      c = c1.value();
    }

    return c;
  }

  /**
   * @brief Set a component value at a 3D pixel index.
   * @tparam T Input value type. The value is cast to the image memory component type.
   * @return True when the coordinate/component/type is valid and the value was written.
   */
  template<typename T>
  bool setValue(uint32_t component, int i, int j, int k, T value)
  {
    const glm::u64vec3& dims = m_header.pixelDimensions();

    if (
      i < 0 || j < 0 || k < 0 || i >= static_cast<int64_t>(dims.x) || j >= static_cast<int64_t>(dims.y) ||
      k >= static_cast<int64_t>(dims.z))
    {
      return false;
    }

    const auto compAndOffset = getComponentAndOffsetForBuffer(component, i, j, k);
    if (!compAndOffset) {
      return false;
    }

    const std::size_t c = compAndOffset->first;
    const std::size_t offset = compAndOffset->second;

    switch (m_header.memoryComponentType()) {
      case ComponentType::Int8: {
        m_data_int8.at(c)[offset] = static_cast<int8_t>(value);
        return true;
      }
      case ComponentType::UInt8: {
        m_data_uint8.at(c)[offset] = static_cast<uint8_t>(value);
        return true;
      }
      case ComponentType::Int16: {
        m_data_int16.at(c)[offset] = static_cast<int16_t>(value);
        return true;
      }
      case ComponentType::UInt16: {
        m_data_uint16.at(c)[offset] = static_cast<uint16_t>(value);
        return true;
      }
      case ComponentType::Int32: {
        m_data_int32.at(c)[offset] = static_cast<int32_t>(value);
        return true;
      }
      case ComponentType::UInt32: {
        m_data_uint32.at(c)[offset] = static_cast<uint32_t>(value);
        return true;
      }
      case ComponentType::Float32: {
        m_data_float32.at(c)[offset] = static_cast<float>(value);
        return true;
      }
      default:
        return false;
    }
  }

  /// @brief Fill all component buffers with one value.
  /// @tparam T Input value type. The value is cast to the image memory component type.
  template<typename T>
  void setAllValues(T v)
  {
    switch (m_header.memoryComponentType()) {
      case ComponentType::Int8: {
        for (auto& C : m_data_int8) {
          std::fill(std::begin(C), std::end(C), static_cast<int8_t>(v));
        }
        return;
      }
      case ComponentType::UInt8: {
        for (auto& C : m_data_uint8) {
          std::fill(std::begin(C), std::end(C), static_cast<uint8_t>(v));
        }
        return;
      }
      case ComponentType::Int16: {
        for (auto& C : m_data_int16) {
          std::fill(std::begin(C), std::end(C), static_cast<int16_t>(v));
        }
        return;
      }
      case ComponentType::UInt16: {
        for (auto& C : m_data_uint16) {
          std::fill(std::begin(C), std::end(C), static_cast<uint16_t>(v));
        }
        return;
      }
      case ComponentType::Int32: {
        for (auto& C : m_data_int32) {
          std::fill(std::begin(C), std::end(C), static_cast<int32_t>(v));
        }
        return;
      }
      case ComponentType::UInt32: {
        for (auto& C : m_data_uint32) {
          std::fill(std::begin(C), std::end(C), static_cast<uint32_t>(v));
        }
        return;
      }
      case ComponentType::Float32: {
        for (auto& C : m_data_float32) {
          std::fill(std::begin(C), std::end(C), static_cast<float>(v));
        }
        return;
      }
      default:
        return;
    }
  }

  /// @brief Convert an integer component value to its quantile in the image distribution.
  QuantileOfValue valueToQuantile(uint32_t component, int64_t value) const;

  /// @brief Convert a floating point component value to its quantile in the image distribution.
  QuantileOfValue valueToQuantile(uint32_t component, double value) const;

  /// @brief Convert a quantile in [0, 1] to the corresponding component value.
  double quantileToValue(uint32_t comp, double quantile) const;

  /// @brief Override header spacing with unit spacing when computing effective geometry.
  void setUseIdentityPixelSpacings(bool identitySpacings);

  /// @brief Return whether effective geometry uses unit pixel spacing.
  bool getUseIdentityPixelSpacings() const;

  /// @brief Override header origin with zero origin when computing effective geometry.
  void setUseZeroPixelOrigin(bool zeroOrigin);

  /// @brief Return whether effective geometry uses zero pixel origin.
  bool getUseZeroPixelOrigin() const;

  /// @brief Override header directions with identity directions when computing effective geometry.
  void setUseIdentityPixelDirections(bool useIdentity);

  /// @brief Return whether effective geometry uses identity pixel directions.
  bool getUseIdentityPixelDirections() const;

  /// @brief Snap effective pixel directions to the closest orthogonal basis.
  void setSnapToClosestOrthogonalPixelDirections(bool snap);

  /// @brief Return whether effective pixel directions are snapped to an orthogonal basis.
  bool getSnapToClosestOrthogonalPixelDirections() const;

  /// @brief Replace all header override flags in one operation.
  void setHeaderOverrides(const ImageHeaderOverrides& overrides);

  /// @brief Get the current header override flags.
  const ImageHeaderOverrides& getHeaderOverrides() const;

  /// @brief Apply user-provided physical geometry and update dependent transforms.
  void setUserSpatialMetadata(const ImageSpatialMetadata& metadata);

  /// @brief Get read-only access to the image header.
  const ImageHeader& header() const;

  /// @brief Get mutable access to the image header.
  ImageHeader& header();

  /// @brief Return true when this image has more than one time point.
  bool isTimeSeries() const;

  /// @brief Get time-axis metadata.
  const ImageTimeAxis& timeAxis() const;

  /// @brief Get read-only access to affine and display transformations associated with the image.
  const ImageTransformations& transformations() const;

  /// @brief Get mutable access to affine and display transformations associated with the image.
  ImageTransformations& transformations();

  /// @brief Get read-only access to display and interaction settings associated with the image.
  const ImageSettings& settings() const;

  /// @brief Get mutable access to display and interaction settings associated with the image.
  ImageSettings& settings();

  /// @brief Write human-readable image metadata to an output stream.
  /// @return The same output stream for chaining.
  std::ostream& metaData(std::ostream& os) const;

  /// @brief Recompute per-component statistics and quantile summaries from current buffers.
  void updateComponentStats();

private:
  /// @brief Load and optionally cast a buffer as an intensity-image component.
  bool loadImageBuffer(
    const void* buffer,
    std::size_t numElements,
    ComponentType srcComponentType,
    ComponentType dstComponentType);

  /// @brief Load and optionally cast a buffer as a segmentation component.
  bool loadSegBuffer(
    const void* buffer,
    std::size_t numElements,
    ComponentType srcComponentType,
    ComponentType dstComponentType);

  /// @brief Map a logical component and 3D pixel index to an owned buffer index and element offset.
  std::optional<std::pair<std::size_t, std::size_t>> getComponentAndOffsetForBuffer(uint32_t comp, int i, int j, int k)
    const;

  /// @brief Map a logical component and linear pixel index to an owned buffer index and element offset.
  std::optional<std::pair<std::size_t, std::size_t>>
  getComponentAndOffsetForBuffer(uint32_t comp, std::size_t index, uint32_t timePoint = 0) const;

  /// Pixel buffers grouped by component type. For separated layout, the outer vector has one entry
  /// per logical component. For interleaved layout, the outer vector has one entry and stores all
  /// logical components in pixel-major order
  std::vector<std::vector<int8_t>> m_data_int8;
  std::vector<std::vector<uint8_t>> m_data_uint8;
  std::vector<std::vector<int16_t>> m_data_int16;
  std::vector<std::vector<uint16_t>> m_data_uint16;
  std::vector<std::vector<int32_t>> m_data_int32;
  std::vector<std::vector<uint32_t>> m_data_uint32;
  std::vector<std::vector<float>> m_data_float32;

  /// @note These vectors separate out interleaved pixels into separate vectors for multi-component
  /// images (regardless of m_bufferType)
  std::vector<std::vector<int8_t>> m_dataSorted_int8;
  std::vector<std::vector<uint8_t>> m_dataSorted_uint8;
  std::vector<std::vector<int16_t>> m_dataSorted_int16;
  std::vector<std::vector<uint16_t>> m_dataSorted_uint16;
  std::vector<std::vector<int32_t>> m_dataSorted_int32;
  std::vector<std::vector<uint32_t>> m_dataSorted_uint32;
  std::vector<std::vector<float>> m_dataSorted_float32;

  /// One T-digest per image component
  mutable std::vector<tdigest::TDigest> m_tdigests;

  ImageRepresentation m_imageRep;        //!< Is this an image or a segmentation?
  MultiComponentBufferType m_bufferType; //!< How are multi-component images represented?

  ImageIoInfo m_ioInfoOnDisk;   //!< Info about image as stored on disk
  ImageIoInfo m_ioInfoInMemory; //!< Info about image as loaded into memory
  ImageTimeAxis m_timeAxis;     //!< Time-axis metadata

  ImageHeader m_header;
  ImageHeaderOverrides m_headerOverrides;
  ImageTransformations m_tx;
  ImageSettings m_settings;
  LoadState m_loadState = LoadState::LoadedPixels;
};
