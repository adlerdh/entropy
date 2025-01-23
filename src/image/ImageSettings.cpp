#include "image/ImageSettings.h"
#include "image/ImageUtility.h"

#include "common/Exception.hpp"
#include "common/Types.h"

#include <spdlog/spdlog.h>

#include <limits>

#undef min
#undef max

ImageSettings::ImageSettings(
  std::string displayName,
  std::size_t numPixels,
  uint32_t numComponents,
  ComponentType componentType,
  std::vector<ComponentStats> componentStats
)
  : m_displayName(std::move(displayName))
  , m_globalVisibility(true)
  , m_globalOpacity(1.0f)
  , m_borderColor{1.0f, 0.0f, 1.0f}
  , m_lockedToReference{true}
  , m_displayAsColor{false}
  , m_ignoreAlpha{false}
  , m_colorInterpolationMode{InterpolationMode::Trilinear}
  , m_useDistanceMapForRaycasting{false}
  , m_isosurfacesVisible{true}
  , m_applyImageColormapToIsosurfaces{false}
  , m_showIsosurfacesIn2d{true}
  , m_isosurfaceWidthIn2d{2.5}
  , m_isosurfaceOpacityModulator{1.0f}
  , m_numPixels(numPixels)
  , m_numComponents(numComponents)
  , m_componentType(std::move(componentType))
  , m_componentSettings(numComponents)
  , m_activeComponent(0)
{
  if (0 == m_numPixels)
  {
    spdlog::error("Zero pixels is invalid when constructing settings for image {}", displayName);
    throw_debug("Invalid number of pixels provided to construct settings for image")
  }

  if (componentStats.size() != m_numComponents)
  {
    spdlog::error(
      "Invalid number of components ({}) provided to construct settings for image {}",
      numComponents,
      displayName
    );
    throw_debug("Invalid number of components provided to construct settings for image")
  }

  static constexpr bool sk_setDefaultVisibilitySettings = true;
  updateWithNewComponentStatistics(componentStats, sk_setDefaultVisibilitySettings);
}

void ImageSettings::setDisplayName(std::string name)
{
  m_displayName = std::move(name);
}
const std::string& ImageSettings::displayName() const
{
  return m_displayName;
}

ComponentType ImageSettings::componentType() const
{
  return m_componentType;
}

void ImageSettings::setBorderColor(glm::vec3 borderColor)
{
  m_borderColor = std::move(borderColor);
}
const glm::vec3& ImageSettings::borderColor() const
{
  return m_borderColor;
}

void ImageSettings::setLockedToReference(bool locked)
{
  m_lockedToReference = locked;
}
bool ImageSettings::isLockedToReference() const
{
  return m_lockedToReference;
}

void ImageSettings::setDisplayImageAsColor(bool doColor)
{
  m_displayAsColor = doColor;
}
bool ImageSettings::displayImageAsColor() const
{
  return m_displayAsColor;
}

void ImageSettings::setIgnoreAlpha(bool ignore)
{
  m_ignoreAlpha = ignore;
}
bool ImageSettings::ignoreAlpha() const
{
  return m_ignoreAlpha;
}

void ImageSettings::setColorInterpolationMode(InterpolationMode mode)
{
  m_colorInterpolationMode = mode;
}
InterpolationMode ImageSettings::colorInterpolationMode() const
{
  return m_colorInterpolationMode;
}

void ImageSettings::setUseDistanceMapForRaycasting(bool use)
{
  m_useDistanceMapForRaycasting = use;
}
bool ImageSettings::useDistanceMapForRaycasting() const
{
  return m_useDistanceMapForRaycasting;
}

void ImageSettings::setIsosurfacesVisible(bool visible)
{
  m_isosurfacesVisible = visible;
}
bool ImageSettings::isosurfacesVisible() const
{
  return m_isosurfacesVisible;
}

void ImageSettings::setApplyImageColormapToIsosurfaces(bool apply)
{
  m_applyImageColormapToIsosurfaces = apply;
}
bool ImageSettings::applyImageColormapToIsosurfaces() const
{
  return m_applyImageColormapToIsosurfaces;
}

void ImageSettings::setShowIsosurfacesIn2d(bool show)
{
  m_showIsosurfacesIn2d = show;
}
bool ImageSettings::showIsosurfacesIn2d() const
{
  return m_showIsosurfacesIn2d;
}

void ImageSettings::setIsosurfaceWidthIn2d(double width)
{
  m_isosurfaceWidthIn2d = width;
}
double ImageSettings::isosurfaceWidthIn2d() const
{
  return m_isosurfaceWidthIn2d;
}

void ImageSettings::setIsosurfaceOpacityModulator(float opacityMod)
{
  m_isosurfaceOpacityModulator = opacityMod;
}
float ImageSettings::isosurfaceOpacityModulator() const
{
  return m_isosurfaceOpacityModulator;
}

std::pair<double, double> ImageSettings::minMaxImageRange(uint32_t i) const
{
  return m_componentSettings[i].m_minMaxImageRange;
}
std::pair<double, double> ImageSettings::minMaxImageRange() const
{
  return minMaxImageRange(m_activeComponent);
}

