#pragma once

#include "common/HistogramSettings.h"
#include "common/Types.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <ostream>
#include <string>
#include <utility>
#include <vector>

/// @brief Image edge-detection space used by edge rendering.
enum class EdgeDetectionMethod
{
  Voxel,
  ScreenPixel
};

/// @brief Multi-component image rendering strategy.
enum class ComponentRenderMode
{
  SingleComponent,
  Color,
  Minimum,
  Mean,
  Maximum,
  Magnitude,
  ComplexPhase,
  ComplexReal,
  ComplexImaginary,
  VectorDirectionColor,
  VectorSignedNormalProjection,
  VectorPlanarProjectionColor,
  VectorJacobianDeterminant,
  VectorGradientMagnitude,
  VectorDivergence,
  VectorCurlMagnitude,
  VectorLaplacianMagnitude
};

/// @brief Display units for complex-valued image phase.
enum class ComplexPhaseUnit
{
  Radians,
  Degrees
};

/// @brief Display range convention for complex-valued image phase.
enum class ComplexPhaseRange
{
  Signed,
  Unsigned
};

/// @brief Units used to space vector-field arrows on slice views.
enum class VectorArrowOverlaySpacingMode
{
  Pixels,
  Voxels,
  Millimeters
};

/// @brief Convention used to warp vector-field grid overlays.
enum class VectorWarpedGridConvention
{
  SamplingField,
  ApparentDeformation
};

/**
 * @brief Per-image display, visualization, and per-component intensity settings.
 *
 * ImageSettings stores UI-facing state derived from image statistics: window/level ranges,
 * thresholds, opacity, visibility, edge display, colormap settings, label-table indices, histogram
 * settings, and global isosurface display options. Methods that take a component index operate on
 * that component directly; overloads without a component index operate on activeComponent().
 *
 * Setters clamp user-provided values to valid ranges where the setting has a bounded domain.
 */
class ImageSettings
{
public:
  /// @brief Construct empty settings. Intended for containers before real image metadata is known.
  explicit ImageSettings() = default;

  /**
   * @brief Construct settings initialized from image size, component type, and component statistics.
   * @param displayName Short image name shown in UI.
   * @param numPixels Number of pixels in each component buffer.
   * @param numComponents Number of components per pixel.
   * @param componentType In-memory component type used for value normalization.
   * @param componentStats Per-component statistics; size must equal \p numComponents.
   * @throws Debug exception if \p numPixels is zero or \p componentStats has the wrong size.
   */
  ImageSettings(
    std::string displayName,
    std::size_t numPixels,
    uint32_t numComponents,
    ComponentType componentType,
    const std::vector<ComponentStats>& componentStats);

  ImageSettings(const ImageSettings&) = default;
  ImageSettings& operator=(const ImageSettings&) = default;

  ImageSettings(ImageSettings&&) = default;
  ImageSettings& operator=(ImageSettings&&) = default;

  ~ImageSettings() = default;

  /// @brief Set the short display name of the image.
  void setDisplayName(std::string name);

  /// @brief Get the short display name of the image.
  const std::string& displayName() const;

  /// @brief Get the in-memory component type for all component buffers.
  ComponentType componentType() const;

  /// @brief Set the active time point used for display.
  void setActiveTimePoint(uint32_t timePoint);

  /// @brief Get the active time point used for display.
  uint32_t activeTimePoint() const;

  /// @brief Set whether time playback loops when it reaches the last frame.
  void setTimePlaybackLoop(bool loop);

  /// @brief Return whether time playback loops when it reaches the last frame.
  bool timePlaybackLoop() const;

  /// @brief Set whether time-series playback is actively advancing frames.
  void setTimePlaybackPlaying(bool playing);

  /// @brief Return whether time-series playback is actively advancing frames.
  bool timePlaybackPlaying() const;

  /// @brief Set the playback speed multiplier for time-series images.
  void setTimePlaybackSpeed(double speed);

  /// @brief Get the playback speed multiplier for time-series images.
  double timePlaybackSpeed() const;

  /// @brief Set the RGB border color used when drawing image bounds.
  void setBorderColor(glm::vec3 borderColorArg);

  /// @brief Get the RGB border color used when drawing image bounds.
  const glm::vec3& borderColor() const;

  /// @brief Set whether the image transform is locked to the reference image.
  void setLockedToReference(bool locked);

  /// @brief Return whether the image transform is locked to the reference image.
  bool isLockedToReference() const;

  /// @brief Set whether the assigned warp is applied when rendering this image.
  void setWarpEnabled(bool enabled);

  /// @brief Return whether the assigned warp is applied when rendering this image.
  bool warpEnabled() const;

  /// @brief Set warp strength multiplier used for render-time warping.
  void setWarpStrength(float strength);

  /// @brief Return warp strength multiplier used for render-time warping.
  float warpStrength() const;

  /// @brief Set whether the warp strength slider may exceed the stored field magnitude.
  void setAllowExaggeratedWarp(bool allow);

  /// @brief Return whether warp strength values above 1.0x are allowed.
  bool allowExaggeratedWarp() const;

  /// @brief Set whether a 3- or 4-component image should be interpreted as RGB/RGBA color.
  void setDisplayImageAsColor(bool doColor);

