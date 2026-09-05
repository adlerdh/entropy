#include "image/ImageSettings.h"
#include "image/ImageUtility.h"

#include "common/Exception.hpp"
#include "common/Types.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#undef min
#undef max

namespace
{

constexpr int k_defaultWindowLowQuantileIndex = 1;   // 1% level
constexpr int k_defaultWindowHighQuantileIndex = 99; // 99% level
constexpr int k_maxQuantileIndex = 100;
constexpr double k_minConstantImageWindowWidth = 1.0e-6;

struct DefaultWindow
{
  double low = 0.0;
  double high = 0.0;
  double lowQuantile = 0.0;
  double highQuantile = 0.0;
};

DefaultWindow computeDefaultWindow(const ComponentStats& stats)
{
  DefaultWindow window;
  window.low = static_cast<double>(stats.quantiles[k_defaultWindowLowQuantileIndex]);
  window.high = static_cast<double>(stats.quantiles[k_defaultWindowHighQuantileIndex]);
  window.lowQuantile = 0.01;
  window.highQuantile = 0.99;

  const double minValue = static_cast<double>(stats.onlineStats.min);
  const double maxValue = static_cast<double>(stats.onlineStats.max);
  if (window.high <= window.low && minValue < maxValue) {
    window.low = minValue;
    window.high = maxValue;
    window.lowQuantile = 0.0;
    window.highQuantile = 1.0;
  }

  return window;
}

double constantImageWindowWidth(double value)
{
  return std::max(1.0, 0.1 * std::abs(value));
}

} // namespace

