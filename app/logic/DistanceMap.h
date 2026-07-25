#pragma once

#include "logic/app/Data.h"

/**
 * @brief Create per-component noise estimate images and store them in application data
 * @param image Source image used to compute the estimates
 * @param imageUid UID of `image` in `data`
 * @param data Application data that receives the generated noise estimate images
 * @throw Propagates exceptions from image-derived-data creation or storage
 */
void createNoiseEstimates(const Image& image, const uuids::uuid& imageUid, AppData& data);

/**
 * @brief Create foreground distance maps for all image components and store them in application data
 * @param image Source image used to compute distance maps
 * @param imageUid UID of `image` in `data`
 * @param downsamplingFactor Scale factor used while generating each distance map
 * @param data Application data that receives the generated distance maps
 * @throw Propagates exceptions from image-derived-data creation or storage
 */
void createDistanceMaps(const Image& image, const uuids::uuid& imageUid, float downsamplingFactor, AppData& data);
