#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

class Image;

namespace image_io
{
/// @brief Categories of failure returned by the image writer.
enum class WriteError : std::uint8_t
{
  None,
  EmptyPath,
  MissingPixelData,
  InvalidComponent,
  InvalidTimePoint,
  IrregularTimeAxis,
  UnsupportedComponentType,
  UnsupportedFormat,
  InvalidImageData,
  IoFailure
};

/// @brief Result of an image export operation, including a user-readable failure reason.
struct WriteResult
{
  WriteError error = WriteError::None; //!< Machine-readable result category
  std::string message;                 //!< User-readable failure detail; empty on success

  /// @brief Return true when the image was written successfully.
  [[nodiscard]] explicit operator bool() const
  {
    return WriteError::None == error;
  }
};

/**
 * @brief Options controlling which part of an image is exported.
 *
 * By default, all components and all time points are written. Selecting a component produces a
 * scalar image. Selecting a time point omits the time dimension from the output.
 */
struct WriteOptions
{
  bool useCompression = true;             //!< Ask the selected writer to use compression
  std::optional<std::uint32_t> component; //!< Optional single component to write
  std::optional<std::uint32_t> timePoint; //!< Optional single time point to write
};

/**
 * @brief Export an Entropy image using the writer selected by the destination extension.
 * @param image Image whose loaded in-memory pixels and effective header geometry are exported.
 * @param destination Output file path. The extension selects the file format.
 * @param options Component, time-point, and compression choices.
 * @return Structured success or failure information. This function never changes the source path
 * stored by p image.
 */
[[nodiscard]] WriteResult
writeImage(const Image& image, const std::filesystem::path& destination, const WriteOptions& options = {});
} // namespace image_io
