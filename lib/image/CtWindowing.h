#pragma once

#include "common/Types.h"
#include "image/ImageIoInfo.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

class Image;

namespace ct_windowing
{

enum class DetectionConfidence
{
  Unknown,
  Likely,
  Confirmed
};

struct DetectionInput
{
  const MetaDataMap* metadata = nullptr;
  PixelType pixelType = PixelType::Undefined;
  ComponentType componentType = ComponentType::Undefined;
  std::uint32_t numComponents = 0;
  std::optional<std::pair<double, double>> valueRange;
};

struct Detection
{
  DetectionConfidence confidence = DetectionConfidence::Unknown;
  bool angiography = false;
  std::string evidence;

  bool isCt() const;
};

enum class PresetSource
{
  Dicom,
  BuiltIn
};

struct WindowPreset
{
  std::string name;
  double width = 1.0;
  double level = 0.0;
  PresetSource source = PresetSource::BuiltIn;
};

/** Detect CT image data using explicit metadata first and conservative pixel-data evidence second. */
Detection detect(const DetectionInput& input);

/** Detect whether an Entropy image contains CT image data expressed in Hounsfield-like values. */
Detection detect(const Image& image);

/** Parse paired DICOM Window Center/Width values and their optional descriptions. */
std::vector<WindowPreset> dicomPresets(const MetaDataMap& metadata);

/** Return Entropy's built-in clinical CT and CTA windowing starting points. */
std::span<const WindowPreset> builtInPresets();

} // namespace ct_windowing
