#pragma once

#include "logic/app/Data.h"

void createNoiseEstimates(const Image& image, const uuids::uuid& imageUid, AppData& data);

// Compute the distance maps to foreground of all image components
void createDistanceMaps(const Image& image, const uuids::uuid& imageUid,
                        float downsamplingFactor, AppData& data);