  /// @brief Return whether a 3- or 4-component image is interpreted as RGB/RGBA color.
  bool displayImageAsColor() const;

  /// @brief Set the rendering strategy for a multi-component image.
  void setComponentRenderMode(ComponentRenderMode mode);

  /// @brief Get the rendering strategy for a multi-component image.
  ComponentRenderMode componentRenderMode() const;

  /// @brief Show vector-field arrows over the current image slice.
  void setVectorArrowOverlayVisible(bool visible);

  /// @brief Return whether vector-field arrows are shown over the image.
  bool vectorArrowOverlayVisible() const;

  /// @brief Set whether vector arrows are drawn over the rendered image.
  void setVectorArrowOverlayOnImage(bool overlayOnImage);

  /// @brief Return whether vector arrows are drawn over the rendered image.
  bool vectorArrowOverlayOnImage() const;

  /// @brief Set approximate spacing between vector arrows, in screen pixels.
  void setVectorArrowOverlayDensity(float densityPx);

  /// @brief Return approximate spacing between vector arrows, in screen pixels.
  float vectorArrowOverlayDensity() const;

  /// @brief Set approximate spacing between vector arrows, in image voxels.
  void setVectorArrowOverlayVoxelSpacing(float spacingVoxels);

  /// @brief Return approximate spacing between vector arrows, in image voxels.
  float vectorArrowOverlayVoxelSpacing() const;

  /// @brief Set approximate spacing between vector arrows, in subject millimeters.
  void setVectorArrowOverlayMillimeterSpacing(float spacingMm);

  /// @brief Return approximate spacing between vector arrows, in subject millimeters.
  float vectorArrowOverlayMillimeterSpacing() const;

  /// @brief Set units used by the vector arrow spacing value.
  void setVectorArrowOverlaySpacingMode(VectorArrowOverlaySpacingMode mode);

  /// @brief Return units used by the vector arrow spacing value.
  VectorArrowOverlaySpacingMode vectorArrowOverlaySpacingMode() const;

  /// @brief Set the fixed RGB color used for vector-field arrows.
  void setVectorArrowOverlayColor(glm::vec3 color);

  /// @brief Return the fixed RGB color used for vector-field arrows.
  const glm::vec3& vectorArrowOverlayColor() const;

  /// @brief Set whether arrows are colored by vector direction instead of fixed color.
  void setVectorArrowOverlayUseDirectionColor(bool useDirectionColor);

  /// @brief Return whether arrows are colored by vector direction.
  bool vectorArrowOverlayUseDirectionColor() const;

  /// @brief Set vector arrow line thickness in screen pixels.
  void setVectorArrowOverlayLineThickness(float thicknessPx);

  /// @brief Return vector arrow line thickness in screen pixels.
  float vectorArrowOverlayLineThickness() const;

  /// @brief Set vector arrow opacity.
  void setVectorArrowOverlayOpacity(float opacityArg);

  /// @brief Return vector arrow opacity.
  float vectorArrowOverlayOpacity() const;

  /// @brief Set whether arrow lengths are proportional to vector magnitude.
  void setVectorArrowOverlayScaleByMagnitude(bool scaleByMagnitude);

  /// @brief Return whether arrow lengths are proportional to vector magnitude.
  bool vectorArrowOverlayScaleByMagnitude() const;

  /// @brief Set dimensionless vector arrow scale multiplier.
  void setVectorArrowOverlayScaleFactor(float scaleFactor);

  /// @brief Return dimensionless vector arrow scale multiplier.
  float vectorArrowOverlayScaleFactor() const;

  /// @brief Show a warped grid generated from the vector field.
  void setVectorWarpedGridVisible(bool visible);

  /// @brief Return whether a warped grid is shown.
  bool vectorWarpedGridVisible() const;

  /// @brief Set whether the warped grid is drawn over the rendered image.
  void setVectorWarpedGridOverlayOnImage(bool overlayOnImage);

  /// @brief Return whether the warped grid is drawn over the rendered image.
  bool vectorWarpedGridOverlayOnImage() const;

  /// @brief Set convention used to interpret the displacement field for grid warping.
  void setVectorWarpedGridConvention(VectorWarpedGridConvention convention);

  /// @brief Return convention used to interpret the displacement field for grid warping.
  VectorWarpedGridConvention vectorWarpedGridConvention() const;

  /// @brief Set approximate warped-grid spacing in screen pixels.
  void setVectorWarpedGridPixelSpacing(float spacingPx);

  /// @brief Return approximate warped-grid spacing in screen pixels.
  float vectorWarpedGridPixelSpacing() const;

  /// @brief Set approximate warped-grid spacing in image voxels.
  void setVectorWarpedGridVoxelSpacing(float spacingVoxels);

  /// @brief Return approximate warped-grid spacing in image voxels.
  float vectorWarpedGridVoxelSpacing() const;

  /// @brief Set warped-grid spacing in subject millimeters.
  void setVectorWarpedGridMillimeterSpacing(float spacingMm);

  /// @brief Return warped-grid spacing in subject millimeters.
  float vectorWarpedGridMillimeterSpacing() const;