std::pair<double, double> ImageSettings::minMaxWindowWidthRange(uint32_t i) const
{
  return m_componentSettings[i].m_minMaxWindowWidthRange;
}
std::pair<double, double> ImageSettings::minMaxWindowWidthRange() const
{
  return minMaxWindowWidthRange(m_activeComponent);
}

std::pair<double, double> ImageSettings::minMaxWindowCenterRange(uint32_t i) const
{
  return m_componentSettings[i].m_minMaxWindowCenterRange;
}
std::pair<double, double> ImageSettings::minMaxWindowCenterRange() const
{
  return minMaxWindowCenterRange(m_activeComponent);
}

std::pair<double, double> ImageSettings::minMaxWindowRange(uint32_t i) const
{
  return {
    minMaxWindowCenterRange(i).first - 0.5 * minMaxWindowWidthRange(i).second,
    minMaxWindowCenterRange(i).second + 0.5 * minMaxWindowWidthRange(i).second
  };
}

std::pair<double, double> ImageSettings::minMaxWindowRange() const
{
  return minMaxWindowRange(m_activeComponent);
}

std::pair<double, double> ImageSettings::minMaxThresholdRange(uint32_t i) const
{
  return m_componentSettings[i].m_minMaxThresholdRange;
}
std::pair<double, double> ImageSettings::minMaxThresholdRange() const
{
  return minMaxThresholdRange(m_activeComponent);
}

void ImageSettings::setWindowValueLow(uint32_t i, double wLow, bool clampValues)
{
  const double wHigh = windowValuesLowHigh(i).second;
  const double narrowestWidth = minMaxWindowWidthRange(i).first;
  const double wMin = minMaxWindowRange(i).first;
  const double wMax = minMaxWindowRange(i).second;
  double wLowAdjusted = wLow;

  if (!clampValues)
  {
    if (wLow > (wHigh - narrowestWidth) || wLow < wMin || wMax < wLow)
    {
      return;
    }
  }
  else
  {
    wLowAdjusted = std::clamp(std::min(wLowAdjusted, wHigh - narrowestWidth), wMin, wMax);
  }

  const double center = 0.5 * (wLowAdjusted + wHigh);
  const double width = wHigh - wLowAdjusted;

  setWindowCenter(i, center);
  setWindowWidth(i, width);

  // Get the newly set window low/high values and convert to percentiles:
  // const std::pair<double, double> newWindowLowHigh = windowValuesLowHigh(i);
}

void ImageSettings::setWindowValueHigh(uint32_t i, double wHigh, bool clampValues)
{
  const double wLow = windowValuesLowHigh(i).first;
  const double wMin = minMaxWindowRange(i).first;
  const double wMax = minMaxWindowRange(i).second;
  const double narrowestWidth = minMaxWindowWidthRange(i).first;
  double wHighAdjusted = wHigh;

  if (!clampValues)
  {
    if (wHigh < (wLow + narrowestWidth) || wHigh < wMin || wMax < wHigh)
    {
      return;
    }
  }
  else
  {
    wHighAdjusted = std::clamp(std::max(wHighAdjusted, wLow + narrowestWidth), wMin, wMax);
  }

  const double center = 0.5 * (wLow + wHighAdjusted);
  const double width = wHighAdjusted - wLow;

  setWindowCenter(i, center);
  setWindowWidth(i, width);

  // Get the newly set window low/high values and convert to percentiles:
  // const std::pair<double, double> newWindowLowHigh = windowValuesLowHigh(i);
}

void ImageSettings::setWindowValueLow(double wLow, bool clampValues)
{
  setWindowValueLow(m_activeComponent, wLow, clampValues);
}
void ImageSettings::setWindowValueHigh(double wHigh, bool clampValues)
{
  setWindowValueHigh(m_activeComponent, wHigh, clampValues);
}

std::pair<double, double> ImageSettings::windowValuesLowHigh(uint32_t i) const
{
  return {windowCenter(i) - 0.5 * windowWidth(i), windowCenter(i) + 0.5 * windowWidth(i)};
}

std::pair<double, double> ImageSettings::windowValuesLowHigh() const
{
  return windowValuesLowHigh(m_activeComponent);
}

void ImageSettings::setWindowQuantileLow(uint32_t i, double pLow, bool clampValues)
{
  std::ignore = i;
  std::ignore = pLow;
  std::ignore = clampValues;
}

void ImageSettings::setWindowQuantileHigh(uint32_t i, double pHigh, bool clampValues)
{
  std::ignore = i;
  std::ignore = pHigh;
  std::ignore = clampValues;
}

void ImageSettings::setWindowQuantileLow(double pLow, bool clampValues)
{
  setWindowQuantileLow(m_activeComponent, pLow, clampValues);
}
void ImageSettings::setWindowQuantileHigh(double pHigh, bool clampValues)
{
  setWindowQuantileHigh(m_activeComponent, pHigh, clampValues);
}

std::pair<double, double> ImageSettings::windowQuantilesLowHigh(uint32_t i) const
{
  return m_componentSettings[i].m_windowQuantilesLowHigh;
}

