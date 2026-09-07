#include "image/CtWindowing.h"

#include "image/Image.h"
#include "image/ImageSettings.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <type_traits>

namespace ct_windowing
{
namespace
{

std::string canonicalKey(std::string_view value)
{
  std::string result;
  result.reserve(value.size());
  for (const unsigned char ch : value) {
    if (std::isalnum(ch)) {
      result.push_back(static_cast<char>(std::tolower(ch)));
    }
  }
  return result;
}

std::string trim(std::string value)
{
  const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch); });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch); });
  if (first >= last.base()) {
    return {};
  }
  return std::string(first, last.base());
}

std::vector<std::string> splitDicomText(const std::string& value)
{
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start <= value.size()) {
    const std::size_t end = value.find('\\', start);
    parts.push_back(trim(value.substr(start, end == std::string::npos ? std::string::npos : end - start)));
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return parts;
}

std::string metadataText(const MetaDataMap::mapped_type& value)
{
  return std::visit(
    [](const auto& data) -> std::string {
      using Value = std::decay_t<decltype(data)>;
      if constexpr (std::is_same_v<Value, std::string>) {
        return data;
      }
      else if constexpr (std::is_same_v<Value, std::vector<char>>) {
        return std::string(data.begin(), data.end());
      }
      else {
        return {};
      }
    },
    value);
}

std::vector<double> metadataNumbers(const MetaDataMap::mapped_type& value)
{
  return std::visit(
    [](const auto& data) -> std::vector<double> {
      using Value = std::decay_t<decltype(data)>;
      if constexpr (std::is_arithmetic_v<Value>) {
        return {static_cast<double>(data)};
      }
      else if constexpr (
        std::is_same_v<Value, std::vector<int>> || std::is_same_v<Value, std::vector<float>> ||
        std::is_same_v<Value, std::vector<double>>)
      {
        return std::vector<double>(data.begin(), data.end());
      }
      else if constexpr (std::is_same_v<Value, std::string> || std::is_same_v<Value, std::vector<char>>) {
        const std::string text = [&]() {
          if constexpr (std::is_same_v<Value, std::string>) {
            return data;
          }
          else {
            return std::string(data.begin(), data.end());
          }
        }();

        std::vector<double> numbers;
        for (const std::string& part : splitDicomText(text)) {
          if (part.empty()) {
            continue;
          }
          char* end = nullptr;
          const double number = std::strtod(part.c_str(), &end);
          if (end != part.c_str() && end && *end == '\0' && std::isfinite(number)) {
            numbers.push_back(number);
          }
        }
        return numbers;
      }
      else {
        return {};
      }
    },
    value);
}

const MetaDataMap::mapped_type* findMetadata(const MetaDataMap& metadata, std::initializer_list<std::string_view> keys)
{
  for (const auto& [key, value] : metadata) {
    const std::string canonical = canonicalKey(key);
    if (std::find(keys.begin(), keys.end(), std::string_view{canonical}) != keys.end()) {
      return &value;
    }
  }
  return nullptr;
}

std::string uppercaseWords(std::string value)
{
  for (char& ch : value) {
    const auto byte = static_cast<unsigned char>(ch);
    ch = std::isalnum(byte) ? static_cast<char>(std::toupper(byte)) : ' ';
  }
  return value;
}

bool containsWord(const std::string& text, std::initializer_list<std::string_view> words)
{
  const std::string normalized = uppercaseWords(text);
  const std::string_view normalizedView{normalized};
  std::size_t start = 0;
  while (start < normalized.size()) {
    start = normalized.find_first_not_of(' ', start);
    if (start == std::string::npos) {
      break;
    }
    const std::size_t end = normalized.find(' ', start);
    const std::string_view token =
      normalizedView.substr(start, (end == std::string::npos ? normalized.size() : end) - start);
    if (std::find(words.begin(), words.end(), token) != words.end()) {
      return true;
    }
    start = end == std::string::npos ? normalized.size() : end + 1;
  }
  return false;
}