  /// @brief Set units used by the warped-grid spacing value.
  void setVectorWarpedGridSpacingMode(VectorArrowOverlaySpacingMode mode);

  /// @brief Return units used by the warped-grid spacing value.
  VectorArrowOverlaySpacingMode vectorWarpedGridSpacingMode() const;

  /// @brief Set warped-grid line thickness in screen pixels.
  void setVectorWarpedGridLineThickness(float thicknessPx);

  /// @brief Return warped-grid line thickness in screen pixels.
  float vectorWarpedGridLineThickness() const;

  /// @brief Set dimensionless warped-grid displacement scale multiplier.
  void setVectorWarpedGridScaleFactor(float scaleFactor);

  /// @brief Return dimensionless warped-grid displacement scale multiplier.
  float vectorWarpedGridScaleFactor() const;

  /// @brief Set warped-grid foreground RGBA color.
  void setVectorWarpedGridForegroundColor(glm::vec4 color);

  /// @brief Return warped-grid foreground RGBA color.
  const glm::vec4& vectorWarpedGridForegroundColor() const;

  /// @brief Set warped-grid background RGBA color.
  void setVectorWarpedGridBackgroundColor(glm::vec4 color);

  /// @brief Return warped-grid background RGBA color.
  const glm::vec4& vectorWarpedGridBackgroundColor() const;

  /// @brief Set whether planar projection color preserves in-plane vector signs.
  void setVectorPlanarProjectionSignedColors(bool signedColors);

  /// @brief Return whether planar projection color preserves in-plane vector signs.
  bool vectorPlanarProjectionSignedColors() const;

  /// @brief Set whether deformation Jacobian determinant rendering uses log(det).
  void setVectorLogJacobianDeterminant(bool logJacobian);

  /// @brief Return whether deformation Jacobian determinant rendering uses log(det).
  bool vectorLogJacobianDeterminant() const;

  /// @brief Set display units for complex-valued image phase.
  void setComplexPhaseUnit(ComplexPhaseUnit unit);

  /// @brief Get display units for complex-valued image phase.
  ComplexPhaseUnit complexPhaseUnit() const;

  /// @brief Set display range convention for complex-valued image phase.
  void setComplexPhaseRange(ComplexPhaseRange range);

  /// @brief Get display range convention for complex-valued image phase.
  ComplexPhaseRange complexPhaseRange() const;

  /// @brief Set whether to ignore the alpha component of RGBA color images.
  void setIgnoreAlpha(bool ignore);

  /// @brief Return whether the alpha component of RGBA color images is ignored.
  bool ignoreAlpha() const;

  /// @brief Set interpolation used when displaying an image as RGB/RGBA color.
  void setColorInterpolationMode(InterpolationMode mode);

  /// @brief Get interpolation used when displaying an image as RGB/RGBA color.
  InterpolationMode colorInterpolationMode() const;

  /// @brief Set whether isosurfaces should use the image colormap instead of per-surface colors.
  void setApplyImageColormapToIsosurfaces(bool apply);

  /// @brief Return whether isosurfaces use the image colormap instead of per-surface colors.
  bool applyImageColormapToIsosurfaces() const;

  /// @brief Set whether isosurface opacity is scaled by this image's opacity.
  void setModulateIsosurfaceOpacityWithImageOpacity(bool modulate);

  /// @brief Return whether isosurface opacity is scaled by this image's opacity.
  bool modulateIsosurfaceOpacityWithImageOpacity() const;

  /// @brief Set 2D isocontour line width in pixels.
  void setIsosurfaceWidthIn2d(double width);

  /// @brief Get 2D isocontour line width in pixels.
  double isoContourLineWidthIn2D() const;

  /// @brief Set opacity multiplier applied to all isosurfaces of this image.
  void setIsosurfaceOpacityModulator(float opacityMod);

  /// @brief Get opacity multiplier applied to all isosurfaces of this image.
  float isosurfaceOpacityModulator() const;

  /// @brief Get the image-wide isosurface opacity multiplier, including image opacity modulation when enabled.
  float effectiveIsosurfaceOpacityModulator() const;

  /// @brief Get the observed native intensity range for a component.
  std::pair<double, double> minMaxImageRange(uint32_t i) const;
  /// @brief Get the observed native intensity range for the active component.
  std::pair<double, double> minMaxImageRange() const;

  /// @brief Get the allowed window-width range in native intensity units for a component.
  std::pair<double, double> minMaxWindowWidthRange(uint32_t i) const;
  /// @brief Get the allowed window-width range for the active component.
  std::pair<double, double> minMaxWindowWidthRange() const;

  /// @brief Get the allowed window-center range in native intensity units for a component.
  std::pair<double, double> minMaxWindowCenterRange(uint32_t i) const;
  /// @brief Get the allowed window-center range for the active component.
  std::pair<double, double> minMaxWindowCenterRange() const;

  /// @brief Get the allowed low/high window-value range in native intensity units for a component.
  std::pair<double, double> minMaxWindowRange(uint32_t i) const;
  /// @brief Get the allowed low/high window-value range for the active component.
  std::pair<double, double> minMaxWindowRange() const;