std::pair<double, double> ImageSettings::windowQuantilesLowHigh() const
{
  return windowQuantilesLowHigh(m_activeComponent);
}

double ImageSettings::windowWidth(uint32_t i) const
{
  return m_componentSettings[i].m_windowWidth;
}
double ImageSettings::windowWidth() const
{
  return windowWidth(m_activeComponent);
}

double ImageSettings::windowCenter(uint32_t i) const
{
  return m_componentSettings[i].m_windowCenter;
}
double ImageSettings::windowCenter() const
{
  return windowCenter(m_activeComponent);
}

void ImageSettings::setWindowWidth(uint32_t i, double width)
{
  m_componentSettings[i].m_windowWidth
    = std::clamp(width, minMaxWindowWidthRange(i).first, minMaxWindowWidthRange(i).second);
  updateInternals();
}

void ImageSettings::setWindowWidth(double width)
{
  setWindowWidth(m_activeComponent, width);
}

void ImageSettings::setWindowCenter(uint32_t i, double center)
{
  m_componentSettings[i].m_windowCenter
    = std::clamp(center, minMaxWindowCenterRange(i).first, minMaxWindowCenterRange(i).second);
  updateInternals();
}

void ImageSettings::setWindowCenter(double center)
{
  setWindowCenter(m_activeComponent, center);
}

void ImageSettings::setThresholdLow(uint32_t i, double tLow)
{
  if (tLow <= m_componentSettings[i].m_thresholds.second)
  {
    m_componentSettings[i].m_thresholds.first
      = std::max(tLow, m_componentSettings[i].m_minMaxThresholdRange.first);
  }
}

void ImageSettings::setThresholdHigh(uint32_t i, double tHigh)
{
  if (m_componentSettings[i].m_thresholds.first <= tHigh)
  {
    m_componentSettings[i].m_thresholds.second
      = std::min(tHigh, m_componentSettings[i].m_minMaxThresholdRange.second);
  }
}

void ImageSettings::setThresholdLow(double tLow)
{
  setThresholdLow(m_activeComponent, tLow);
}
void ImageSettings::setThresholdHigh(double tHigh)
{
  setThresholdHigh(m_activeComponent, tHigh);
}

std::pair<double, double> ImageSettings::thresholds(uint32_t i) const
{
  return m_componentSettings[i].m_thresholds;
}
std::pair<double, double> ImageSettings::thresholds() const
{
  return thresholds(m_activeComponent);
}

bool ImageSettings::thresholdsActive(uint32_t i) const
{
  const auto& S = m_componentSettings[i];

  return (
    S.m_minMaxThresholdRange.first < S.m_thresholds.first
    || S.m_thresholds.second < S.m_minMaxThresholdRange.second
  );
}

bool ImageSettings::thresholdsActive() const
{
  return thresholdsActive(m_activeComponent);
}

void ImageSettings::setForegroundThresholdLow(uint32_t i, double fgThreshLow)
{
  m_componentSettings[i].m_foregroundThresholds.first = fgThreshLow;
}
void ImageSettings::setForegroundThresholdLow(double fgThreshLow)
{
  setForegroundThresholdLow(m_activeComponent, fgThreshLow);
}
void ImageSettings::setForegroundThresholdHigh(uint32_t i, double fgThreshHigh)
{
  m_componentSettings[i].m_foregroundThresholds.second = fgThreshHigh;
}
void ImageSettings::setForegroundThresholdHigh(double fgThreshHigh)
{
  setForegroundThresholdHigh(m_activeComponent, fgThreshHigh);
}
std::pair<double, double> ImageSettings::foregroundThresholds(uint32_t i) const
{
  return m_componentSettings[i].m_foregroundThresholds;
}
std::pair<double, double> ImageSettings::foregroundThresholds() const
{
  return foregroundThresholds(m_activeComponent);
}

void ImageSettings::setOpacity(uint32_t i, double op)
{
  m_componentSettings[i].m_opacity = std::max(std::min(op, 1.0), 0.0);
}
void ImageSettings::setOpacity(double op)
{
  setOpacity(m_activeComponent, op);
}
double ImageSettings::opacity(uint32_t i) const
{
  return m_componentSettings[i].m_opacity;
}
double ImageSettings::opacity() const
{
  return opacity(m_activeComponent);
}

void ImageSettings::setVisibility(uint32_t i, bool visible)
{
  m_componentSettings[i].m_visible = visible;
}
void ImageSettings::setVisibility(bool visible)
{
  setVisibility(m_activeComponent, visible);
}
bool ImageSettings::visibility(uint32_t i) const
{
  return m_componentSettings[i].m_visible;
}
bool ImageSettings::visibility() const
{
  return visibility(m_activeComponent);
}

void ImageSettings::setGlobalVisibility(bool visible)
{
  m_globalVisibility = visible;
}
bool ImageSettings::globalVisibility() const
{
  return m_globalVisibility;
}

void ImageSettings::setGlobalOpacity(double opacity)
{
  m_globalOpacity = static_cast<float>(std::max(std::min(opacity, 1.0), 0.0));
}

double ImageSettings::globalOpacity() const
{
  return m_globalOpacity;
}