std::vector<std::string> descriptiveMetadata(const MetaDataMap& metadata)
{
  static constexpr std::array<std::string_view, 7>
    keys{"00081030", "0008103e", "00181030", "studydescription", "seriesdescription", "protocolname", "descrip"};

  std::vector<std::string> descriptions;
  for (const auto& [key, value] : metadata) {
    const std::string canonical = canonicalKey(key);
    if (std::find(keys.begin(), keys.end(), canonical) != keys.end()) {
      const std::string text = metadataText(value);
      if (!text.empty()) {
        descriptions.push_back(text);
      }
    }
  }
  return descriptions;
}

bool isCtSopClass(std::string_view value)
{
  const std::string uid = trim(std::string(value));
  return uid == "1.2.840.10008.5.1.4.1.1.2" || uid == "1.2.840.10008.5.1.4.1.1.2.1" ||
         uid == "1.2.840.10008.5.1.4.1.1.2.2";
}

} // namespace

bool Detection::isCt() const
{
  return confidence != DetectionConfidence::Unknown;
}

Detection detect(const DetectionInput& input)
{
  if (input.pixelType != PixelType::Scalar || input.numComponents != 1u) {
    return {};
  }

  bool angiography = false;
  if (input.metadata) {
    const MetaDataMap& metadata = *input.metadata;
    const auto descriptions = descriptiveMetadata(metadata);
    angiography = std::any_of(descriptions.begin(), descriptions.end(), [](const std::string& text) {
      return containsWord(text, {"CTA", "CTPA", "ANGIO", "ANGIOGRAPHY", "ANGIOGRAM", "ARTERIAL", "RUNOFF"});
    });

    if (const auto* modality = findMetadata(metadata, {"00080060", "dicom00080060", "modality"})) {
      const std::string value = metadataText(*modality);
      if (containsWord(value, {"CT", "CTA"})) {
        return {DetectionConfidence::Confirmed, angiography || containsWord(value, {"CTA"}), "CT modality metadata"};
      }
      if (!trim(value).empty()) {
        return {};
      }
    }

    if (const auto* sopClass = findMetadata(metadata, {"00080016", "dicom00080016", "sopclassuid"});
        sopClass && isCtSopClass(metadataText(*sopClass)))
    {
      return {DetectionConfidence::Confirmed, angiography, "CT DICOM SOP class"};
    }

    if (const auto* units = findMetadata(metadata, {"00281054", "dicom00281054", "rescaletype", "units"})) {
      const std::string value = metadataText(*units);
      if (containsWord(value, {"HU", "HOUNSFIELD"})) {
        return {DetectionConfidence::Confirmed, angiography, "Hounsfield-unit metadata"};
      }
    }

    const bool descriptionSaysCt = std::any_of(descriptions.begin(), descriptions.end(), [](const std::string& text) {
      return containsWord(text, {"CT", "CTA", "CTPA"}) ||
             uppercaseWords(text).find("COMPUTED TOMOGRAPHY") != std::string::npos;
    });
    if (descriptionSaysCt) {
      return {DetectionConfidence::Likely, angiography, "CT description metadata"};
    }
  }

  if (input.valueRange) {
    const auto [minimum, maximum] = *input.valueRange;
    const bool commonCtStorage =
      input.componentType == ComponentType::Int16 || input.componentType == ComponentType::Int32 ||
      input.componentType == ComponentType::Float32 || input.componentType == ComponentType::Float64;
    const bool plausibleHuRange = std::isfinite(minimum) && std::isfinite(maximum) && minimum <= -500.0 &&
                                  maximum >= 100.0 && minimum >= -4096.0 && maximum <= 20000.0;
    if (commonCtStorage && plausibleHuRange) {
      return {DetectionConfidence::Likely, angiography, "Hounsfield-like scalar value range"};
    }
  }

  return {};
}

