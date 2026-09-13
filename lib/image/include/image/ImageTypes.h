#pragma once

/**
 * @brief Pixel loading state for an image record.
 */
enum class ImageLoadState
{
  HeaderOnly,    //!< Header metadata is available, but pixel buffers have not been loaded
  LoadingPixels, //!< Pixel loading is in progress
  LoadedPixels,  //!< Pixel buffers are loaded and available
  Failed,        //!< Loading failed
  Skipped        //!< Pixel loading was intentionally skipped
};

/**
 * @brief Semantic role of an image.
 */
enum class ImageRepresentation
{
  Image,       //!< A scalar or vector intensity image
  Segmentation //!< A label image
};

/**
 * @brief Storage layout for multi-component image buffers.
 */
enum class MultiComponentBufferType
{
  SeparateImages,  //!< Each component is stored in a separate image buffer
  InterleavedImage //!< Components are interleaved in one image buffer
};