  /// @brief Get the allowed threshold range in native intensity units for a component.
  std::pair<double, double> minMaxThresholdRange(uint32_t i) const;
  /// @brief Get the allowed threshold range for the active component.
  std::pair<double, double> minMaxThresholdRange() const;

  /// @brief Set the low window value in native intensity units for a component.
  /// @param clampValues When true, clamp to minMaxWindowRange(component).
  void setWindowValueLow(uint32_t i, double wLow, bool clampValues = true);
  /// @brief Set the low window value for the active component.
  void setWindowValueLow(double wLow, bool clampValues = false);

  /// @brief Set the high window value in native intensity units for a component.
  /// @param clampValues When true, clamp to minMaxWindowRange(component).
  void setWindowValueHigh(uint32_t i, double wHigh, bool clampValues = true);
  /// @brief Set the high window value for the active component.
  void setWindowValueHigh(double wHigh, bool clampValues = false);

  /// @brief Get low/high window values in native intensity units for a component.
  std::pair<double, double> windowValuesLowHigh(uint32_t i) const;
  /// @brief Get low/high window values for the active component.
  std::pair<double, double> windowValuesLowHigh() const;

  /// @brief Set the lower window quantile in [0, 1] for a component.
  /// @param clampValues When true, clamp to [0, 1].
  static void setWindowQuantileLow(uint32_t i, double pLow, bool clampValues = true);
  /// @brief Set the lower window quantile for the active component.
  void setWindowQuantileLow(double pLow, bool clampValues = false) const;

  /// @brief Set the upper window quantile in [0, 1] for a component.
  /// @param clampValues When true, clamp to [0, 1].
  static void setWindowQuantileHigh(uint32_t i, double pHigh, bool clampValues = true);
  /// @brief Set the upper window quantile for the active component.
  void setWindowQuantileHigh(double pHigh, bool clampValues = false) const;

  /// @brief Get lower/upper window quantiles in [0, 1] for a component.
  std::pair<double, double> windowQuantilesLowHigh(uint32_t i) const;
  /// @brief Get lower/upper window quantiles for the active component.
  std::pair<double, double> windowQuantilesLowHigh() const;

  /// @brief Get window width in native intensity units for a component.
  double windowWidth(uint32_t i) const;
  /// @brief Get window width for the active component.
  double windowWidth() const;

  /// @brief Get window center, also called level, in native intensity units for a component.
  double windowCenter(uint32_t i) const;
  /// @brief Get window center for the active component.
  double windowCenter() const;

  /// @brief Set window width in native intensity units for a component, clamped to the valid range.
  void setWindowWidth(uint32_t i, double width);
  /// @brief Set window width for the active component.
  void setWindowWidth(double width);

  /// @brief Set window center in native intensity units for a component, clamped to the valid range.
  void setWindowCenter(uint32_t i, double center);
  /// @brief Set window center for the active component.
  void setWindowCenter(double center);
  /// @brief Set every component window to the supplied native intensity range.
  void setWindowRangeForAllComponents(std::pair<double, double> range);

  /// @brief Set low threshold in native intensity units for a component.
  void setThresholdLow(uint32_t i, double tLow);
  /// @brief Set low threshold for the active component.
  void setThresholdLow(double tLow);

  /// @brief Set high threshold in native intensity units for a component.
  void setThresholdHigh(uint32_t i, double tHigh);
  /// @brief Set high threshold for the active component.
  void setThresholdHigh(double tHigh);

  /// @brief Get low/high thresholds in native intensity units for a component.
  std::pair<double, double> thresholds(uint32_t i) const;
  /// @brief Get low/high thresholds for the active component.
  std::pair<double, double> thresholds() const;

  /// @brief Return whether thresholds restrict the full native intensity range for a component.
  bool thresholdsActive(uint32_t i) const;
  /// @brief Return whether thresholds are active for the active component.
  bool thresholdsActive() const;

  /// @brief Set component opacity in [0, 1].
  void setOpacity(uint32_t i, double op);
  /// @brief Set opacity for the active component.
  void setOpacity(double op);

  /// @brief Get component opacity in [0, 1].
  double opacity(uint32_t i) const;
  /// @brief Get opacity for the active component.
  double opacity() const;

  /// @brief Set component visibility.
  void setVisibility(uint32_t i, bool visible);
  /// @brief Set visibility for the active component.
  void setVisibility(bool visible);

  /// @brief Get component visibility.
  bool visibility(uint32_t i) const;
  /// @brief Get visibility for the active component.
  bool visibility() const;

  /// @brief Set global visibility applied in addition to per-component visibility.
  void setGlobalVisibility(bool visible);

  /// @brief Get global visibility applied in addition to per-component visibility.
  bool globalVisibility() const;

  /// @brief Set global opacity multiplier in [0, 1].
  void setGlobalOpacity(double opacityArg);

  /// @brief Get global opacity multiplier in [0, 1].
  double globalOpacity() const;

  /// @brief Enable or disable image edge visualization.
  void setEdgesVisible(bool visible);
  /// @brief Return whether image edge visualization is enabled.
  bool edgesVisible() const;

  /// @brief Set the image edge-rendering method.
  void setEdgeDetectionMethod(EdgeDetectionMethod method);
  /// @brief Get the image edge-rendering method.
  EdgeDetectionMethod edgeDetectionMethod() const;