void ImageSettings::setShowEdges(uint32_t i, bool show)
{
  m_componentSettings[i].m_showEdges = show;
}
void ImageSettings::setShowEdges(bool show)
{
  setShowEdges(m_activeComponent, show);
}

bool ImageSettings::showEdges(uint32_t i) const
{
  return m_componentSettings[i].m_showEdges;
}
bool ImageSettings::showEdges() const
{
  return showEdges(m_activeComponent);
}

void ImageSettings::setThresholdEdges(uint32_t i, bool threshold)
{
  m_componentSettings[i].m_thresholdEdges = threshold;
}
void ImageSettings::setThresholdEdges(bool threshold)
{
  setThresholdEdges(m_activeComponent, threshold);
}

bool ImageSettings::thresholdEdges(uint32_t i) const
{
  return m_componentSettings[i].m_thresholdEdges;
}
bool ImageSettings::thresholdEdges() const
{
  return thresholdEdges(m_activeComponent);
}

void ImageSettings::setUseFreiChen(uint32_t i, bool use)
{
  m_componentSettings[i].m_useFreiChen = use;
}
void ImageSettings::setUseFreiChen(bool use)
{
  setUseFreiChen(m_activeComponent, use);
}

bool ImageSettings::useFreiChen(uint32_t i) const
{
  return m_componentSettings[i].m_useFreiChen;
}
bool ImageSettings::useFreiChen() const
{
  return useFreiChen(m_activeComponent);
}

void ImageSettings::setEdgeMagnitude(uint32_t i, double mag)
{
  m_componentSettings[i].m_edgeMagnitude = mag;
}
void ImageSettings::setEdgeMagnitude(double mag)
{
  setEdgeMagnitude(m_activeComponent, mag);
}

double ImageSettings::edgeMagnitude(uint32_t i) const
{
  return m_componentSettings[i].m_edgeMagnitude;
}
double ImageSettings::edgeMagnitude() const
{
  return edgeMagnitude(m_activeComponent);
}

void ImageSettings::setWindowedEdges(uint32_t i, bool windowed)
{
  m_componentSettings[i].m_windowedEdges = windowed;
}
void ImageSettings::setWindowedEdges(bool windowed)
{
  setWindowedEdges(m_activeComponent, windowed);
}

bool ImageSettings::windowedEdges(uint32_t i) const
{
  return m_componentSettings[i].m_windowedEdges;
}
bool ImageSettings::windowedEdges() const
{
  return windowedEdges(m_activeComponent);
}

void ImageSettings::setOverlayEdges(uint32_t i, bool overlay)
{
  m_componentSettings[i].m_overlayEdges = overlay;
}
void ImageSettings::setOverlayEdges(bool overlay)
{
  setOverlayEdges(m_activeComponent, overlay);
}

bool ImageSettings::overlayEdges(uint32_t i) const
{
  return m_componentSettings[i].m_overlayEdges;
}
bool ImageSettings::overlayEdges() const
{
  return overlayEdges(m_activeComponent);
}

void ImageSettings::setColormapEdges(uint32_t i, bool showEdges)
{
  m_componentSettings[i].m_colormapEdges = showEdges;
}
void ImageSettings::setColormapEdges(bool showEdges)
{
  setColormapEdges(m_activeComponent, showEdges);
}

bool ImageSettings::colormapEdges(uint32_t i) const
{
  return m_componentSettings[i].m_colormapEdges;
}
bool ImageSettings::colormapEdges() const
{
  return colormapEdges(m_activeComponent);
}

void ImageSettings::setEdgeColor(uint32_t i, glm::vec3 color)
{
  m_componentSettings[i].m_edgeColor = std::move(color);
}
void ImageSettings::setEdgeColor(glm::vec3 color)
{
  setEdgeColor(m_activeComponent, color);
}

glm::vec3 ImageSettings::edgeColor(uint32_t i) const
{
  return m_componentSettings[i].m_edgeColor;
}
glm::vec3 ImageSettings::edgeColor() const
{
  return edgeColor(m_activeComponent);
}

void ImageSettings::setEdgeOpacity(uint32_t i, double opacity)
{
  m_componentSettings[i].m_edgeOpacity = opacity;
}
void ImageSettings::setEdgeOpacity(double opacity)
{
  setEdgeOpacity(m_activeComponent, opacity);
}

double ImageSettings::edgeOpacity(uint32_t i) const
{
  return m_componentSettings[i].m_edgeOpacity;
}
double ImageSettings::edgeOpacity() const
{
  return edgeOpacity(m_activeComponent);
}

void ImageSettings::setColorMapIndex(uint32_t i, std::size_t index)
{
  m_componentSettings[i].m_colorMapIndex = index;
}
void ImageSettings::setColorMapIndex(std::size_t index)
{
  setColorMapIndex(m_activeComponent, index);
}

size_t ImageSettings::colorMapIndex(uint32_t i) const
{
  return m_componentSettings[i].m_colorMapIndex;
}
size_t ImageSettings::colorMapIndex() const
{
  return colorMapIndex(m_activeComponent);
}

