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

/// @brief Confidence assigned to automatic CT image detection
enum class DetectionConfidence
{
  Unknown,  //!< Available evidence does not identify the image as CT
  Likely,   //!< Pixel values or descriptive metadata are consistent with CT
  Confirmed //!< Authoritative metadata identifies the image as CT
};

/// @brief Image metadata and pixel characteristics used to detect CT data
struct DetectionInput
{
  const MetaDataMap* metadata = nullptr;                  //!< Optional image or DICOM metadata
  PixelType pixelType = PixelType::Undefined;             //!< Logical pixel organization
  ComponentType componentType = ComponentType::Undefined; //!< In-memory scalar component type
  std::uint32_t numComponents = 0;                        //!< Number of components per pixel
  std::optional<std::pair<double, double>> valueRange;    //!< Optional finite minimum and maximum image values
};

/// @brief Result of CT detection, including its confidence and supporting evidence
struct Detection
{
  DetectionConfidence confidence = DetectionConfidence::Unknown; //!< Strength of the CT classification
  bool angiography = false;                                      //!< Whether metadata indicates CT angiography
  std::string evidence; //!< Human-readable description of the decisive evidence

  /// @brief Return true when the available evidence identifies the image as likely or confirmed CT
  bool isCt() const;
};

/// @brief Origin of a CT windowing preset
enum class PresetSource
{
  Dicom,  //!< Preset read from image DICOM metadata
  BuiltIn //!< Preset supplied by Entropy
};

/// @brief Named CT window expressed as width and level in native image-value units
struct WindowPreset
{
  std::string name;                            //!< User-facing preset name
  double width = 1.0;                          //!< Window width
  double level = 0.0;                          //!< Window center
  PresetSource source = PresetSource::BuiltIn; //!< Source of the preset
};

/// @brief Detect CT image data using explicit metadata first and conservative pixel-data evidence second
/// @param input Metadata and pixel characteristics to evaluate
/// @return Detection confidence, angiography classification, and supporting evidence
Detection detect(const DetectionInput& input);

/// @brief Detect whether an Entropy image contains CT image data expressed in Hounsfield-like values
/// @param image Image whose metadata and scalar-value range are evaluated
/// @return Detection confidence, angiography classification, and supporting evidence
Detection detect(const Image& image);

/// @brief Parse paired DICOM Window Center/Width values and their optional descriptions
/// @param metadata Metadata containing DICOM window center, width, and optional explanation values
/// @return Valid, unique presets in metadata order, with missing or malformed pairs omitted
std::vector<WindowPreset> dicomPresets(const MetaDataMap& metadata);

/// @brief Return Entropy's built-in clinical CT and CTA windowing starting points
/// @return Non-owning view of immutable presets with static storage duration
std::span<const WindowPreset> builtInPresets();

} // namespace ct_windowing