  /// @brief Set whether edge magnitudes are thresholded into a binary result.
  void setHardEdges(bool hard);
  /// @brief Return whether edge magnitudes are thresholded into a binary result.
  bool hardEdges() const;

  /// @brief Set whether pixel-space edges are thinned using non-maximum suppression.
  void setThinPixelEdges(bool thin);
  /// @brief Return whether pixel-space edges are thinned.
  bool thinPixelEdges() const;

  /// @brief Set the voxel-space edge magnitude scale.
  void setVoxelEdgeScale(double scale);
  /// @brief Get the voxel-space edge magnitude scale.
  double voxelEdgeScale() const;

  /// @brief Set the voxel-space hard-edge threshold.
  void setVoxelEdgeThreshold(double threshold);
  /// @brief Get the voxel-space hard-edge threshold.
  double voxelEdgeThreshold() const;

  /// @brief Set the screen-pixel-space edge magnitude scale.
  void setPixelEdgeScale(double scale);
  /// @brief Get the screen-pixel-space edge magnitude scale.
  double pixelEdgeScale() const;

  /// @brief Set the screen-pixel-space hard-edge threshold.
  void setPixelEdgeThreshold(double threshold);
  /// @brief Get the screen-pixel-space hard-edge threshold.
  double pixelEdgeThreshold() const;

  /// @brief Set whether edges are drawn as an overlay rather than as a standalone image.
  void setOverlayEdges(bool overlay);
  /// @brief Return whether edges are drawn as an overlay.
  bool overlayEdges() const;

  /// @brief Set whether edges use the active component's colormap instead of edgeColor().
  void setColormapEdges(bool showEdgesArg);
  /// @brief Return whether edges use the active component's colormap.
  bool colormapEdges() const;

  /// @brief Set solid RGB edge color.
  void setEdgeColor(glm::vec3 color);
  /// @brief Get solid RGB edge color.
  const glm::vec3& edgeColor() const;

  /// @brief Set edge opacity in [0, 1].
  void setEdgeOpacity(double opacityArg);
  /// @brief Get edge opacity in [0, 1].
  double edgeOpacity() const;

  /// @brief Copy all edge-visualization settings from another image.
  void copyEdgeSettingsFrom(const ImageSettings& source);

  /// @brief Set the selected color-map index for a component.
  void setColorMapIndex(uint32_t i, std::size_t index);
  /// @brief Set the selected color-map index for the active component.
  void setColorMapIndex(std::size_t index);

  /// @brief Get the selected color-map index for a component.
  std::size_t colorMapIndex(uint32_t i) const;
  /// @brief Get the selected color-map index for the active component.
  std::size_t colorMapIndex() const;

  /// @brief Set whether the color map is sampled in reverse for a component.
  void setColorMapInverted(uint32_t i, bool inverted);
  /// @brief Set colormap inversion for the active component.
  void setColorMapInverted(bool inverted);

  /// @brief Return whether the color map is sampled in reverse for a component.
  bool isColorMapInverted(uint32_t i) const;
  /// @brief Return whether the active component color map is sampled in reverse.
  bool isColorMapInverted() const;

  /// @brief Set the number of quantization levels used when the component colormap is discrete.
  void setColorMapQuantization(uint32_t i, uint32_t levels);
  /// @brief Set the number of quantization levels for the active component.
  void setColorMapQuantizationLevels(uint32_t levels);

  /// @brief Get the number of quantization levels for a component colormap.
  std::size_t colorMapQuantizationLevels(uint32_t i) const;
  /// @brief Get the number of quantization levels for the active component.
  std::size_t colorMapQuantizationLevels() const;

  /// @brief Set whether a component colormap is sampled continuously.
  void setColorMapContinuous(uint32_t i, bool continuous);
  /// @brief Set continuous/discrete colormap sampling for the active component.
  void setColorMapContinuous(bool continuous);

  /// @brief Return whether a component colormap is sampled continuously.
  bool colorMapContinuous(uint32_t i) const;
  /// @brief Return whether the active component colormap is sampled continuously.
  bool colorMapContinuous() const;

  /// @brief Set the hue modification factor for a component colormap.
  void setColorMapHueModFactor(uint32_t i, double hueMod);
  /// @brief Set the saturation modification factor for a component colormap.
  void setColorMapSatModFactor(uint32_t i, double satMod);
  /// @brief Set the value/brightness modification factor for a component colormap.
  void setColorMapValModFactor(uint32_t i, double valMod);

  /// @brief Set the hue modification factor for the active component.
  void setColorMapHueModFactor(double hueMod);
  /// @brief Set the saturation modification factor for the active component.
  void setColorMapSatModFactor(double satMod);
  /// @brief Set the value/brightness modification factor for the active component.
  void setColorMapValModFactor(double valMod);

  /// @brief Set hue, saturation, and value modification factors for a component colormap.
  void setColormapHsvModfactors(uint32_t i, const glm::vec3& hsvMods);
  /// @brief Set hue, saturation, and value modification factors for the active component.
  void setColormapHsvModfactors(const glm::vec3& hsvMods);

