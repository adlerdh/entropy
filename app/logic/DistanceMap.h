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
