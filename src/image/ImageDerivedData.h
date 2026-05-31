#pragma once

#include "Image.h"

#include <cstdint>
#include <vector>

struct ComponentImageResult
{
  uint32_t component{0};
  Image image;
};

struct DistanceMapImageResult
{
  uint32_t component{0};
  Image image;
  double boundaryIsoValue{0.0};
};

std::vector<ComponentImageResult> createNoiseEstimateImages(const Image& image, uint32_t radius);

std::vector<DistanceMapImageResult> createDistanceMapImages(const Image& image, float downsamplingFactor);