  /// @brief Get hue, saturation, and value modification factors for a component colormap.
  const glm::vec3& colorMapHsvModFactors(uint32_t i) const;
  /// @brief Get hue, saturation, and value modification factors for the active component.
  const glm::vec3& colorMapHsvModFactors() const;

  /// @brief Set selected label-table index for a segmentation component.
  void setLabelTableIndex(uint32_t i, std::size_t index);
  /// @brief Set selected label-table index for the active component.
  void setLabelTableIndex(std::size_t index);

  /// @brief Get selected label-table index for a segmentation component.
  std::size_t labelTableIndex(uint32_t i) const;
  /// @brief Get selected label-table index for the active component.
  std::size_t labelTableIndex() const;

  /// @brief Set scalar interpolation mode for a component.
  void setInterpolationMode(uint32_t i, InterpolationMode mode);
  /// @brief Set scalar interpolation mode for the active component.
  void setInterpolationMode(InterpolationMode mode);

  /// @brief Get scalar interpolation mode for a component.
  InterpolationMode interpolationMode(uint32_t i) const;
  /// @brief Get scalar interpolation mode for the active component.
  InterpolationMode interpolationMode() const;

  /// Get window/level slope 'm' and intercept 'b' for a given component.
  /// These are used to map NATIVE (raw) image intensity units 'x' to NORMALIZED units 'y' in the
  /// range [0, 1]: y = m*x + b
  /// after window/level have been applied
  std::pair<double, double> slopeIntercept_normalized_T_native(uint32_t i) const;
  std::pair<double, double> slopeIntercept_normalized_T_native() const;

  /// Get the slope/intercept mapping from native intensity to render-buffer intensity.
  // std::pair<double, double> slopeIntercept_texture_T_native() const;

  /// Get normalized window/level slope 'm' and intercept 'b' for a given component.
  /// These are used to map render-buffer intensity units 'x' to normalized units 'y' in the
  /// normalized range [0, 1]: y = m*x + b
  /// after window/level have been applied
  std::pair<double, double> slopeIntercept_normalized_T_texture(uint32_t i) const;
  std::pair<double, double> slopeIntercept_normalized_T_texture() const;

  /// @brief Get slope mapping render-buffer intensity to native intensity without window/level.
  float slope_native_T_texture() const;

  /// @brief Get normalized_T_texture slope/intercept as a glm::dvec2 for a component.
  glm::dvec2 slopeInterceptVec2_normalized_T_texture(uint32_t i) const;
  /// @brief Get normalized_T_texture slope/intercept as a glm::dvec2 for the active component.
  glm::dvec2 slopeInterceptVec2_normalized_T_texture() const;

  /// @brief Get slope/intercept for the widest possible texture-space window for a component.
  glm::dvec2 largestSlopeInterceptTextureVec2(uint32_t i) const;
  /// @brief Get slope/intercept for the widest possible texture-space window for the active component.
  glm::dvec2 largestSlopeInterceptTextureVec2() const;

  /// @brief Get foreground threshold range in native intensity units for a component.
  std::pair<double, double> thresholdRange(uint32_t i) const;
  /// @brief Get foreground threshold range for the active component.
  std::pair<double, double> thresholdRange() const;

  /// Set foreground low threshold (in native image intensity units) for a given component.
  void setForegroundThresholdLow(uint32_t i, double fgThreshLow);
  void setForegroundThresholdLow(double fgThreshLow);

  /// Set foreground high threshold (in native image intensity units) for a given component.
  void setForegroundThresholdHigh(uint32_t i, double fgThreshHigh);
  void setForegroundThresholdHigh(double fgThreshHigh);

  /// Get foreground thresholds (in native image intensity units) for a given component
  std::pair<double, double> foregroundThresholds(uint32_t i) const;
  std::pair<double, double> foregroundThresholds() const;

  /// @brief Get the number of components per pixel tracked by these settings.
  uint32_t numComponents() const;

  /// @brief Get statistics for an image component.
  /// @pre The component must be in [0, numComponents()).
  const ComponentStats& componentStatistics(uint32_t i) const;
  /// @brief Get statistics for the active component.
  const ComponentStats& componentStatistics() const;

  /// @brief Get read-only histogram settings for an image component.
  const HistogramSettings& histogramSettings(uint32_t comp) const;
  /// @brief Get mutable histogram settings for an image component.
  HistogramSettings& histogramSettings(uint32_t comp);

  /// @brief Get read-only histogram settings for the active component.
  const HistogramSettings& histogramSettings() const;
  /// @brief Get mutable histogram settings for the active component.
  HistogramSettings& histogramSettings();

  /// @brief Replace component statistics and recompute dependent ranges and defaults.
  /// @param componentStats New per-component statistics; size must equal numComponents().
  /// @param setDefaultVisibilitySettings When true, reset visibility-related defaults.
  void updateWithNewComponentStatistics(std::vector<ComponentStats> componentStats, bool setDefaultVisibilitySettings);

  /// @brief Set the active component used by overloads without an explicit component index.
  void setActiveComponent(uint32_t component);

  /// @brief Get the active component used by overloads without an explicit component index.
  uint32_t activeComponent() const;

  /// @brief Set whether exact quantiles are available for window/level editing.
  void setUsingExactQuantiles(bool set);
  /// @brief Return whether exact quantiles are available for window/level editing.
  bool usingExactQuantiles() const;