void ImageSettings::setColorMapInverted(uint32_t i, bool inverted)
{
  m_componentSettings[i].m_colorMapInverted = inverted;
}
void ImageSettings::setColorMapInverted(bool inverted)
{
  setColorMapInverted(m_activeComponent, inverted);
}

bool ImageSettings::isColorMapInverted(uint32_t i) const
{
  return m_componentSettings[i].m_colorMapInverted;
}
bool ImageSettings::isColorMapInverted() const
{
  return isColorMapInverted(m_activeComponent);
}

void ImageSettings::setColorMapQuantization(uint32_t i, uint32_t levels)
{
  m_componentSettings[i].m_numColorMapLevels = levels;
}
void ImageSettings::setColorMapQuantizationLevels(uint32_t levels)
{
  setColorMapQuantization(m_activeComponent, levels);
}

size_t ImageSettings::colorMapQuantizationLevels(uint32_t i) const
{
  return m_componentSettings[i].m_numColorMapLevels;
}
size_t ImageSettings::colorMapQuantizationLevels() const
{
  return colorMapQuantizationLevels(m_activeComponent);
}

void ImageSettings::setColorMapContinuous(uint32_t i, bool continuous)
{
  m_componentSettings[i].m_colorMapContinuous = continuous;
}
void ImageSettings::setColorMapContinuous(bool continuous)
{
  setColorMapContinuous(m_activeComponent, continuous);
}

bool ImageSettings::colorMapContinuous(uint32_t i) const
{
  return m_componentSettings[i].m_colorMapContinuous;
}
bool ImageSettings::colorMapContinuous() const
{
  return colorMapContinuous(m_activeComponent);
}

void ImageSettings::setColorMapHueModFactor(uint32_t i, double hueMod)
{
  m_componentSettings[i].m_hsvModFactors[0] = hueMod;
}
void ImageSettings::setColorMapSatModFactor(uint32_t i, double satMod)
{
  m_componentSettings[i].m_hsvModFactors[1] = satMod;
}
void ImageSettings::setColorMapValModFactor(uint32_t i, double valMod)
{
  m_componentSettings[i].m_hsvModFactors[2] = valMod;
}

void ImageSettings::setColorMapHueModFactor(double hueMod)
{
  setColorMapHueModFactor(m_activeComponent, hueMod);
}
void ImageSettings::setColorMapSatModFactor(double satMod)
{
  setColorMapSatModFactor(m_activeComponent, satMod);
}
void ImageSettings::setColorMapValModFactor(double valMod)
{
  setColorMapValModFactor(m_activeComponent, valMod);
}

void ImageSettings::setColormapHsvModfactors(uint32_t i, const glm::vec3& hsvMods)
{
  m_componentSettings[i].m_hsvModFactors = hsvMods;
}
void ImageSettings::setColormapHsvModfactors(const glm::vec3& hsvMods)
{
  setColormapHsvModfactors(m_activeComponent, hsvMods);
}

const glm::vec3& ImageSettings::colorMapHsvModFactors(uint32_t i) const
{
  return m_componentSettings[i].m_hsvModFactors;
}
const glm::vec3& ImageSettings::colorMapHsvModFactors() const
{
  return colorMapHsvModFactors(m_activeComponent);
}

void ImageSettings::setLabelTableIndex(uint32_t i, std::size_t index)
{
  m_componentSettings[i].m_labelTableIndex = index;
}
void ImageSettings::setLabelTableIndex(std::size_t index)
{
  setLabelTableIndex(m_activeComponent, index);
}

size_t ImageSettings::labelTableIndex(uint32_t i) const
{
  return m_componentSettings[i].m_labelTableIndex;
}
size_t ImageSettings::labelTableIndex() const
{
  return labelTableIndex(m_activeComponent);
}

void ImageSettings::setInterpolationMode(uint32_t i, InterpolationMode mode)
{
  m_componentSettings[i].m_interpolationMode = mode;
}
void ImageSettings::setInterpolationMode(InterpolationMode mode)
{
  setInterpolationMode(m_activeComponent, mode);
}

InterpolationMode ImageSettings::interpolationMode(uint32_t i) const
{
  return m_componentSettings[i].m_interpolationMode;
}
InterpolationMode ImageSettings::interpolationMode() const
{
  return interpolationMode(m_activeComponent);
}

std::pair<double, double> ImageSettings::thresholdRange(uint32_t i) const
{
  return m_componentSettings[i].m_minMaxThresholdRange;
}
std::pair<double, double> ImageSettings::thresholdRange() const
{
  return thresholdRange(m_activeComponent);
}

std::pair<double, double> ImageSettings::slopeIntercept_normalized_T_native(uint32_t i) const
{
  return {m_componentSettings[i].m_slope_native, m_componentSettings[i].m_intercept_native};
}
std::pair<double, double> ImageSettings::slopeIntercept_normalized_T_native() const
{
  return slopeIntercept_normalized_T_native(m_activeComponent);
}

std::pair<double, double> ImageSettings::slopeIntercept_normalized_T_texture(uint32_t i) const
{
  return {m_componentSettings[i].m_slope_texture, m_componentSettings[i].m_intercept_texture};
}
std::pair<double, double> ImageSettings::slopeIntercept_normalized_T_texture() const
{
  return slopeIntercept_normalized_T_texture(m_activeComponent);
}

