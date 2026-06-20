#pragma once

#include "logic/app/Settings.h"

#include "common/Types.h"

#include <filesystem>
#include <string>
#include <cstddef>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

struct RenderData;

namespace user_preferences
{

/**
 * @brief Rendering preferences persisted in the user settings file.
 *
 * This type intentionally contains only plain user-facing preference values.
 * It mirrors the persisted subset of RenderData without requiring OpenGL
 * resources, which keeps preference parsing and schema tests headless.
 */
struct RenderPreferences
{
  enum class SegMaskingForRaycasting
  {
    SegMasksIn,
    SegMasksOut,
    Disabled
  };

  struct MetricParams
  {
    std::size_t colorMapIndex = 0;
    glm::vec2 slopeIntercept{1.0f, 0.0f};
    bool invertColormap = false;
    bool continuousColormap = true;
    int colormapLevels = 8;
  };

  enum class LocalNccPresentation
  {
    Dissimilarity,
    Correlation
  };

  enum class LocalNccInvalidStyle
  {
    Transparent,
    Gray
  };

  bool showImageBorders = true;
  CrosshairsSnapping crosshairsSnapping = CrosshairsSnapping::Disabled;
  glm::vec4 crosshairsColor{0.05f, 0.6f, 1.0f, 1.0f};
  glm::vec3 background2dColor{0.1f, 0.1f, 0.1f};
  glm::vec4 background3dColor{0.0f, 0.0f, 0.0f, 0.5f};
  glm::vec4 anatomicalLabelColor{0.695f, 0.870f, 0.090f, 1.0f};
  AnatomicalLabelType anatomicalLabelType = AnatomicalLabelType::Human;

  bool showScaleBars = true;
  bool showScaleBarsInLightboxViews = false;
  glm::vec4 scaleBarColor{0.380392f, 0.858824f, 0.250980f, 1.0f};
  ScaleBarPosition scaleBarPosition = ScaleBarPosition::BottomRight;
  ScaleBarOrientation scaleBarOrientation = ScaleBarOrientation::Horizontal;
  ScaleBarTicks scaleBarTicks = ScaleBarTicks::Automatic;
  float scaleBarTargetFraction = 0.2f;
  float scaleBarMarginPx = 12.0f;

  bool showLightboxOffsetLabels = true;
  glm::vec4 lightboxOffsetLabelColor{0.75f, 0.75f, 0.75f, 0.8f};

  bool floatingPointLinearInterpolation = false;
  bool useMaximumIntensityProjectionExtent = false;
  float intensityProjectionSlabThicknessMm = 10.0f;
  float xrayEnergyKeV = 80.0f;
  float xrayWindow = 1.0f;
  float xrayLevel = 0.5f;

  bool modulateSegmentationOpacityWithImageOpacity = true;
  SegmentationOutlineStyle segmentationOutlineStyle = SegmentationOutlineStyle::Disabled;
  float segmentationInteriorOpacity = 0.2f;
  float segmentationErosionFactor = 0.5f;

  bool squaredDifference = true;
  MetricParams squaredDifferenceMetric;
  MetricParams localNccMetric;
  int localNccPatchRadius = 3;
  float localNccSampleSpacing = 1.0f;
  float localNccMinValidFraction = 0.75f;
  float localNccVarianceEpsilon = 1.0e-5f;
  bool localNccIgnoreNegativeCorrelation = true;
  LocalNccPresentation localNccPresentation = LocalNccPresentation::Dissimilarity;
  LocalNccInvalidStyle localNccInvalidStyle = LocalNccInvalidStyle::Transparent;
  bool overlayMagentaCyan = true;
  glm::ivec2 quadrants{true, true};
  int checkerboardSquares = 10;
  float flashlightRadiusFraction = 0.15f;
  bool flashlightOverlayMovingImage = true;

  bool limitFrameRate = false;
  double targetFrameTimeSeconds = 1.0 / 60.0;
  float raycastSamplingFactor = 0.5f;
  bool transparentBackgroundWhenNoHit = true;
  bool renderFrontFaces = true;
  bool renderBackFaces = true;
  SegMaskingForRaycasting segmentationMasking = SegMaskingForRaycasting::Disabled;

