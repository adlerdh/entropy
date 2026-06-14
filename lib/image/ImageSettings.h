#pragma once

#include "common/HistogramSettings.h"
#include "common/Types.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <ostream>
#include <string>
#include <utility>
#include <vector>

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
    std::vector<ComponentStats> componentStats);

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

  /// @brief Set the RGB border color used when drawing image bounds.
  void setBorderColor(glm::vec3 borderColor);

  /// @brief Get the RGB border color used when drawing image bounds.
  const glm::vec3& borderColor() const;

  /// @brief Set whether the image transform is locked to the reference image.
  void setLockedToReference(bool locked);

  /// @brief Return whether the image transform is locked to the reference image.
  bool isLockedToReference() const;

  /// @brief Set whether a 3- or 4-component image should be interpreted as RGB/RGBA color.
  void setDisplayImageAsColor(bool doColor);

  /// @brief Return whether a 3- or 4-component image is interpreted as RGB/RGBA color.
  bool displayImageAsColor() const;

  /// @brief Set whether to ignore the alpha component of RGBA color images.
  void setIgnoreAlpha(bool ignore);

  /// @brief Return whether the alpha component of RGBA color images is ignored.
  bool ignoreAlpha() const;

  /// @brief Set interpolation used when displaying an image as RGB/RGBA color.
  void setColorInterpolationMode(InterpolationMode mode);

  /// @brief Get interpolation used when displaying an image as RGB/RGBA color.
  InterpolationMode colorInterpolationMode() const;

  /// @brief Set whether boundary distance maps may accelerate raycasting.
  void setUseDistanceMapForRaycasting(bool use);

  /// @brief Return whether boundary distance maps may accelerate raycasting.
  bool useDistanceMapForRaycasting() const;

  /// @brief Set global visibility for all isosurfaces associated with this image.
  void setIsosurfacesVisible(bool visible);

  /// @brief Return global visibility for all isosurfaces associated with this image.
  bool isosurfacesVisible() const;

  /// @brief Set whether isosurfaces should use the image colormap instead of per-surface colors.
  void setApplyImageColormapToIsosurfaces(bool visible);

  /// @brief Return whether isosurfaces use the image colormap instead of per-surface colors.
  bool applyImageColormapToIsosurfaces() const;

  /// @brief Set whether isocontours are shown in 2D slice views.
  void setShowIsoscontoursIn2D(bool show);

  /// @brief Return whether isocontours are shown in 2D slice views.
  bool showIsocontoursIn2D() const;

  /// @brief Set 2D isocontour line width in pixels.
  void setIsosurfaceWidthIn2d(double width);

  /// @brief Get 2D isocontour line width in pixels.
  double isoContourLineWidthIn2D() const;

  /// @brief Set opacity multiplier applied to all isosurfaces of this image.
  void setIsosurfaceOpacityModulator(float opacityMod);

  /// @brief Get opacity multiplier applied to all isosurfaces of this image.
  float isosurfaceOpacityModulator() const;

  /// @brief Get the observed native intensity range for a component.
  std::pair<double, double> minMaxImageRange(uint32_t component) const;
  /// @brief Get the observed native intensity range for the active component.
  std::pair<double, double> minMaxImageRange() const;

  /// @brief Get the allowed window-width range in native intensity units for a component.
  std::pair<double, double> minMaxWindowWidthRange(uint32_t component) const;
  /// @brief Get the allowed window-width range for the active component.
  std::pair<double, double> minMaxWindowWidthRange() const;

  /// @brief Get the allowed window-center range in native intensity units for a component.
  std::pair<double, double> minMaxWindowCenterRange(uint32_t component) const;
  /// @brief Get the allowed window-center range for the active component.
  std::pair<double, double> minMaxWindowCenterRange() const;

  /// @brief Get the allowed low/high window-value range in native intensity units for a component.
  std::pair<double, double> minMaxWindowRange(uint32_t component) const;
  /// @brief Get the allowed low/high window-value range for the active component.
  std::pair<double, double> minMaxWindowRange() const;

  /// @brief Get the allowed threshold range in native intensity units for a component.
  std::pair<double, double> minMaxThresholdRange(uint32_t component) const;
  /// @brief Get the allowed threshold range for the active component.
  std::pair<double, double> minMaxThresholdRange() const;

  /// @brief Set the low window value in native intensity units for a component.
  /// @param clampValues When true, clamp to minMaxWindowRange(component).
  void setWindowValueLow(uint32_t component, double wLow, bool clampValues = true);
  /// @brief Set the low window value for the active component.
  void setWindowValueLow(double wLow, bool clampValues = false);

  /// @brief Set the high window value in native intensity units for a component.
  /// @param clampValues When true, clamp to minMaxWindowRange(component).
  void setWindowValueHigh(uint32_t component, double wHigh, bool clampValues = true);
  /// @brief Set the high window value for the active component.
  void setWindowValueHigh(double wHigh, bool clampValues = false);

  /// @brief Get low/high window values in native intensity units for a component.
  std::pair<double, double> windowValuesLowHigh(uint32_t component) const;
  /// @brief Get low/high window values for the active component.
  std::pair<double, double> windowValuesLowHigh() const;

  /// @brief Set the lower window quantile in [0, 1] for a component.
  /// @param clampValues When true, clamp to [0, 1].
  void setWindowQuantileLow(uint32_t component, double pLow, bool clampValues = true);
  /// @brief Set the lower window quantile for the active component.
  void setWindowQuantileLow(double pLow, bool clampValues = false);

  /// @brief Set the upper window quantile in [0, 1] for a component.
  /// @param clampValues When true, clamp to [0, 1].
  void setWindowQuantileHigh(uint32_t component, double pHigh, bool clampValues = true);
  /// @brief Set the upper window quantile for the active component.
  void setWindowQuantileHigh(double pHigh, bool clampValues = false);

  /// @brief Get lower/upper window quantiles in [0, 1] for a component.
  std::pair<double, double> windowQuantilesLowHigh(uint32_t component) const;
  /// @brief Get lower/upper window quantiles for the active component.
  std::pair<double, double> windowQuantilesLowHigh() const;

  /// @brief Get window width in native intensity units for a component.
  double windowWidth(uint32_t component) const;
  /// @brief Get window width for the active component.
  double windowWidth() const;

  /// @brief Get window center, also called level, in native intensity units for a component.
  double windowCenter(uint32_t component) const;
  /// @brief Get window center for the active component.
  double windowCenter() const;

  /// @brief Set window width in native intensity units for a component, clamped to the valid range.
  void setWindowWidth(uint32_t component, double width);
  /// @brief Set window width for the active component.
  void setWindowWidth(double width);

  /// @brief Set window center in native intensity units for a component, clamped to the valid range.
  void setWindowCenter(uint32_t component, double center);
  /// @brief Set window center for the active component.
  void setWindowCenter(double center);

  /// @brief Set low threshold in native intensity units for a component.
  void setThresholdLow(uint32_t component, double thresh);
  /// @brief Set low threshold for the active component.
  void setThresholdLow(double thresh);

  /// @brief Set high threshold in native intensity units for a component.
  void setThresholdHigh(uint32_t component, double thresh);
  /// @brief Set high threshold for the active component.
  void setThresholdHigh(double thresh);

  /// @brief Get low/high thresholds in native intensity units for a component.
  std::pair<double, double> thresholds(uint32_t component) const;
  /// @brief Get low/high thresholds for the active component.
  std::pair<double, double> thresholds() const;

  /// @brief Return whether thresholds restrict the full native intensity range for a component.
  bool thresholdsActive(uint32_t component) const;
  /// @brief Return whether thresholds are active for the active component.
  bool thresholdsActive() const;

  /// @brief Set component opacity in [0, 1].
  void setOpacity(uint32_t component, double opacity);
  /// @brief Set opacity for the active component.
  void setOpacity(double opacity);

  /// @brief Get component opacity in [0, 1].
  double opacity(uint32_t component) const;
  /// @brief Get opacity for the active component.
  double opacity() const;

  /// @brief Set component visibility.
  void setVisibility(uint32_t component, bool visible);
  /// @brief Set visibility for the active component.
  void setVisibility(bool visible);

  /// @brief Get component visibility.
  bool visibility(uint32_t component) const;
  /// @brief Get visibility for the active component.
  bool visibility() const;

  /// @brief Set global visibility applied in addition to per-component visibility.
  void setGlobalVisibility(bool visible);

  /// @brief Get global visibility applied in addition to per-component visibility.
  bool globalVisibility() const;

  /// @brief Set global opacity multiplier in [0, 1].
  void setGlobalOpacity(double opacity);

  /// @brief Get global opacity multiplier in [0, 1].
  double globalOpacity() const;

  /// @brief Set whether edge visualization is enabled for a component.
  void setShowEdges(uint32_t component, bool show);
  /// @brief Set edge visualization for the active component.
  void setShowEdges(bool show);

  /// @brief Return whether edge visualization is enabled for a component.
  bool showEdges(uint32_t component) const;
  /// @brief Return whether edge visualization is enabled for the active component.
  bool showEdges() const;

  /// @brief Set whether edge magnitudes are thresholded for a component.
  void setThresholdEdges(uint32_t component, bool threshold);
  /// @brief Set edge thresholding for the active component.
  void setThresholdEdges(bool threshold);

  /// @brief Return whether edge magnitudes are thresholded for a component.
  bool thresholdEdges(uint32_t component) const;
  /// @brief Return whether edge magnitudes are thresholded for the active component.
  bool thresholdEdges() const;

  /// @brief Set whether Frei-Chen filters are used for edge computation for a component.
  void setUseFreiChen(uint32_t component, bool use);
  /// @brief Set Frei-Chen edge computation for the active component.
  void setUseFreiChen(bool use);

  /// @brief Return whether Frei-Chen filters are used for a component.
  bool useFreiChen(uint32_t component) const;
  /// @brief Return whether Frei-Chen filters are used for the active component.
  bool useFreiChen() const;

  /// @brief Set edge magnitude threshold for a component.
  void setEdgeMagnitude(uint32_t component, double mag);
  /// @brief Set edge magnitude threshold for the active component.
  void setEdgeMagnitude(double mag);

  /// @brief Get edge magnitude threshold for a component.
  double edgeMagnitude(uint32_t component) const;
  /// @brief Get edge magnitude threshold for the active component.
  double edgeMagnitude() const;

  /// @brief Set whether edges are computed after applying window/level for a component.
  void setWindowedEdges(uint32_t component, bool windowed);
  /// @brief Set windowed edge computation for the active component.
  void setWindowedEdges(bool overlay);

  /// @brief Return whether edges are computed after window/level for a component.
  bool windowedEdges(uint32_t component) const;
  /// @brief Return whether edges are computed after window/level for the active component.
  bool windowedEdges() const;

  /// @brief Set whether edges are drawn as an overlay rather than as a standalone image.
  void setOverlayEdges(uint32_t component, bool overlay);
  /// @brief Set edge overlay mode for the active component.
  void setOverlayEdges(bool overlay);

  /// @brief Return whether edges are drawn as an overlay for a component.
  bool overlayEdges(uint32_t component) const;
  /// @brief Return whether edges are drawn as an overlay for the active component.
  bool overlayEdges() const;

  /// @brief Set whether edges use the component colormap instead of edgeColor().
  void setColormapEdges(uint32_t component, bool showEdges);
  /// @brief Set edge colormap mode for the active component.
  void setColormapEdges(bool showEdges);

  /// @brief Return whether edges use the component colormap for a component.
  bool colormapEdges(uint32_t component) const;
  /// @brief Return whether edges use the component colormap for the active component.
  bool colormapEdges() const;

  /// @brief Set solid RGB edge color for a component.
  void setEdgeColor(uint32_t component, glm::vec3 color);
  /// @brief Set solid RGB edge color for the active component.
  void setEdgeColor(glm::vec3 color);

  /// @brief Get solid RGB edge color for a component.
  glm::vec3 edgeColor(uint32_t component) const;
  /// @brief Get solid RGB edge color for the active component.
  glm::vec3 edgeColor() const;

  /// @brief Set edge opacity in [0, 1] for a component.
  void setEdgeOpacity(uint32_t component, double opacity);
  /// @brief Set edge opacity for the active component.
  void setEdgeOpacity(double opacity);

  /// @brief Get edge opacity in [0, 1] for a component.
  double edgeOpacity(uint32_t component) const;
  /// @brief Get edge opacity for the active component.
  double edgeOpacity() const;

  /// @brief Set the selected color-map index for a component.
  void setColorMapIndex(uint32_t component, std::size_t index);
  /// @brief Set the selected color-map index for the active component.
  void setColorMapIndex(std::size_t index);

  /// @brief Get the selected color-map index for a component.
  std::size_t colorMapIndex(uint32_t component) const;
  /// @brief Get the selected color-map index for the active component.
  std::size_t colorMapIndex() const;

  /// @brief Set whether the color map is sampled in reverse for a component.
  void setColorMapInverted(uint32_t component, bool inverted);
  /// @brief Set colormap inversion for the active component.
  void setColorMapInverted(bool inverted);

  /// @brief Return whether the color map is sampled in reverse for a component.
  bool isColorMapInverted(uint32_t component) const;
  /// @brief Return whether the active component color map is sampled in reverse.
  bool isColorMapInverted() const;

  /// @brief Set the number of quantization levels used when the component colormap is discrete.
  void setColorMapQuantization(uint32_t component, uint32_t levels);
  /// @brief Set the number of quantization levels for the active component.
  void setColorMapQuantizationLevels(uint32_t levels);

  /// @brief Get the number of quantization levels for a component colormap.
  std::size_t colorMapQuantizationLevels(uint32_t component) const;
  /// @brief Get the number of quantization levels for the active component.
  std::size_t colorMapQuantizationLevels() const;

  /// @brief Set whether a component colormap is sampled continuously.
  void setColorMapContinuous(uint32_t component, bool continuous);
  /// @brief Set continuous/discrete colormap sampling for the active component.
  void setColorMapContinuous(bool continuous);

  /// @brief Return whether a component colormap is sampled continuously.
  bool colorMapContinuous(uint32_t component) const;
  /// @brief Return whether the active component colormap is sampled continuously.
  bool colorMapContinuous() const;

  /// @brief Set the hue modification factor for a component colormap.
  void setColorMapHueModFactor(uint32_t component, double hueMod);
  /// @brief Set the saturation modification factor for a component colormap.
  void setColorMapSatModFactor(uint32_t component, double satMod);
  /// @brief Set the value/brightness modification factor for a component colormap.
  void setColorMapValModFactor(uint32_t component, double valMod);

  /// @brief Set the hue modification factor for the active component.
  void setColorMapHueModFactor(double hueMod);
  /// @brief Set the saturation modification factor for the active component.
  void setColorMapSatModFactor(double satMod);
  /// @brief Set the value/brightness modification factor for the active component.
  void setColorMapValModFactor(double valMod);

  /// @brief Set hue, saturation, and value modification factors for a component colormap.
  void setColormapHsvModfactors(uint32_t component, const glm::vec3& hsvMods);
  /// @brief Set hue, saturation, and value modification factors for the active component.
  void setColormapHsvModfactors(const glm::vec3& hsvMods);

  /// @brief Get hue, saturation, and value modification factors for a component colormap.
  const glm::vec3& colorMapHsvModFactors(uint32_t component) const;
  /// @brief Get hue, saturation, and value modification factors for the active component.
  const glm::vec3& colorMapHsvModFactors() const;

  /// @brief Set selected label-table index for a segmentation component.
  void setLabelTableIndex(uint32_t component, std::size_t index);
  /// @brief Set selected label-table index for the active component.
  void setLabelTableIndex(std::size_t index);

  /// @brief Get selected label-table index for a segmentation component.
  std::size_t labelTableIndex(uint32_t component) const;
  /// @brief Get selected label-table index for the active component.
  std::size_t labelTableIndex() const;

  /// @brief Set scalar interpolation mode for a component.
  void setInterpolationMode(uint32_t component, InterpolationMode mode);
  /// @brief Set scalar interpolation mode for the active component.
  void setInterpolationMode(InterpolationMode mode);

  /// @brief Get scalar interpolation mode for a component.
  InterpolationMode interpolationMode(uint32_t component) const;
  /// @brief Get scalar interpolation mode for the active component.
  InterpolationMode interpolationMode() const;

  /// Get window/level slope 'm' and intercept 'b' for a given component.
  /// These are used to map NATIVE (raw) image intensity units 'x' to NORMALIZED units 'y' in the
  /// range [0, 1]: y = m*x + b
  /// after window/level have been applied
  std::pair<double, double> slopeIntercept_normalized_T_native(uint32_t component) const;
  std::pair<double, double> slopeIntercept_normalized_T_native() const;

  /// Get the slope/intercept mapping from native intensity to render-buffer intensity.
  // std::pair<double, double> slopeIntercept_texture_T_native() const;

  /// Get normalized window/level slope 'm' and intercept 'b' for a given component.
  /// These are used to map render-buffer intensity units 'x' to normalized units 'y' in the
  /// normalized range [0, 1]: y = m*x + b
  /// after window/level have been applied
  std::pair<double, double> slopeIntercept_normalized_T_texture(uint32_t component) const;
  std::pair<double, double> slopeIntercept_normalized_T_texture() const;

  /// @brief Get slope mapping render-buffer intensity to native intensity without window/level.
  float slope_native_T_texture() const;

  /// @brief Get normalized_T_texture slope/intercept as a glm::dvec2 for a component.
  glm::dvec2 slopeInterceptVec2_normalized_T_texture(uint32_t component) const;
  /// @brief Get normalized_T_texture slope/intercept as a glm::dvec2 for the active component.
  glm::dvec2 slopeInterceptVec2_normalized_T_texture() const;

  /// @brief Get slope/intercept for the widest possible texture-space window for a component.
  glm::dvec2 largestSlopeInterceptTextureVec2(uint32_t component) const;
  /// @brief Get slope/intercept for the widest possible texture-space window for the active component.
  glm::dvec2 largestSlopeInterceptTextureVec2() const;

  /// @brief Get foreground threshold range in native intensity units for a component.
  std::pair<double, double> thresholdRange(uint32_t component) const;
  /// @brief Get foreground threshold range for the active component.
  std::pair<double, double> thresholdRange() const;

  /// Set foreground low threshold (in native image intensity units) for a given component.
  void setForegroundThresholdLow(uint32_t component, double background);
  void setForegroundThresholdLow(double background);

  /// Set foreground high threshold (in native image intensity units) for a given component.
  void setForegroundThresholdHigh(uint32_t component, double background);
  void setForegroundThresholdHigh(double background);

  /// Get foreground thresholds (in native image intensity units) for a given component
  std::pair<double, double> foregroundThresholds(uint32_t component) const;
  std::pair<double, double> foregroundThresholds() const;

  /// @brief Get the number of components per pixel tracked by these settings.
  uint32_t numComponents() const;

  /// @brief Get statistics for an image component.
  /// @pre The component must be in [0, numComponents()).
  const ComponentStats& componentStatistics(uint32_t component) const;
  /// @brief Get statistics for the active component.
  const ComponentStats& componentStatistics() const;

  /// @brief Get read-only histogram settings for an image component.
  const HistogramSettings& histogramSettings(uint32_t component) const;
  /// @brief Get mutable histogram settings for an image component.
  HistogramSettings& histogramSettings(uint32_t component);

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
  void setUsingExactQuantiles(bool);
  /// @brief Return whether exact quantiles are available for window/level editing.
  bool usingExactQuantiles() const;

  /// Map a native image value to its normalized render-buffer representation.
  /// This mapping accounts for component type.
  /// @see https://www.khronos.org/opengl/wiki/Normalized_Integer
  double mapNativeIntensityToTexture(double nativeImageValue) const;

  friend std::ostream& operator<<(std::ostream&, const ImageSettings&);

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
    double m_slope_native{0.0};     //!< Slope (m) computed from window
    double m_intercept_native{0.0}; //!< Intercept (b) computed from window and level

    /// @note The following slope (m) and intercept (b) are used to map image TEXTURE intensity
    /// values (x) into the range [0.0, 1.0], via m*x + b
    double m_slope_texture{0.0};     //!< Slope computed from window
    double m_intercept_texture{0.0}; //!< Intercept computed from window and level

    /// @note The following values of slope (m) and intercept (b) are used to map image TEXTURE
    /// intensity values (x) into the range [0.0, 1.0], via m*x + b These values represent the
    /// largest window possible
    double m_largest_slope_texture{0.0};     //!< Slope computed from window
    double m_largest_intercept_texture{0.0}; //!< Intercept computed from window and level

    double m_opacity{0.0}; //!< Opacity in range [0.0, 1.0]
    bool m_visible{false}; //!< Visibility flag (show/hide the component)

    bool m_showEdges{false};      //!< Flag to show edges
    bool m_thresholdEdges{false}; //!< Flag to threshold edges
    bool m_useFreiChen{false};    //!< Flag to use Frei-Chen filters
    double m_edgeMagnitude{0.0};  //!< Magnitude of edges to show [0.0, 4.0] if thresholding is turned one
    bool m_windowedEdges{false};  //!< Flag to compute edges after applying windowing (width/level) to the image
    bool m_overlayEdges{false};   //!< Flag to overlay edges atop image (true) or show edges on their own (false)
    bool m_colormapEdges{false};  //!< Flag to apply colormap to edges (true) or to render edges with
                                  //!< a solid color (false)
    glm::vec3 m_edgeColor{0.0f};  //!< Edge color (used if not rendering edges using colormap)
    double m_edgeOpacity{0.0};    //!< Edge opacity: only applies when shown as an overlay atop the image

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

  // The following settings only apply to images with 3 or 4 components:
  bool m_displayAsColor{false};                                          //!< Display the image as RGB/RGBA color
  bool m_ignoreAlpha{false};                                             //!< Ignore the alpha component of the image
  InterpolationMode m_colorInterpolationMode{InterpolationMode::Linear}; //!< Interpolation mode

  // These apply to the image's isosurfaces:
  bool m_useDistanceMapForRaycasting{true}; //!< Use the distance map to accelerate raycasting of the image

  bool m_isosurfacesVisible{true};               //!< Visibility of image isosurfaces
  bool m_applyImageColormapToIsosurfaces{false}; //!< Color image isosurfaces using the image colormap
  bool m_showIsocontoursIn2D{true};              //!< Visibility of isosurface edges in 2D image slices

  /// Width of isovalue lines in 2D, roughly in terms of pixels
  double m_isocontourLineWidthIn2D{2.0};
  float m_isosurfaceOpacityModulator{1.0f}; //!< Modulator of surface opacity for the image
  /*** End settings for all components ***/

  std::size_t m_numPixels;                            //!< Number of pixels in the image (and hence in each component)
  uint32_t m_numComponents;                           //!< Number of components per pixel
  ComponentType m_componentType;                      //!< Component type
  std::vector<ComponentStats> m_componentStats;       //!< Per-component statistics
  std::vector<ComponentSettings> m_componentSettings; //!< Per-component settings

  uint32_t m_activeComponent{0}; //!< Active component

  /// Exact quantiles requires sorted buffers
  bool m_usingExactQuantiles = false;
};

std::ostream& operator<<(std::ostream&, const ImageSettings&);

#include <spdlog/fmt/ostr.h>
#if FMT_VERSION >= 90000
template<>
struct fmt::formatter<ImageSettings> : ostream_formatter
{
};
#endif