  /// Map a native image value to its normalized render-buffer representation.
  /// This mapping accounts for component type.
  /// @see https://www.khronos.org/opengl/wiki/Normalized_Integer
  double mapNativeIntensityToTexture(double nativeImageValue) const;

  friend std::ostream& operator<<(std::ostream& os, const ImageSettings& settings);

private:
  void updateInternals();

  /// @brief Settings for one image component
  struct ComponentSettings
  {
    std::pair<double, double> m_minMaxImageRange{0.0, 0.0};        //!< Min/max image value range
    std::pair<double, double> m_minMaxWindowWidthRange{0.0, 0.0};  //!< Valid window width range
    std::pair<double, double> m_minMaxWindowCenterRange{0.0, 0.0}; //!< Valid window center range
    std::pair<double, double> m_minMaxThresholdRange{0.0, 0.0};    //!< Valid threshold range

    /// Window center and width in native image intensity units
    double m_windowCenter{0.0};
    double m_windowWidth{0.0};

    std::pair<double, double> m_windowQuantilesLowHigh{0.0, 0.0};

    /// Low and high threshold values in native image intensity units
    std::pair<double, double> m_thresholds{0.0, 0.0};

    /// Native image intensity value thresholds for the image foreground. A Euclidean distance
    /// map is generated from the foreground. This distance map is used to accelerate raycasting
    /// of the image's isosurfaces.
    std::pair<double, double> m_foregroundThresholds{0.0, 0.0};

    /// @note The following slope (m) and intercept (b) are used to map NATIVE image intensity
    /// values (x) into the range [0.0, 1.0], via m*x + b
    /// Slope (m) computed from window
    double m_slope_native{0.0};
    double m_intercept_native{0.0}; //!< Intercept (b) computed from window and level

    /// @note The following slope (m) and intercept (b) are used to map image TEXTURE intensity
    /// values (x) into the range [0.0, 1.0], via m*x + b
    /// Slope computed from window
    double m_slope_texture{0.0};
    double m_intercept_texture{0.0}; //!< Intercept computed from window and level

    /// @note The following values of slope (m) and intercept (b) are used to map image TEXTURE
    /// intensity values (x) into the range [0.0, 1.0], via m*x + b These values represent the
    /// largest window possible
    /// Slope computed from window
    double m_largest_slope_texture{0.0};
    double m_largest_intercept_texture{0.0}; //!< Intercept computed from window and level

    double m_opacity{0.0}; //!< Opacity in range [0.0, 1.0]
    bool m_visible{false}; //!< Visibility flag (show/hide the component)

    std::size_t m_colorMapIndex{0}; //!< Color map index
    bool m_colorMapInverted{false}; //!< Whether the color map is inverted

    bool m_colorMapContinuous{true}; //!< Whether the color map is continuous (true) or discrete (false)
    uint32_t m_numColorMapLevels{8}; //!< Number of quantization levels

    glm::vec3 m_hsvModFactors{0.0f, 1.0f, 1.0f}; //!< HSV modification factors

    std::size_t m_labelTableIndex{0}; //!< Label table index (for segmentation images only)

    /// Interpolation mode
    InterpolationMode m_interpolationMode{InterpolationMode::Linear};

    HistogramSettings m_histogramSettings; //!< Histogram calculation and display settings
  };

  /*** Start settings that apply for all components ***/
  std::string m_displayName;                 //!< Display name of the image in the UI
  bool m_globalVisibility{true};             //!< Global visibility
  float m_globalOpacity{1.0f};               //!< Global opacity
  glm::vec3 m_borderColor{1.0f, 0.0f, 1.0f}; //!< Border color
  bool m_lockedToReference{true};            //!< Lock this image to the reference image
  bool m_warpEnabled{true};                  //!< Apply the assigned warp while rendering
  float m_warpStrength{1.0f};                //!< Warp strength multiplier
  bool m_allowExaggeratedWarp{false};        //!< Permit warp strength greater than 1.0x