Detection detect(const Image& image)
{
  std::optional<std::pair<double, double>> range;
  if (image.header().numComponentsPerPixel() == 1u) {
    const OnlineStats& statistics = image.settings().componentStatistics(0).onlineStats;
    if (statistics.count > 0u) {
      range = std::pair<double, double>{static_cast<double>(statistics.min), static_cast<double>(statistics.max)};
    }
  }

  return detect(DetectionInput{
    .metadata = &image.header().metaData(),
    .pixelType = image.header().pixelType(),
    .componentType = image.header().memoryComponentType(),
    .numComponents = image.header().numComponentsPerPixel(),
    .valueRange = range});
}

std::vector<WindowPreset> dicomPresets(const MetaDataMap& metadata)
{
  const auto* centersValue = findMetadata(metadata, {"00281050", "dicom00281050", "windowcenter"});
  const auto* widthsValue = findMetadata(metadata, {"00281051", "dicom00281051", "windowwidth"});
  if (!centersValue || !widthsValue) {
    return {};
  }

  const std::vector<double> centers = metadataNumbers(*centersValue);
  const std::vector<double> widths = metadataNumbers(*widthsValue);
  const auto* explanationsValue = findMetadata(metadata, {"00281055", "dicom00281055", "windowcenterwidthexplanation"});
  const std::vector<std::string> explanations =
    explanationsValue ? splitDicomText(metadataText(*explanationsValue)) : std::vector<std::string>{};

  std::vector<WindowPreset> presets;
  const std::size_t pairCount = std::min(centers.size(), widths.size());
  presets.reserve(pairCount);
  for (std::size_t index = 0; index < pairCount; ++index) {
    if (!std::isfinite(centers[index]) || !std::isfinite(widths[index]) || widths[index] < 1.0) {
      continue;
    }

    const bool duplicate = std::any_of(presets.begin(), presets.end(), [&](const WindowPreset& preset) {
      return std::abs(preset.width - widths[index]) <= std::numeric_limits<double>::epsilon() &&
             std::abs(preset.level - centers[index]) <= std::numeric_limits<double>::epsilon();
    });
    if (duplicate) {
      continue;
    }

    std::string name = index < explanations.size() ? explanations[index] : std::string{};
    if (name.empty()) {
      name = pairCount == 1u ? "DICOM window" : "DICOM window " + std::to_string(index + 1u);
    }
    presets.push_back(WindowPreset{std::move(name), widths[index], centers[index], PresetSource::Dicom});
  }
  return presets;
}

std::span<const WindowPreset> builtInPresets()
{
  static const std::array presets{
    WindowPreset{"Brain", 80.0, 40.0, PresetSource::BuiltIn},
    WindowPreset{"Stroke", 40.0, 40.0, PresetSource::BuiltIn},
    WindowPreset{"Subdural", 200.0, 75.0, PresetSource::BuiltIn},
    WindowPreset{"Soft tissue / abdomen", 400.0, 40.0, PresetSource::BuiltIn},
    WindowPreset{"Liver", 150.0, 30.0, PresetSource::BuiltIn},
    WindowPreset{"Mediastinum", 350.0, 50.0, PresetSource::BuiltIn},
    WindowPreset{"Lung", 1500.0, -600.0, PresetSource::BuiltIn},
    WindowPreset{"Bone", 1800.0, 400.0, PresetSource::BuiltIn},
    WindowPreset{"Temporal bone", 4000.0, 700.0, PresetSource::BuiltIn},
    WindowPreset{"Vascular / CTA", 600.0, 200.0, PresetSource::BuiltIn},
    WindowPreset{"Pulmonary angiography", 700.0, 100.0, PresetSource::BuiltIn},
    WindowPreset{"Coronary CTA", 800.0, 300.0, PresetSource::BuiltIn}};
  return presets;
}

} // namespace ct_windowing