  bool asciiEnabled = false;
  glm::vec2 asciiCellSizePx{8.0f, 16.0f};
  int asciiCharsetIndex = 0;
  glm::vec3 asciiForegroundColor{1.0f, 1.0f, 1.0f};
  glm::vec3 asciiBackgroundColor{0.0f, 0.0f, 0.0f};
  float asciiBackgroundAlpha = 1.0f;
  bool asciiUseColormapAsForeground = false;
  bool asciiSpatialMatching = false;
  float asciiSpatialExponent = 1.0f;

  bool annotationsOnTop = false;
  bool landmarksOnTop = false;
  bool hideAnnotationVertices = false;
};

/**
 * @brief Return the built-in persisted rendering preferences.
 */
RenderPreferences defaultRenderPreferences();

/**
 * @brief Return the current user preferences as versioned JSON text.
 * @param settings Application settings to serialize.
 * @param renderPreferences Rendering preferences to serialize.
 * @return Human-readable JSON representation of the user preferences.
 */
std::string toJsonString(const AppSettings& settings, const RenderPreferences& renderPreferences);

/**
 * @brief Apply user preferences from versioned JSON text.
 * @param settings Application settings to update.
 * @param renderPreferences Rendering preferences to update.
 * @param text JSON text to parse.
 * @param error Optional destination for a parse or validation error.
 * @return True iff the text was parsed and applied successfully.
 */
bool applyJsonString(
  AppSettings& settings,
  RenderPreferences& renderPreferences,
  const std::string& text,
  std::string* error = nullptr);

/**
 * @brief Save user preferences to disk without requiring RenderData.
 * @param settings Application settings to serialize.
 * @param renderPreferences Rendering preferences to serialize.
 * @param fileName Destination JSON file.
 * @param error Optional destination for an I/O or serialization error.
 * @return True iff the preferences were written successfully.
 */
bool save(
  const AppSettings& settings,
  const RenderPreferences& renderPreferences,
  const std::filesystem::path& fileName,
  std::string* error = nullptr);

/**
 * @brief Load user preferences from disk without requiring RenderData.
 * @param settings Application settings to update.
 * @param renderPreferences Rendering preferences to update.
 * @param fileName Source JSON file.
 * @param error Optional destination for an I/O or parse error.
 * @return True iff the file was absent or was loaded successfully.
 */
bool load(
  AppSettings& settings,
  RenderPreferences& renderPreferences,
  const std::filesystem::path& fileName,
  std::string* error = nullptr);

/**
 * @brief Return the current user preferences as versioned JSON text.
 * @param settings Application settings to serialize.
 * @param renderData Rendering defaults to serialize.
 * @return Human-readable JSON representation of the user preferences.
 */
std::string toJsonString(const AppSettings& settings, const RenderData& renderData);

/**
 * @brief Apply user preferences from versioned JSON text.
 * @param settings Application settings to update.
 * @param renderData Rendering defaults to update.
 * @param text JSON text to parse.
 * @param error Optional destination for a parse or validation error.
 * @return True iff the text was parsed and applied successfully.
 */
bool applyJsonString(
  AppSettings& settings,
  RenderData& renderData,
  const std::string& text,
  std::string* error = nullptr);

/**
 * @brief Save user preferences to disk.
 * @param settings Application settings to serialize.
 * @param renderData Rendering defaults to serialize.
 * @param fileName Destination JSON file.
 * @param error Optional destination for an I/O or serialization error.
 * @return True iff the preferences were written successfully.
 */
bool save(
  const AppSettings& settings,
  const RenderData& renderData,
  const std::filesystem::path& fileName,
  std::string* error = nullptr);

/**
 * @brief Load user preferences from disk.
 * @param settings Application settings to update.
 * @param renderData Rendering defaults to update.
 * @param fileName Source JSON file.
 * @param error Optional destination for an I/O or parse error.
 * @return True iff the file was absent or was loaded successfully.
 */
bool load(
  AppSettings& settings,
  RenderData& renderData,
  const std::filesystem::path& fileName,
  std::string* error = nullptr);

/**
 * @brief Restore the built-in application preference defaults.
 * @param settings Application settings to reset.
 * @param renderData Rendering defaults to reset.
 */
void applyDefaults(AppSettings& settings, RenderData& renderData);

} // namespace user_preferences
