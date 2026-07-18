#pragma once

/**
 * @file ImageUtility.tpp
 * @brief Compatibility include for image utility template implementations.
 *
 * The template implementations are split by responsibility into smaller internal headers. Include
 * this file from callers that need the full set of image utility templates.
 */

#include "common/Exception.hpp"
#include "common/Types.h"
#include "../Image.h"

#include "../external/TDigest.h"

#include <itkBinaryThresholdImageFilter.h>
#include <itkCastImageFilter.h>
#include <itkClampImageFilter.h>
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionIterator.h>
#include <itkImageToHistogramFilter.h>
#include <itkImportImageFilter.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkNoiseImageFilter.h>
#include <itkResampleImageFilter.h>
#include <itkSignedMaurerDistanceMapImageFilter.h>
#include <itkStatisticsImageFilter.h>
#include <itkVectorImage.h>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/std.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <vector>

// Without undefining min and max, there are some errors compiling in Visual Studio.
#undef min
#undef max

#define DEBUG_IMAGE_OUTPUT 0

#include "ImageUtilityItkImageConstruction.tpp"
#include "ImageUtilityStatistics.tpp"
#include "ImageUtilityItkIo.tpp"
#include "ImageUtilityDerivedImages.tpp"
#include "ImageUtilityLoad.tpp"