glm::dvec2 ImageSettings::slopeInterceptVec2_normalized_T_texture(uint32_t i) const
{
  return {m_componentSettings[i].m_slope_texture, m_componentSettings[i].m_intercept_texture};
}
glm::dvec2 ImageSettings::slopeInterceptVec2_normalized_T_texture() const
{
  return slopeInterceptVec2_normalized_T_texture(m_activeComponent);
}

float ImageSettings::slope_native_T_texture() const
{
  // Example for int8_t:
  // -1.0 maps to -127
  // 0.0 maps to 0
  // 1.0 maps to 127
  // i.e. NATIVE = M * TEXTURE, where M = 127

  // Example for uint8_t:
  // 0.0 maps to 0
  // 1.0 maps to 255
  // i.e. NATIVE = M * TEXTURE, where M = 255

  switch (m_componentType)
  {
  case ComponentType::Int8:
  {
    return static_cast<float>(std::numeric_limits<int8_t>::max());
  }
  case ComponentType::Int16:
  {
    return static_cast<float>(std::numeric_limits<int16_t>::max());
  }
  case ComponentType::Int32:
  {
    return static_cast<float>(std::numeric_limits<int32_t>::max());
  }
  case ComponentType::UInt8:
  {
    return static_cast<float>(std::numeric_limits<uint8_t>::max());
  }
  case ComponentType::UInt16:
  {
    return static_cast<float>(std::numeric_limits<uint16_t>::max());
  }
  case ComponentType::UInt32:
  {
    return static_cast<float>(std::numeric_limits<uint32_t>::max());
  }
  case ComponentType::Float32:
  {
    return 1.0f;
  }
  default:
  {
    spdlog::error("Invalid component type {}", componentTypeString(m_componentType));
    return 1.0f;
  }
  }
}

glm::dvec2 ImageSettings::largestSlopeInterceptTextureVec2(uint32_t i) const
{
  return {
    m_componentSettings[i].m_largest_slope_texture,
    m_componentSettings[i].m_largest_intercept_texture
  };
}

glm::dvec2 ImageSettings::largestSlopeInterceptTextureVec2() const
{
  return largestSlopeInterceptTextureVec2(m_activeComponent);
}

uint32_t ImageSettings::numComponents() const
{
  return m_numComponents;
}

const ComponentStats& ImageSettings::componentStatistics(uint32_t i) const
{
  if (m_componentStats.size() <= i)
  {
    spdlog::error("Invalid image component {}", i);
    throw_debug("Invalid image component")
  }

  return m_componentStats.at(i);
}

const ComponentStats& ImageSettings::componentStatistics() const
{
  return componentStatistics(m_activeComponent);
}

const HistogramSettings& ImageSettings::histogramSettings(uint32_t comp) const
{
  return m_componentSettings.at(comp).m_histogramSettings;
}
const HistogramSettings& ImageSettings::histogramSettings() const
{
  return histogramSettings(m_activeComponent);
}

HistogramSettings& ImageSettings::histogramSettings(uint32_t comp)
{
  return m_componentSettings.at(comp).m_histogramSettings;
}
HistogramSettings& ImageSettings::histogramSettings()
{
  return histogramSettings(m_activeComponent);
}

void ImageSettings::setActiveComponent(uint32_t component)
{
  if (component < m_numComponents)
  {
    m_activeComponent = component;
  }
  else
  {
    spdlog::error(
      "Attempting to set invalid active component {} (only {} components total for image {})",
      component,
      m_numComponents,
      m_displayName
    );
  }
}

