#pragma once

#include <glm/fwd.hpp>

#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

/**
 * @brief CPU-side image color map definition used to map scalar image values to RGBA colors.
 *
 * ImageColorMap owns a non-empty table of RGBA colors represented as 32-bit floating point values.
 * RGB constructor inputs are promoted to opaque RGBA colors. RGBA inputs are stored exactly as
 * supplied; color values are not alpha-premultiplied by this class.
 */
class ImageColorMap
{
public:
  /**
   * @brief Interpolation mode requested when sampling between adjacent color entries.
   */
  enum class InterpolationMode
  {
    Nearest, //!< Use nearest-neighbor sampling
    Linear   //!< Use linear interpolation between adjacent colors
  };

  /**
   * @brief Construct color map from a vector of RGB 32-bit float tuples.
   * The alpha component of each color is assumed to be 1.0.
   *
   * @param name Short name of color map
   * @param technicalName Technically descriptive name of color map
   * @param description More lengthy description of color map
   * @param interpMode Interpolation mode used when sampling this color map.
   * @param colors Vector of colors, represented as RGB tuples with components
   * in range [0.0, 1.0]. The color's alpha value is assumed to be 1.0.
   * @throws Debug exception if \p colors is empty.
   */
  ImageColorMap(
    const std::string& name,
    const std::string& technicalName,
    const std::string& description,
    InterpolationMode interpMode,
    std::vector<glm::vec3> colors);

  /**
   * @brief Construct color map from a vector of RGBA 32-bit float tuples.
   * @param name Short name of color map
   * @param technicalName Technically descriptive name for color map
   * @param description More lengthy description of color map
   * @param interpMode Interpolation mode used when sampling this color map.
   * @param colors Vector of colors, represented as RGBA tuples
   * with components in range [0.0, 1.0]
   * @throws Debug exception if \p colors is empty.
   */
  ImageColorMap(
    const std::string& name,
    const std::string& technicalName,
    const std::string& description,
    InterpolationMode interpMode,
    std::vector<glm::vec4> colors);

  ImageColorMap(const ImageColorMap&) = default;
  ImageColorMap& operator=(const ImageColorMap&) = default;

  ImageColorMap(ImageColorMap&&) = default;
  ImageColorMap& operator=(ImageColorMap&&) = default;

  ~ImageColorMap() = default;

  /// @brief Set the compact preview map shown in UI controls for this color map.
  /// @param preview Vector of RGBA preview colors with components in range [0.0, 1.0].
  void setPreviewMap(std::vector<glm::vec4> preview);

  /// @brief Return whether a non-empty preview map has been assigned.
  bool hasPreviewMap() const;

  /// @brief Get the number of colors in the preview map.
  std::size_t numPreviewMapColors() const;

  /// @brief Get a pointer to the first float of the contiguous preview RGBA buffer.
  /// @pre hasPreviewMap() must be true.
  /// @return Pointer to 4 * numPreviewMapColors() floats.
  const float* getPreviewMap() const;

  /// @brief Get the short user-facing name of the color map.
  const std::string& name() const;

  /// @brief Get the stable technical name of the color map.
  const std::string& technicalName() const;

  /// @brief Get the descriptive text associated with the color map.
  const std::string& description() const;

  /// @brief Get the number of entries in the color table.
  std::size_t numColors() const;

  /// @brief Get the RGBA color at a color-table index.
  /// @throws Debug exception if \p index is outside [0, numColors()).
  glm::vec4 color_RGBA_F32(std::size_t index) const;

  /// @brief Get the total byte count of the contiguous RGBA float color buffer.
  std::size_t numBytes_RGBA_F32() const;

  /// @brief Get a pointer to the first float of the contiguous RGBA color buffer.
  /// @return Pointer to 4 * numColors() floats.
  const float* data_RGBA_F32() const;

  /// @brief Get read-only access to the owned RGBA color table.
  const std::vector<glm::vec4>& data_RGBA_asVector() const;

  /// @brief Replace a color-table entry.
  /// @return True when \p i is valid and the color was updated; false otherwise.
  bool setColorRGBA(std::size_t i, const glm::vec4& rgba);

  /// @brief Get slope/intercept coefficients for normalized color-map coordinate mapping.
  /// @param inverted When true, returns coefficients that invert the normalized coordinate.
  glm::vec2 slopeIntercept(bool inverted = false) const;

  /// @brief Set whether sampling beyond the map border should be treated as transparent.
  void setTransparentBorder(bool set);

  /// @brief Return whether transparent-border sampling is enabled.
  bool transparentBorder() const;

  /// @brief Cyclically rotate the color table by a fractional amount of its length.
  /// @param fraction Fractional rotation amount. Values outside [0, 1) are wrapped.
  void cyclicRotate(float fraction);

  /// @brief Reverse the order of entries in the color table.
  void reverse();

  /// @brief Set the interpolation mode used for sampling this color map.
  void setInterpolationMode(const InterpolationMode& mode);

  /// @brief Get the interpolation mode used for sampling this color map.
  InterpolationMode interpolationMode() const;

  /// @brief Load a color map from CSV text.
  ///
  /// The first three lines are the brief name, technical name, and description. Each remaining line
  /// must contain either RGB or RGBA floating point components. RGB rows are promoted to alpha 1.
  /// Header text is sanitized to alphanumeric characters plus spaces, '-', '_', '(' and ')'.
  ///
  /// @return A color map on success, or std::nullopt when metadata or color rows are invalid.
  static std::optional<ImageColorMap> loadImageColorMap(std::istringstream& csv);

  /// @brief Create a linear color map between two endpoint colors.
  /// @param startColor First color-table entry.
  /// @param endColor Last color-table entry.
  /// @param numSteps Requested number of color-table entries. Values less than 2 are promoted to 2.
  /// @param briefName Short user-facing name.
  /// @param description Descriptive text.
  /// @param technicalName Stable technical name.
  static ImageColorMap createLinearImageColorMap(
    const glm::vec4& startColor,
    const glm::vec4& endColor,
    std::size_t numSteps,
    std::string briefName,
    std::string description,
    std::string technicalName);

private:
  std::string m_name;          //!< Short name of the color map
  std::string m_technicalName; //!< Technical name of the color map
  std::string m_description;   //!< Description of the color map

  /// Table of RGBA colors represented using 32-bit floating point values per component.
  /// Components are only meaningful if in range [0.0, 1.0]
  std::vector<glm::vec4> m_colors_RGBA_F32;

  std::vector<glm::vec4> m_preview;      //!< Optional preview color map
  InterpolationMode m_interpolationMode; //!< Interpolation mode
  bool m_transparentBorder;              //!< Whether border sampling should be transparent
};
