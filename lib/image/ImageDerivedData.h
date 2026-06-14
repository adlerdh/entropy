#pragma once

#include "image/Image.h"

#include <cstdint>
#include <vector>

/**
 * @brief Derived image produced for one source image component.
 */
struct ComponentImageResult
{
  uint32_t component{0}; //!< Source image component used to compute the derived image.
  Image image;           //!< Derived scalar image.
};

/**
 * @brief Distance-map image produced for one source image component.
 */
struct DistanceMapImageResult
{
  uint32_t component{0};        //!< Source image component used to compute the distance map.
  Image image;                  //!< Derived scalar distance-map image.
  double boundaryIsoValue{0.0}; //!< Isovalue corresponding to the foreground boundary in the distance map.
};

/**
 * @brief Create one noise-estimate image for each supported component of an image.
 * @param image Source image.
 * @param radius Neighborhood radius, in voxels, used by the noise-estimation filter.
 * @return Derived images paired with their source component indices.
 */
std::vector<ComponentImageResult> createNoiseEstimateImages(const Image& image, uint32_t radius);

/**
 * @brief Create Euclidean distance maps for supported components of an image.
 * @param image Source image.
 * @param downsamplingFactor Factor applied before distance-map computation.
 * @return Distance-map images paired with their source component indices and boundary isovalues.
 */
std::vector<DistanceMapImageResult> createDistanceMapImages(const Image& image, float downsamplingFactor);