void ImageSettings::updateWithNewComponentStatistics(
  std::vector<ComponentStats> componentStats, bool setDefaultVisibilitySettings
)
{
  // Default window covers 1st to 99th quantile intensity range of the first pixel component.
  // Recall that the histogram has 1001 bins.
  static constexpr int qLow = 1;   // 1% level
  static constexpr int qHigh = 99; // 99% level
  static constexpr int qMax = 100; // 100% level

  if (componentStats.size() != m_numComponents)
  {
    spdlog::error(
      "Component statistics has {} components, where {} are expected",
      componentStats.size(),
      m_numComponents
    );
    return;
  }

  m_componentStats = std::move(componentStats);

  for (std::size_t i = 0; i < m_numComponents; ++i)
  {
    const auto& stats = m_componentStats[i];
    ComponentSettings& setting = m_componentSettings[i];

    // Min/max window width/center and threshold ranges are based on min/max component values:
    setting.m_minMaxImageRange = std::make_pair(stats.m_minimum, stats.m_maximum);
    setting.m_minMaxThresholdRange = std::make_pair(stats.m_minimum, stats.m_maximum);

    setting.m_minMaxWindowCenterRange = std::make_pair(stats.m_minimum, stats.m_maximum);
    setting.m_minMaxWindowWidthRange = std::make_pair(0.0, stats.m_maximum - stats.m_minimum);

    // Default thresholds are min/max values:
    setting.m_thresholds = std::make_pair(stats.m_minimum, stats.m_maximum);

    // Default window limits are the low and high quantiles:
    const double winLow = stats.m_quantiles[qLow];
    const double winHigh = stats.m_quantiles[qHigh];

    setting.m_windowCenter = 0.5 * (winLow + winHigh);
    setting.m_windowWidth = winHigh - winLow;

    // Use the [1%, 100%] intensity range to define foreground
    // (until we have an algorithm to compute a foreground mask)
    setting.m_foregroundThresholds = std::make_pair(stats.m_quantiles[qLow], stats.m_quantiles[qMax]);

    // Update histogram settings
    setting.m_histogramSettings.m_numBinsMethod = NumBinsComputationMethod::FreedmanDiaconis;
    setting.m_histogramSettings.m_isCumulative = false;
    setting.m_histogramSettings.m_isDensity = false;
    setting.m_histogramSettings.m_isHorizontal = false;
    setting.m_histogramSettings.m_isLogScale = false;
    setting.m_histogramSettings.m_useCustomIntensityRange = false;
    setting.m_histogramSettings.m_intensityRange[0] = stats.m_minimum;
    setting.m_histogramSettings.m_intensityRange[1] = stats.m_maximum;

    if (0 == m_numPixels)
    {
      spdlog::warn(
        "Component {} of image {} has zero pixels, so setting number of histogram bins to one",
        i,
        m_displayName
      );
      setting.m_histogramSettings.m_numBins = 1;
    }
    else
    {
      std::optional<std::size_t> numBins = computeNumHistogramBins(
        setting.m_histogramSettings.m_numBinsMethod, m_numPixels, m_componentStats[i]
      );

      if (!numBins)
      {
        spdlog::warn(
          "Could not compute number of histogram for component {} of image {}", i, m_displayName
        );
        spdlog::info("Falling back to Sturge's method for computing number of histogram bins");

        setting.m_histogramSettings.m_numBinsMethod = NumBinsComputationMethod::Sturges;
        numBins = computeNumHistogramBins(
          setting.m_histogramSettings.m_numBinsMethod, m_numPixels, m_componentStats[i]
        );
      }

      setting.m_histogramSettings.m_numBins = numBins.value_or(1);

      setting.m_histogramSettings.m_binWidth = (m_componentStats[i].m_maximum
                                                - m_componentStats[i].m_minimum)
                                               / setting.m_histogramSettings.m_numBins;
    }

    if (setDefaultVisibilitySettings)
    {
      // Default to max opacity and nearest neighbor interpolation
      setting.m_opacity = 1.0;
      setting.m_visible = true;

      setting.m_showEdges = false;
      setting.m_thresholdEdges = false;
      setting.m_useFreiChen = false;
      setting.m_edgeMagnitude = 0.25;
      setting.m_windowedEdges = false;
      setting.m_overlayEdges = false;
      setting.m_colormapEdges = false;
      setting.m_edgeColor = glm::vec3{1.0f, 0.0f, 1.0f};
      setting.m_edgeOpacity = 1.0;
      setting.m_interpolationMode = InterpolationMode::Trilinear;

      // Use the first color map and label table
      setting.m_colorMapIndex = 0;
      setting.m_colorMapInverted = false;
      setting.m_labelTableIndex = 0;
    }
  }

  updateInternals();
}

uint32_t ImageSettings::activeComponent() const
{
  return m_activeComponent;
}