  ComponentRenderMode m_componentRenderMode{ComponentRenderMode::SingleComponent}; //!< Multi-component render mode
  ComplexPhaseUnit m_complexPhaseUnit{ComplexPhaseUnit::Radians};   //!< Display units for complex phase values
  ComplexPhaseRange m_complexPhaseRange{ComplexPhaseRange::Signed}; //!< Display range for complex phase values
  bool m_vectorArrowOverlayVisible{false};                          //!< Show vector-field arrows on slices
  bool m_vectorArrowOverlayOnImage{true};                           //!< Draw arrows over the rendered image
  float m_vectorArrowOverlayDensity{32.0f};                         //!< Arrow spacing in screen pixels
  float m_vectorArrowOverlayVoxelSpacing{4.0f};                     //!< Arrow spacing in image voxels
  float m_vectorArrowOverlayMillimeterSpacing{10.0f};               //!< Arrow spacing in subject millimeters
  VectorArrowOverlaySpacingMode m_vectorArrowOverlaySpacingMode{
    VectorArrowOverlaySpacingMode::Voxels};                //!< Spacing units
  glm::vec3 m_vectorArrowOverlayColor{1.0f, 0.86f, 0.31f}; //!< Fixed vector arrow color
  bool m_vectorArrowOverlayUseDirectionColor{false};       //!< Color arrows by vector direction
  float m_vectorArrowOverlayLineThickness{1.4f};           //!< Arrow line thickness in screen pixels
  float m_vectorArrowOverlayOpacity{1.0f};                 //!< Arrow opacity multiplier
  bool m_vectorArrowOverlayScaleByMagnitude{true};         //!< Scale arrow length by magnitude
  float m_vectorArrowOverlayScaleFactor{1.0f};             //!< Dimensionless arrow scale multiplier
  bool m_vectorWarpedGridVisible{false};                   //!< Show warped vector-field grid
  bool m_vectorWarpedGridOverlayOnImage{true};             //!< Draw warped grid over the rendered image
  VectorWarpedGridConvention m_vectorWarpedGridConvention{
    VectorWarpedGridConvention::SamplingField};    //!< Warped grid convention
  float m_vectorWarpedGridPixelSpacing{32.0f};     //!< Grid spacing in screen pixels
  float m_vectorWarpedGridVoxelSpacing{4.0f};      //!< Grid spacing in image voxels
  float m_vectorWarpedGridMillimeterSpacing{4.0f}; //!< Grid spacing in subject millimeters
  VectorArrowOverlaySpacingMode m_vectorWarpedGridSpacingMode{
    VectorArrowOverlaySpacingMode::Voxels};    //!< Grid spacing units
  float m_vectorWarpedGridLineThickness{1.5f}; //!< Grid line thickness in screen pixels
  float m_vectorWarpedGridScaleFactor{1.0f};   //!< Dimensionless grid warp scale multiplier
  glm::vec4 m_vectorWarpedGridForegroundColor{209.0f / 255.0f, 79.0f / 255.0f, 1.0f, 1.0f}; //!< Grid line RGBA color
  glm::vec4 m_vectorWarpedGridBackgroundColor{0.0f};                     //!< Grid background RGBA color
  bool m_vectorPlanarProjectionSignedColors{true};                       //!< Preserve in-plane vector signs in color
  bool m_vectorLogJacobianDeterminant{false};                            //!< Show log deformation Jacobian determinant
  bool m_ignoreAlpha{false};                                             //!< Ignore the alpha component of the image
  InterpolationMode m_colorInterpolationMode{InterpolationMode::Linear}; //!< Interpolation mode

  EdgeDetectionMethod m_edgeDetectionMethod{EdgeDetectionMethod::Voxel}; //!< Selected edge-rendering method
  bool m_edgesVisible{false};                                            //!< Show image edges
  bool m_hardEdges{false};                 //!< Threshold edge magnitude into a binary result
  bool m_thinPixelEdges{true};             //!< Thin screen-space edges
  double m_voxelEdgeScale{4.0};            //!< Voxel-space edge magnitude scale
  double m_voxelEdgeThreshold{0.25};       //!< Voxel-space hard-edge threshold
  double m_pixelEdgeScale{2.0};            //!< Screen-space edge magnitude scale
  double m_pixelEdgeThreshold{0.2};        //!< Screen-space hard-edge threshold
  bool m_overlayEdges{false};              //!< Overlay edges atop the image
  bool m_colormapEdges{false};             //!< Color edges with the image colormap
  glm::vec3 m_edgeColor{1.0f, 0.0f, 1.0f}; //!< Solid edge color
  double m_edgeOpacity{1.0};               //!< Edge opacity

  // These apply to the image's isosurfaces:

  bool m_applyImageColormapToIsosurfaces{false};           //!< Color image isosurfaces using the image colormap
  bool m_modulateIsosurfaceOpacityWithImageOpacity{false}; //!< Scale isosurface opacity by image opacity

  /// Width of isovalue lines in 2D, roughly in terms of pixels
  double m_isocontourLineWidthIn2D{2.0};
  float m_isosurfaceOpacityModulator{1.0f}; //!< Modulator of surface opacity for the image
  /*** End settings for all components ***/

  std::size_t m_numPixels = 0;  //!< Number of pixels in the image (and hence in each component)
  uint32_t m_numComponents = 0; //!< Number of components per pixel
  ComponentType m_componentType = ComponentType::Undefined; //!< Component type
  std::vector<ComponentStats> m_componentStats;             //!< Per-component statistics
  std::vector<ComponentSettings> m_componentSettings;       //!< Per-component settings

  uint32_t m_activeComponent{0};     //!< Active component
  uint32_t m_activeTimePoint{0};     //!< Active time point for time-series display
  bool m_timePlaybackLoop{true};     //!< Loop playback at the last time point
  bool m_timePlaybackPlaying{false}; //!< Whether time-series playback is running
  double m_timePlaybackSpeed{1.0};   //!< Time playback speed multiplier

  /// Exact quantiles requires sorted buffers
  bool m_usingExactQuantiles = false;
};

std::ostream& operator<<(std::ostream& os, const ImageSettings& settings);

#include <spdlog/fmt/ostr.h>
#if FMT_VERSION >= 90000
template<>
struct fmt::formatter<ImageSettings> : ostream_formatter
{
};
#endif