ImageSettings::ImageSettings(
  std::string displayName,
  std::size_t numPixels,
  uint32_t numComponents,
  ComponentType componentType,
  const std::vector<ComponentStats>& componentStats)
  : m_numPixels(numPixels)
  , m_numComponents(numComponents)
  , m_componentType(componentType)
  , m_componentSettings(numComponents)
{
  if (0 == m_numPixels) {
    spdlog::error("Zero pixels is invalid when constructing settings for image {}", displayName);
    throwDebug("Invalid number of pixels provided to construct settings for image");
  }

  if (componentStats.size() != m_numComponents) {
    spdlog::error(
      "Invalid number of components ({}) provided to construct settings for image {}",
      numComponents,
      displayName);
    throwDebug("Invalid number of components provided to construct settings for image");
  }

  m_displayName = std::move(displayName);

  constexpr bool k_setDefaultVisibilitySettings = true;
  updateWithNewComponentStatistics(componentStats, k_setDefaultVisibilitySettings);
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

void ImageSettings::setActiveTimePoint(uint32_t timePoint)
{
  m_activeTimePoint = timePoint;
}

uint32_t ImageSettings::activeTimePoint() const
{
  return m_activeTimePoint;
}

void ImageSettings::setTimePlaybackLoop(bool loop)
{
  m_timePlaybackLoop = loop;
}

bool ImageSettings::timePlaybackLoop() const
{
  return m_timePlaybackLoop;
}

void ImageSettings::setTimePlaybackPlaying(bool playing)
{
  m_timePlaybackPlaying = playing;
}

bool ImageSettings::timePlaybackPlaying() const
{
  return m_timePlaybackPlaying;
}

void ImageSettings::setTimePlaybackSpeed(double speed)
{
  m_timePlaybackSpeed = std::max(0.0, speed);
}

double ImageSettings::timePlaybackSpeed() const
{
  return m_timePlaybackSpeed;
}

void ImageSettings::setBorderColor(glm::vec3 borderColorArg)
{
  m_borderColor = borderColorArg;
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

void ImageSettings::setWarpEnabled(bool enabled)
{
  m_warpEnabled = enabled;
}

bool ImageSettings::warpEnabled() const
{
  return m_warpEnabled;
}

void ImageSettings::setWarpStrength(float strength)
{
  m_warpStrength = std::clamp(strength, 0.0f, 4.0f);
}

float ImageSettings::warpStrength() const
{
  return m_warpStrength;
}

void ImageSettings::setAllowExaggeratedWarp(bool allow)
{
  m_allowExaggeratedWarp = allow;
  if (!m_allowExaggeratedWarp) {
    m_warpStrength = std::min(m_warpStrength, 1.0f);
  }
}

bool ImageSettings::allowExaggeratedWarp() const
{
  return m_allowExaggeratedWarp;
}

void ImageSettings::setDisplayImageAsColor(bool doColor)
{
  setComponentRenderMode(doColor ? ComponentRenderMode::Color : ComponentRenderMode::SingleComponent);
}

bool ImageSettings::displayImageAsColor() const
{
  return ComponentRenderMode::Color == m_componentRenderMode;
}

void ImageSettings::setComponentRenderMode(ComponentRenderMode mode)
{
  m_componentRenderMode = mode;
}

ComponentRenderMode ImageSettings::componentRenderMode() const
{
  return m_componentRenderMode;
}

void ImageSettings::setVectorArrowOverlayVisible(bool visible)
{
  m_vectorArrowOverlayVisible = visible;
}

bool ImageSettings::vectorArrowOverlayVisible() const
{
  return m_vectorArrowOverlayVisible;
}

void ImageSettings::setVectorArrowOverlayOnImage(bool overlayOnImage)
{
  m_vectorArrowOverlayOnImage = overlayOnImage;
}

bool ImageSettings::vectorArrowOverlayOnImage() const
{
  return m_vectorArrowOverlayOnImage;
}

void ImageSettings::setVectorArrowOverlayDensity(float densityPx)
{
  m_vectorArrowOverlayDensity = std::clamp(densityPx, 0.1f, 100.0f);
}

float ImageSettings::vectorArrowOverlayDensity() const
{
  return m_vectorArrowOverlayDensity;
}

void ImageSettings::setVectorArrowOverlayVoxelSpacing(float spacingVoxels)
{
  m_vectorArrowOverlayVoxelSpacing = std::clamp(spacingVoxels, 0.1f, 100.0f);
}

float ImageSettings::vectorArrowOverlayVoxelSpacing() const
{
  return m_vectorArrowOverlayVoxelSpacing;
}

void ImageSettings::setVectorArrowOverlayMillimeterSpacing(float spacingMm)
{
  m_vectorArrowOverlayMillimeterSpacing = std::clamp(spacingMm, 0.1f, 1000.0f);
}

float ImageSettings::vectorArrowOverlayMillimeterSpacing() const
{
  return m_vectorArrowOverlayMillimeterSpacing;
}

void ImageSettings::setVectorArrowOverlaySpacingMode(VectorArrowOverlaySpacingMode mode)
{
  m_vectorArrowOverlaySpacingMode = mode;
}

VectorArrowOverlaySpacingMode ImageSettings::vectorArrowOverlaySpacingMode() const
{
  return m_vectorArrowOverlaySpacingMode;
}

void ImageSettings::setVectorArrowOverlayColor(glm::vec3 color)
{
  m_vectorArrowOverlayColor = glm::clamp(color, glm::vec3{0.0f}, glm::vec3{1.0f});
}

const glm::vec3& ImageSettings::vectorArrowOverlayColor() const
{
  return m_vectorArrowOverlayColor;
}

void ImageSettings::setVectorArrowOverlayUseDirectionColor(bool useDirectionColor)
{
  m_vectorArrowOverlayUseDirectionColor = useDirectionColor;
}

bool ImageSettings::vectorArrowOverlayUseDirectionColor() const
{
  return m_vectorArrowOverlayUseDirectionColor;
}

void ImageSettings::setVectorArrowOverlayLineThickness(float thicknessPx)
{
  m_vectorArrowOverlayLineThickness = std::clamp(thicknessPx, 0.25f, 4.0f);
}

float ImageSettings::vectorArrowOverlayLineThickness() const
{
  return m_vectorArrowOverlayLineThickness;
}

void ImageSettings::setVectorArrowOverlayOpacity(float opacityArg)
{
  m_vectorArrowOverlayOpacity = std::clamp(opacityArg, 0.0f, 1.0f);
}

float ImageSettings::vectorArrowOverlayOpacity() const
{
  return m_vectorArrowOverlayOpacity;
}

void ImageSettings::setVectorArrowOverlayScaleByMagnitude(bool scaleByMagnitude)
{
  m_vectorArrowOverlayScaleByMagnitude = scaleByMagnitude;
}

bool ImageSettings::vectorArrowOverlayScaleByMagnitude() const
{
  return m_vectorArrowOverlayScaleByMagnitude;
}

void ImageSettings::setVectorArrowOverlayScaleFactor(float scaleFactor)
{
  m_vectorArrowOverlayScaleFactor = std::clamp(scaleFactor, 0.01f, 10.0f);
}

float ImageSettings::vectorArrowOverlayScaleFactor() const
{
  return m_vectorArrowOverlayScaleFactor;
}

void ImageSettings::setVectorWarpedGridVisible(bool visible)
{
  m_vectorWarpedGridVisible = visible;
}

bool ImageSettings::vectorWarpedGridVisible() const
{
  return m_vectorWarpedGridVisible;
}

void ImageSettings::setVectorWarpedGridOverlayOnImage(bool overlayOnImage)
{
  m_vectorWarpedGridOverlayOnImage = overlayOnImage;
}

bool ImageSettings::vectorWarpedGridOverlayOnImage() const
{
  return m_vectorWarpedGridOverlayOnImage;
}

void ImageSettings::setVectorWarpedGridConvention(VectorWarpedGridConvention convention)
{
  m_vectorWarpedGridConvention = convention;
}

VectorWarpedGridConvention ImageSettings::vectorWarpedGridConvention() const
{
  return m_vectorWarpedGridConvention;
}

void ImageSettings::setVectorWarpedGridPixelSpacing(float spacingPx)
{
  m_vectorWarpedGridPixelSpacing = std::clamp(spacingPx, 1.0f, 100.0f);
}

float ImageSettings::vectorWarpedGridPixelSpacing() const
{
  return m_vectorWarpedGridPixelSpacing;
}

void ImageSettings::setVectorWarpedGridVoxelSpacing(float spacingVoxels)
{
  m_vectorWarpedGridVoxelSpacing = std::clamp(spacingVoxels, 0.1f, 1000.0f);
}

float ImageSettings::vectorWarpedGridVoxelSpacing() const
{
  return m_vectorWarpedGridVoxelSpacing;
}

void ImageSettings::setVectorWarpedGridMillimeterSpacing(float spacingMm)
{
  m_vectorWarpedGridMillimeterSpacing = std::clamp(spacingMm, 0.1f, 1000.0f);
}

float ImageSettings::vectorWarpedGridMillimeterSpacing() const
{
  return m_vectorWarpedGridMillimeterSpacing;
}

void ImageSettings::setVectorWarpedGridSpacingMode(VectorArrowOverlaySpacingMode mode)
{
  m_vectorWarpedGridSpacingMode = mode;
}

VectorArrowOverlaySpacingMode ImageSettings::vectorWarpedGridSpacingMode() const
{
  return m_vectorWarpedGridSpacingMode;
}

void ImageSettings::setVectorWarpedGridLineThickness(float thicknessPx)
{
  m_vectorWarpedGridLineThickness = std::clamp(thicknessPx, 0.25f, 8.0f);
}

float ImageSettings::vectorWarpedGridLineThickness() const
{
  return m_vectorWarpedGridLineThickness;
}

void ImageSettings::setVectorWarpedGridScaleFactor(float scaleFactor)
{
  m_vectorWarpedGridScaleFactor = std::clamp(scaleFactor, 0.0f, 10.0f);
}

float ImageSettings::vectorWarpedGridScaleFactor() const
{
  return m_vectorWarpedGridScaleFactor;
}

void ImageSettings::setVectorWarpedGridForegroundColor(glm::vec4 color)
{
  m_vectorWarpedGridForegroundColor = glm::clamp(color, glm::vec4{0.0f}, glm::vec4{1.0f});
}

const glm::vec4& ImageSettings::vectorWarpedGridForegroundColor() const
{
  return m_vectorWarpedGridForegroundColor;
}

void ImageSettings::setVectorWarpedGridBackgroundColor(glm::vec4 color)
{
  m_vectorWarpedGridBackgroundColor = glm::clamp(color, glm::vec4{0.0f}, glm::vec4{1.0f});
}

const glm::vec4& ImageSettings::vectorWarpedGridBackgroundColor() const
{
  return m_vectorWarpedGridBackgroundColor;
}

void ImageSettings::setVectorPlanarProjectionSignedColors(bool signedColors)
{
  m_vectorPlanarProjectionSignedColors = signedColors;
}

bool ImageSettings::vectorPlanarProjectionSignedColors() const
{
  return m_vectorPlanarProjectionSignedColors;
}

void ImageSettings::setVectorLogJacobianDeterminant(bool logJacobian)
{
  m_vectorLogJacobianDeterminant = logJacobian;
}

bool ImageSettings::vectorLogJacobianDeterminant() const
{
  return m_vectorLogJacobianDeterminant;
}

void ImageSettings::setComplexPhaseUnit(ComplexPhaseUnit unit)
{
  m_complexPhaseUnit = unit;
}

ComplexPhaseUnit ImageSettings::complexPhaseUnit() const
{
  return m_complexPhaseUnit;
}

void ImageSettings::setComplexPhaseRange(ComplexPhaseRange range)
{
  m_complexPhaseRange = range;
}

ComplexPhaseRange ImageSettings::complexPhaseRange() const
{
  return m_complexPhaseRange;
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

void ImageSettings::setApplyImageColormapToIsosurfaces(bool apply)
{
  m_applyImageColormapToIsosurfaces = apply;
}

bool ImageSettings::applyImageColormapToIsosurfaces() const
{
  return m_applyImageColormapToIsosurfaces;
}

void ImageSettings::setModulateIsosurfaceOpacityWithImageOpacity(bool modulate)
{
  m_modulateIsosurfaceOpacityWithImageOpacity = modulate;
}

bool ImageSettings::modulateIsosurfaceOpacityWithImageOpacity() const
{
  return m_modulateIsosurfaceOpacityWithImageOpacity;
}

void ImageSettings::setIsosurfaceWidthIn2d(double width)
{
  m_isocontourLineWidthIn2D = width;
}

double ImageSettings::isoContourLineWidthIn2D() const
{
  return m_isocontourLineWidthIn2D;
}

void ImageSettings::setIsosurfaceOpacityModulator(float opacityMod)
{
  m_isosurfaceOpacityModulator = opacityMod;
}

float ImageSettings::isosurfaceOpacityModulator() const
{
  return m_isosurfaceOpacityModulator;
}

float ImageSettings::effectiveIsosurfaceOpacityModulator() const
{
  const double imageOpacity = m_modulateIsosurfaceOpacityWithImageOpacity ? opacity() * globalOpacity() : 1.0;
  return m_isosurfaceOpacityModulator * static_cast<float>(imageOpacity);
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
    minMaxWindowCenterRange(i).second + 0.5 * minMaxWindowWidthRange(i).second};
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

  if (!clampValues) {
    if (wLow > (wHigh - narrowestWidth) || wLow < wMin || wMax < wLow) {
      return;
    }
  }
  else {
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

  if (!clampValues) {
    if (wHigh < (wLow + narrowestWidth) || wHigh < wMin || wMax < wHigh) {
      return;
    }
  }
  else {
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

// cppcheck-suppress functionStatic -- this setter is instance-shaped pending quantile-window implementation
void ImageSettings::setWindowQuantileLow(uint32_t i, double pLow, bool clampValues)
{
  std::ignore = i;
  std::ignore = pLow;
  std::ignore = clampValues;
}

// cppcheck-suppress functionStatic -- this setter is instance-shaped pending quantile-window implementation
void ImageSettings::setWindowQuantileHigh(uint32_t i, double pHigh, bool clampValues)
{
  std::ignore = i;
  std::ignore = pHigh;
  std::ignore = clampValues;
}

void ImageSettings::setWindowQuantileLow(double pLow, bool clampValues) const
{
  setWindowQuantileLow(m_activeComponent, pLow, clampValues);
}

void ImageSettings::setWindowQuantileHigh(double pHigh, bool clampValues) const
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
  m_componentSettings[i].m_windowWidth =
    std::clamp(width, minMaxWindowWidthRange(i).first, minMaxWindowWidthRange(i).second);
  updateInternals();
}

void ImageSettings::setWindowWidth(double width)
{
  setWindowWidth(m_activeComponent, width);
}

void ImageSettings::setWindowCenter(uint32_t i, double center)
{
  m_componentSettings[i].m_windowCenter =
    std::clamp(center, minMaxWindowCenterRange(i).first, minMaxWindowCenterRange(i).second);
  updateInternals();
}

void ImageSettings::setWindowCenter(double center)
{
  setWindowCenter(m_activeComponent, center);
}

void ImageSettings::setWindowRangeForAllComponents(std::pair<double, double> range)
{
  const auto [low, high] = range;

  const bool constantRange = high <= low;
  const double center = constantRange ? low : 0.5 * (low + high);
  const double width = constantRange ? constantImageWindowWidth(low) : high - low;
  const std::pair<double, double> centerRange =
    constantRange ? std::make_pair(low - width, low + width) : std::make_pair(low, high);
  const std::pair<double, double> widthRange =
    constantRange ? std::make_pair(k_minConstantImageWindowWidth, 10.0 * width) : std::make_pair(0.0, width);

  for (ComponentSettings& setting : m_componentSettings) {
    setting.m_minMaxWindowCenterRange = centerRange;
    setting.m_minMaxWindowWidthRange = widthRange;
    setting.m_windowCenter = center;
    setting.m_windowWidth = width;
    setting.m_windowQuantilesLowHigh = {0.0, 1.0};
  }

  updateInternals();
}

void ImageSettings::setThresholdLow(uint32_t i, double tLow)
{
  if (tLow <= m_componentSettings[i].m_thresholds.second) {
    m_componentSettings[i].m_thresholds.first = std::max(tLow, m_componentSettings[i].m_minMaxThresholdRange.first);
  }
}

void ImageSettings::setThresholdHigh(uint32_t i, double tHigh)
{
  if (m_componentSettings[i].m_thresholds.first <= tHigh) {
    m_componentSettings[i].m_thresholds.second = std::min(tHigh, m_componentSettings[i].m_minMaxThresholdRange.second);
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
    S.m_minMaxThresholdRange.first < S.m_thresholds.first || S.m_thresholds.second < S.m_minMaxThresholdRange.second);
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

void ImageSettings::setGlobalOpacity(double opacityArg)
{
  m_globalOpacity = static_cast<float>(std::max(std::min(opacityArg, 1.0), 0.0));
}

double ImageSettings::globalOpacity() const
{
  return m_globalOpacity;
}

void ImageSettings::setThinPixelEdges(bool thin)
{
  m_thinPixelEdges = thin;
}

bool ImageSettings::thinPixelEdges() const
{
  return m_thinPixelEdges;
}

void ImageSettings::setEdgesVisible(bool visible)
{
  m_edgesVisible = visible;
}

bool ImageSettings::edgesVisible() const
{
  return m_edgesVisible;
}

void ImageSettings::setEdgeDetectionMethod(EdgeDetectionMethod method)
{
  switch (method) {
    case EdgeDetectionMethod::Voxel:
    case EdgeDetectionMethod::ScreenPixel:
      m_edgeDetectionMethod = method;
      break;
  }
}

EdgeDetectionMethod ImageSettings::edgeDetectionMethod() const
{
  return m_edgeDetectionMethod;
}

void ImageSettings::setHardEdges(bool hard)
{
  m_hardEdges = hard;
}

bool ImageSettings::hardEdges() const
{
  return m_hardEdges;
}

void ImageSettings::setVoxelEdgeScale(double scale)
{
  m_voxelEdgeScale = std::isfinite(scale) ? std::clamp(scale, 0.01, 10.0) : 4.0;
}

double ImageSettings::voxelEdgeScale() const
{
  return m_voxelEdgeScale;
}

void ImageSettings::setVoxelEdgeThreshold(double threshold)
{
  m_voxelEdgeThreshold = std::isfinite(threshold) ? std::clamp(threshold, 0.0, 1.0) : 0.25;
}

double ImageSettings::voxelEdgeThreshold() const
{
  return m_voxelEdgeThreshold;
}

void ImageSettings::setPixelEdgeScale(double scale)
{
  m_pixelEdgeScale = std::isfinite(scale) ? std::clamp(scale, 0.01, 10.0) : 2.0;
}

double ImageSettings::pixelEdgeScale() const
{
  return m_pixelEdgeScale;
}

void ImageSettings::setPixelEdgeThreshold(double threshold)
{
  m_pixelEdgeThreshold = std::isfinite(threshold) ? std::clamp(threshold, 0.0, 1.0) : 0.2;
}

double ImageSettings::pixelEdgeThreshold() const
{
  return m_pixelEdgeThreshold;
}

void ImageSettings::setOverlayEdges(bool overlay)
{
  m_overlayEdges = overlay;
}

bool ImageSettings::overlayEdges() const
{
  return m_overlayEdges;
}

void ImageSettings::setColormapEdges(bool showEdgesArg)
{
  m_colormapEdges = showEdgesArg;
}

bool ImageSettings::colormapEdges() const
{
  return m_colormapEdges;
}

void ImageSettings::setEdgeColor(glm::vec3 color)
{
  if (!std::isfinite(color.r) || !std::isfinite(color.g) || !std::isfinite(color.b)) {
    m_edgeColor = glm::vec3{1.0f, 0.0f, 1.0f};
    return;
  }
  m_edgeColor = glm::clamp(color, glm::vec3{0.0f}, glm::vec3{1.0f});
}

const glm::vec3& ImageSettings::edgeColor() const
{
  return m_edgeColor;
}

void ImageSettings::setEdgeOpacity(double opacityArg)
{
  m_edgeOpacity = std::isfinite(opacityArg) ? std::clamp(opacityArg, 0.0, 1.0) : 1.0;
}

double ImageSettings::edgeOpacity() const
{
  return m_edgeOpacity;
}

void ImageSettings::copyEdgeSettingsFrom(const ImageSettings& source)
{
  m_edgeDetectionMethod = source.m_edgeDetectionMethod;
  m_edgesVisible = source.m_edgesVisible;
  m_hardEdges = source.m_hardEdges;
  m_thinPixelEdges = source.m_thinPixelEdges;
  m_voxelEdgeScale = source.m_voxelEdgeScale;
  m_voxelEdgeThreshold = source.m_voxelEdgeThreshold;
  m_pixelEdgeScale = source.m_pixelEdgeScale;
  m_pixelEdgeThreshold = source.m_pixelEdgeThreshold;
  m_overlayEdges = source.m_overlayEdges;
  m_colormapEdges = source.m_colormapEdges;
  m_edgeColor = source.m_edgeColor;
  m_edgeOpacity = source.m_edgeOpacity;
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
  m_componentSettings[i].m_hsvModFactors[0] = static_cast<float>(hueMod);
}

void ImageSettings::setColorMapSatModFactor(uint32_t i, double satMod)
{
  m_componentSettings[i].m_hsvModFactors[1] = static_cast<float>(satMod);
}

void ImageSettings::setColorMapValModFactor(uint32_t i, double valMod)
{
  m_componentSettings[i].m_hsvModFactors[2] = static_cast<float>(valMod);
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

  switch (m_componentType) {
    case ComponentType::Int8: {
      return static_cast<float>(std::numeric_limits<int8_t>::max());
    }
    case ComponentType::Int16: {
      return static_cast<float>(std::numeric_limits<int16_t>::max());
    }
    case ComponentType::Int32: {
      return static_cast<float>(std::numeric_limits<int32_t>::max());
    }
    case ComponentType::UInt8: {
      return static_cast<float>(std::numeric_limits<uint8_t>::max());
    }
    case ComponentType::UInt16: {
      return static_cast<float>(std::numeric_limits<uint16_t>::max());
    }
    case ComponentType::UInt32: {
      return static_cast<float>(std::numeric_limits<uint32_t>::max());
    }
    case ComponentType::Float32: {
      return 1.0f;
    }
    default: {
      spdlog::error("Invalid component type {}", componentTypeString(m_componentType));
      return 1.0f;
    }
  }
}

glm::dvec2 ImageSettings::largestSlopeInterceptTextureVec2(uint32_t i) const
{
  return {m_componentSettings[i].m_largest_slope_texture, m_componentSettings[i].m_largest_intercept_texture};
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
  if (m_componentStats.size() <= i) {
    spdlog::error("Invalid image component {}", i);
    throwDebug("Invalid image component");
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
  if (component < m_numComponents) {
    m_activeComponent = component;
  }
  else {
    spdlog::error(
      "Attempting to set invalid active component {} (only {} components total for image {})",
      component,
      m_numComponents,
      m_displayName);
  }
}

void ImageSettings::updateWithNewComponentStatistics(
  std::vector<ComponentStats> componentStats,
  bool setDefaultVisibilitySettings)
{
  if (componentStats.size() != m_numComponents) {
    spdlog::error(
      "Component statistics has {} components, where {} are expected",
      componentStats.size(),
      m_numComponents);
    return;
  }

  m_componentStats = std::move(componentStats);

  if (setDefaultVisibilitySettings) {
    m_edgeDetectionMethod = EdgeDetectionMethod::Voxel;
    m_edgesVisible = false;
    m_hardEdges = false;
    m_thinPixelEdges = true;
    m_voxelEdgeScale = 4.0;
    m_voxelEdgeThreshold = 0.25;
    m_pixelEdgeScale = 2.0;
    m_pixelEdgeThreshold = 0.2;
    m_overlayEdges = false;
    m_colormapEdges = false;
    m_edgeColor = glm::vec3{1.0f, 0.0f, 1.0f};
    m_edgeOpacity = 1.0;
  }

  for (std::size_t i = 0; i < m_numComponents; ++i) {
    const auto& stats = m_componentStats[i];
    ComponentSettings& setting = m_componentSettings[i];

    // Min/max window width/center and threshold ranges are based on min/max component values:
    setting.m_minMaxImageRange =
      std::make_pair(static_cast<double>(stats.onlineStats.min), static_cast<double>(stats.onlineStats.max));
    setting.m_minMaxThresholdRange =
      std::make_pair(static_cast<double>(stats.onlineStats.min), static_cast<double>(stats.onlineStats.max));

    const double minValue = static_cast<double>(stats.onlineStats.min);
    const double maxValue = static_cast<double>(stats.onlineStats.max);
    const bool constantComponent = minValue >= maxValue;
    if (constantComponent) {
      const double width = constantImageWindowWidth(minValue);
      setting.m_minMaxWindowCenterRange = std::make_pair(minValue - width, minValue + width);
      setting.m_minMaxWindowWidthRange = std::make_pair(k_minConstantImageWindowWidth, 10.0 * width);
    }
    else {
      setting.m_minMaxWindowCenterRange = std::make_pair(minValue, maxValue);
      setting.m_minMaxWindowWidthRange = std::make_pair(0.0, maxValue - minValue);
    }

    // Default thresholds are min/max values:
    setting.m_thresholds =
      std::make_pair(static_cast<double>(stats.onlineStats.min), static_cast<double>(stats.onlineStats.max));

    // Default window limits are the low and high quantiles. Sparse binary images can have both
    // quantiles on the background value; use the full component range when the robust range
    // collapses so foreground objects are visible on first load.
    const DefaultWindow window = computeDefaultWindow(stats);

    setting.m_windowCenter = 0.5 * (window.low + window.high);
    setting.m_windowWidth = window.high - window.low;
    if (constantComponent) {
      setting.m_windowCenter = minValue;
      setting.m_windowWidth = constantImageWindowWidth(minValue);
    }
    setting.m_windowQuantilesLowHigh = {window.lowQuantile, window.highQuantile};

    setting.m_foregroundThresholds = std::make_pair(
      static_cast<double>(stats.quantiles[k_defaultWindowLowQuantileIndex]),
      static_cast<double>(stats.quantiles[k_maxQuantileIndex]));

    // Default thresholds for foreground mask that is used for the raycasting distance map:
    // Use the [50%, 100%] intensity range to define foreground
    // (until we have an algorithm to compute a foreground mask)
    setting.m_foregroundThresholds.first = static_cast<double>(stats.quantiles[50]);
    setting.m_foregroundThresholds.second = static_cast<double>(stats.quantiles[k_maxQuantileIndex]);

    // Update histogram settings
    setting.m_histogramSettings.m_numBinsMethod = NumBinsComputationMethod::FreedmanDiaconis;
    setting.m_histogramSettings.m_isCumulative = false;
    setting.m_histogramSettings.m_isDensity = false;
    setting.m_histogramSettings.m_isHorizontal = false;
    setting.m_histogramSettings.m_isLogScale = true;
    setting.m_histogramSettings.m_useCustomIntensityRange = false;
    setting.m_histogramSettings.m_intensityRange[0] = static_cast<double>(stats.onlineStats.min);
    setting.m_histogramSettings.m_intensityRange[1] = static_cast<double>(stats.onlineStats.max);

    if (0 == m_numPixels) {
      spdlog::warn(
        "Component {} of image {} has zero pixels, so setting number of histogram bins to one",
        i,
        m_displayName);
      setting.m_histogramSettings.m_numBins = 1;
    }
    else {
      std::optional<std::size_t> numBins =
        computeNumHistogramBins(setting.m_histogramSettings.m_numBinsMethod, m_numPixels, m_componentStats[i]);

      if (!numBins) {
        spdlog::debug("Could not compute histogram bin count for component {} of image {}", i, m_displayName);
        spdlog::debug("Falling back to Sturge's method for computing histogram bin count");

        setting.m_histogramSettings.m_numBinsMethod = NumBinsComputationMethod::Sturges;
        numBins =
          computeNumHistogramBins(setting.m_histogramSettings.m_numBinsMethod, m_numPixels, m_componentStats[i]);
      }

      setting.m_histogramSettings.m_numBins = static_cast<int>(
        std::min<std::size_t>(numBins.value_or(1), static_cast<std::size_t>(std::numeric_limits<int>::max())));

      setting.m_histogramSettings.m_binWidth =
        static_cast<double>(m_componentStats[i].onlineStats.max - m_componentStats[i].onlineStats.min) /
        setting.m_histogramSettings.m_numBins;
    }

    if (setDefaultVisibilitySettings) {
      // Default to max opacity and nearest neighbor interpolation
      setting.m_opacity = 1.0;
      setting.m_visible = true;

      setting.m_interpolationMode = InterpolationMode::Linear;

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

void ImageSettings::setUsingExactQuantiles(bool set)
{
  m_usingExactQuantiles = set;
}

bool ImageSettings::usingExactQuantiles() const
{
  return m_usingExactQuantiles;
}

void ImageSettings::updateInternals()
{
  for (uint32_t i = 0; i < m_componentSettings.size(); ++i) {
    auto& S = m_componentSettings[i];

    const double imageRange = S.m_minMaxImageRange.second - S.m_minMaxImageRange.first;

    if (windowWidth(i) <= 0.0) {
      S.m_slope_native = 0.0;
      S.m_intercept_native = 0.0;
      S.m_slope_texture = 0.0;
      S.m_intercept_texture = 0.0;
      S.m_largest_slope_texture = 0.0;
      S.m_largest_intercept_texture = 0.0;
      continue;
    }

    if (imageRange <= 0.0) {
      S.m_slope_native = 1.0 / windowWidth(i);
      S.m_intercept_native = 0.5 - windowCenter(i) / windowWidth(i);
      S.m_slope_texture = 0.0;
      S.m_intercept_texture = 0.5;
      S.m_largest_slope_texture = 0.0;
      S.m_largest_intercept_texture = 0.5;
      continue;
    }

    S.m_slope_native = 1.0 / windowWidth(i);
    S.m_intercept_native = 0.5 - windowCenter(i) / windowWidth(i);

    /**
     * @note Unsigned normalized values are computed as float = int / MAX,
     * where MAX = 2^B - 1 = 255 for an 8-bit value.
     *
     * Signed normalized values are computed as float = max(int / MAX, -1),
     * where MAX = 2^(B - 1) - 1 = 127 for an 8-bit value.
     */

    double M = 0.0;

    switch (m_componentType) {
      case ComponentType::Int8:
      case ComponentType::UInt8: {
        M = static_cast<double>(std::numeric_limits<uint8_t>::max());
        break;
      }
      case ComponentType::Int16:
      case ComponentType::UInt16: {
        M = static_cast<double>(std::numeric_limits<uint16_t>::max());
        break;
      }
      case ComponentType::Int32:
      case ComponentType::UInt32: {
        M = static_cast<double>(std::numeric_limits<uint32_t>::max());
        break;
      }
      case ComponentType::Float32: {
        M = 0.0;
        break;
      }
      default: {
        break;
      }
    }

    switch (m_componentType) {
      case ComponentType::Int8:
      case ComponentType::Int16:
      case ComponentType::Int32: {
        /// @todo This mapping may be slightly wrong for the signed integer case
        S.m_slope_texture = 0.5 * M / imageRange;
        S.m_intercept_texture = -(S.m_minMaxImageRange.first + 0.5) / imageRange;
        break;
      }
      case ComponentType::UInt8:
      case ComponentType::UInt16:
      case ComponentType::UInt32: {
        S.m_slope_texture = M / imageRange;
        S.m_intercept_texture = -S.m_minMaxImageRange.first / imageRange;
        break;
      }
      case ComponentType::Float32: {
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
  /// @note Some normalized signed-integer representations use this alternate mapping:
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

  switch (m_componentType) {
    case ComponentType::Int8: {
      constexpr double M = static_cast<double>(std::numeric_limits<int8_t>::max());
      return std::max(nativeImageValue / M, -1.0);
    }
    case ComponentType::Int16: {
      constexpr double M = static_cast<double>(std::numeric_limits<int16_t>::max());
      return std::max(nativeImageValue / M, -1.0);
      // return (2.0 * nativeImageValue + 1.0) / 65535.0;
    }
    case ComponentType::Int32: {
      constexpr double M = static_cast<double>(std::numeric_limits<int32_t>::max());
      return std::max(nativeImageValue / M, -1.0);
    }
    case ComponentType::UInt8: {
      constexpr double M = static_cast<double>(std::numeric_limits<uint8_t>::max());
      return nativeImageValue / M;
    }
    case ComponentType::UInt16: {
      constexpr double M = static_cast<double>(std::numeric_limits<uint16_t>::max());
      return nativeImageValue / M;
    }
    case ComponentType::UInt32: {
      constexpr double M = static_cast<double>(std::numeric_limits<uint32_t>::max());
      return nativeImageValue / M;
    }
    case ComponentType::Float32: {
      return nativeImageValue;
    }
    default: {
      spdlog::error("Invalid component type {}", componentTypeString(m_componentType));
      return nativeImageValue;
    }
  }
}

std::ostream& operator<<(std::ostream& os, const ImageSettings& settings)
{
  os << "Display name: " << settings.m_displayName;

  for (std::size_t i = 0; i < settings.m_componentStats.size(); ++i) {
    const auto& s = settings.m_componentSettings[i];
    const auto& t = settings.m_componentStats[i];

    os << "\nStatistics (component " << i << "):" << "\n\tMin: " << t.onlineStats.min << "\n\tQ01: " << t.quantiles[1]
       << "\n\tQ25: " << t.quantiles[25] << "\n\tMed: " << t.quantiles[50] << "\n\tQ75: " << t.quantiles[75]
       << "\n\tQ99: " << t.quantiles[99] << "\n\tMax: " << t.onlineStats.max << "\n\tAvg: " << t.onlineStats.mean
       << "\n\tStd: " << t.onlineStats.stdev;

    os << "\n\n\tWindow: [" << s.m_windowCenter - 0.5 * s.m_windowWidth << ", "
       << s.m_windowCenter + 0.5 * s.m_windowWidth << "]" << "\n\tThreshold: [" << s.m_thresholds.first << ", "
       << s.m_thresholds.second << "]";
  }

  return os;
}