void ImageSettings::updateInternals()
{
  for (uint32_t i = 0; i < m_componentSettings.size(); ++i)
  {
    auto& S = m_componentSettings[i];

    const double imageRange = S.m_minMaxImageRange.second - S.m_minMaxImageRange.first;

    if (imageRange <= 0.0 || windowWidth(i) <= 0.0)
    {
      // Resort to default slope/intercept and normalized threshold values
      // if either the image range or the window width are not positive:

      S.m_slope_native = 0.0;
      S.m_intercept_native = 0.0;

      S.m_slope_texture = 0.0;
      S.m_intercept_texture = 0.0;

      S.m_largest_slope_texture = 0.0;
      S.m_largest_intercept_texture = 0.0;

      continue;
    }

    S.m_slope_native = 1.0 / windowWidth(i);
    S.m_intercept_native = 0.5 - windowCenter(i) / windowWidth(i);

    /**
         * @note In OpenGL, UNSIGNED normalized floats are computed as
         * float = int / MAX, where MAX = 2^B - 1 = 255
         *
         * SIGNED normalized floats are computed as either
         * float = max(int / MAX, -1) where MAX = 2^(B-1) - 1 = 127
         * (this is the method used most commonly in OpenGL 4.2 and above)
         *
         * or alternatively as (depending on implementation)
         * float = (2*int + 1) / (2^B - 1) = (2*int + 1) / 255
         *
         * @see https://www.khronos.org/opengl/wiki/Normalized_Integer
         */

    double M = 0.0;

    switch (m_componentType)
    {
    case ComponentType::Int8:
    case ComponentType::UInt8:
    {
      M = static_cast<double>(std::numeric_limits<uint8_t>::max());
      break;
    }
    case ComponentType::Int16:
    case ComponentType::UInt16:
    {
      M = static_cast<double>(std::numeric_limits<uint16_t>::max());
      break;
    }
    case ComponentType::Int32:
    case ComponentType::UInt32:
    {
      M = static_cast<double>(std::numeric_limits<uint32_t>::max());
      break;
    }
    case ComponentType::Float32:
    {
      M = 0.0;
      break;
    }
    default:
      break;
    }

    switch (m_componentType)
    {
    case ComponentType::Int8:
    case ComponentType::Int16:
    case ComponentType::Int32:
    {
      /// @todo This mapping may be slightly wrong for the signed integer case
      S.m_slope_texture = 0.5 * M / imageRange;
      S.m_intercept_texture = -(S.m_minMaxImageRange.first + 0.5) / imageRange;
      break;
    }
    case ComponentType::UInt8:
    case ComponentType::UInt16:
    case ComponentType::UInt32:
    {
      S.m_slope_texture = M / imageRange;
      S.m_intercept_texture = -S.m_minMaxImageRange.first / imageRange;
      break;
    }
    case ComponentType::Float32:
    {
      S.m_slope_texture = 1.0 / imageRange;
      S.m_intercept_texture = -S.m_minMaxImageRange.first / imageRange;
      break;
    }
    default:
      break;
    }

    const double a = 1.0 / imageRange;
    const double b = -S.m_minMaxImageRange.first / imageRange;

    // Normalized window and level:
    const double windowNorm = a * windowWidth(i);
    const double levelNorm = a * windowCenter(i) + b;

    // The slope and intercept that give the largest window:
    S.m_largest_slope_texture = S.m_slope_texture;
    S.m_largest_intercept_texture = S.m_intercept_texture;

    // Apply windowing and leveling to the slope and intercept:
    S.m_slope_texture = S.m_slope_texture / windowNorm;
    S.m_intercept_texture = S.m_intercept_texture / windowNorm + (0.5 - levelNorm / windowNorm);
  }
}

double ImageSettings::mapNativeIntensityToTexture(double nativeImageValue) const
{
  /// @note An alternate mapping for signed integers is sometimes used in OpenGL < 4.2:
  /// return (2.0 * nativeImageValue + 1.0) / (2^B - 1)
  /// This mapping does not allow for a signed integer to exactly express the value zero.

  // Example for int8_t:
  // M = 127
  // -128 maps to -1.0
  // -127 maps to -1.0
  // 0 maps to 0
  // 127 maps to 1.0

  // Example for uint8_t:
  // M = 255
  // 0 maps to 0
  // 255 maps to 1.0

  switch (m_componentType)
  {
  case ComponentType::Int8:
  {
    constexpr double M = static_cast<double>(std::numeric_limits<int8_t>::max());
    return std::max(nativeImageValue / M, -1.0);
  }
  case ComponentType::Int16:
  {
    constexpr double M = static_cast<double>(std::numeric_limits<int16_t>::max());
    return std::max(nativeImageValue / M, -1.0);
    // return (2.0 * nativeImageValue + 1.0) / 65535.0;
  }
  case ComponentType::Int32:
  {
    constexpr double M = static_cast<double>(std::numeric_limits<int32_t>::max());
    return std::max(nativeImageValue / M, -1.0);
  }
  case ComponentType::UInt8:
  {
    constexpr double M = static_cast<double>(std::numeric_limits<uint8_t>::max());
    return nativeImageValue / M;
  }
  case ComponentType::UInt16:
  {
    constexpr double M = static_cast<double>(std::numeric_limits<uint16_t>::max());
    return nativeImageValue / M;
  }
  case ComponentType::UInt32:
  {
    constexpr double M = static_cast<double>(std::numeric_limits<uint32_t>::max());
    return nativeImageValue / M;
  }
  case ComponentType::Float32:
  {
    return nativeImageValue;
  }
  default:
  {
    spdlog::error("Invalid component type {}", componentTypeString(m_componentType));
    return nativeImageValue;
  }
  }
}

std::ostream& operator<<(std::ostream& os, const ImageSettings& settings)
{
  os << "Display name: " << settings.m_displayName;

  for (std::size_t i = 0; i < settings.m_componentStats.size(); ++i)
  {
    const auto& s = settings.m_componentSettings[i];
    const auto& t = settings.m_componentStats[i];

    os << "\nStatistics (component " << i << "):"
       << "\n\tMin: " << t.m_minimum << "\n\tQ01: " << t.m_quantiles[1]
       << "\n\tQ25: " << t.m_quantiles[25] << "\n\tMed: " << t.m_quantiles[50]
       << "\n\tQ75: " << t.m_quantiles[75] << "\n\tQ99: " << t.m_quantiles[99]
       << "\n\tMax: " << t.m_maximum << "\n\tAvg: " << t.m_mean << "\n\tStd: " << t.m_stdDeviation;

    os << "\n\n\tWindow: [" << s.m_windowCenter - 0.5 * s.m_windowWidth << ", "
       << s.m_windowCenter + 0.5 * s.m_windowWidth << "]"
       << "\n\tThreshold: [" << s.m_thresholds.first << ", " << s.m_thresholds.second << "]";
  }

  return os;
}
