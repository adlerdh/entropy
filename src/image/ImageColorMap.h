#ifndef IMAGE_COLORMAP_H
#define IMAGE_COLORMAP_H

#include "rendering/utility/gl/GLTextureTypes.h"

#include <glm/fwd.hpp>

#include <optional>
#include <sstream>
#include <string>
#include <vector>

/**
 * @brief Class for an image color map. Each color is stored as an RGBA float tuple.
 */
class ImageColorMap
{
public:
  enum class InterpolationMode
  {
    Nearest,
    Linear
  };

  //    enum class ForcedInterpolationMode
  //    {
  //        Nearest, //!< Only nearest-neighbor interpolation
  //        Linear, //!< Only linear interpolation
  //        None //!< Disable interpolation forcing
  //    };

  /**
     * @brief Construct color map from a vector of RGB 32-bit float tuples.
     * The alpha component of each color is assumed to be 1.0.
     *
     * @param name Short name of color map
     * @param technicalName Technically descriptive name of color map
     * @param description More lengthy description of color map
     * @param colors Vector of colors, represented as RGB tuples with components
     * in range [0.0, 1.0]. The color's alpha value is assumed to be 1.0.
     */
  ImageColorMap(
    const std::string& name,
    const std::string& technicalName,
    const std::string& description,
    InterpolationMode interpMode,
    std::vector<glm::vec3> colors
  );

  /**
     * @brief Construct color map from a vector of pre-multiplied RGBA 32-bit float tuples.
     * @param name Short name of color map
     * @param technicalName Technically descriptive name for color map
     * @param description More lengthy description of color map
     * @param colors Vector of colors, represented as premultiplied RGBA tuples
     * with components in range [0.0, 1.0]
     */
  ImageColorMap(
    const std::string& name,
    const std::string& technicalName,
    const std::string& description,
    InterpolationMode interpMode,
    std::vector<glm::vec4> colors
  );

  ImageColorMap(const ImageColorMap&) = default;
  ImageColorMap& operator=(const ImageColorMap&) = default;

  ImageColorMap(ImageColorMap&&) = default;
  ImageColorMap& operator=(ImageColorMap&&) = default;

  ~ImageColorMap() = default;

  /**
     * @brief Set color map that it to be displayed as a preview of the actual color map
     * @param preview Vector of premultiplied RGBA colors with components in range [0.0, 1.0]
     */
  void setPreviewMap(std::vector<glm::vec4> preview);

  /// Return whether there exists a preview map. By default, none exists until
  /// \c setPreviewMap is called.
  bool hasPreviewMap() const;

  /// Get the number of colors in the preview map
  std::size_t numPreviewMapColors() const;

  /**
     * @brief Get a pointer to the raw preview color buffer
     * @return Const pointer to first element of buffer
     */
  const float* getPreviewMap() const;

  /// Get short name of color map
  const std::string& name() const;

  /// Get technical name of color map
  const std::string& technicalName() const;

  /// Get description of color map
  const std::string& description() const;

  /// Get number of colors in the color map
  std::size_t numColors() const;

  /// Get the color at a given index of the color map
  glm::vec4 color_RGBA_F32(std::size_t index) const;

  /// Get the total number of bytes occupied by the color map
  std::size_t numBytes_RGBA_F32() const;

  /// Get a constant raw pointer to the color map RGBA 32-bit floating point data buffer.
  /// The buffer is guaranteed to have length 4 * numColors()
  const float* data_RGBA_F32() const;

  /// Get the vector holding the data
  const std::vector<glm::vec4>& data_RGBA_asVector() const;

  /// Set a color
  bool setColorRGBA(std::size_t i, const glm::vec4& rgba);

  /// Slope and intercept used to map texels to range [0.0, 1.0]:
  glm::vec2 slopeIntercept(bool inverted = false) const;

  /// Set/get whether the map has a transparent border.
  /// To be used with constant color maps.
  void setTransparentBorder(bool set);
  bool transparentBorder() const;

  /**
     * @brief Cyclically rotate the color map by a fractional amount of its total length
     * @param fraction Fractional amount (in [0.0, 1.0]) by which to rotate the map
     */
  void cyclicRotate(float fraction);

  /// Reverse the color map
  void reverse();

  /// @brief Set/get interpolation mode
  void setInterpolationMode(const InterpolationMode& mode);
  InterpolationMode interpolationMode() const;

  //    /// @brief Set/get forced interpolation mode
  //    void setForcedInterpolationMode( const ForcedInterpolationMode& mode );
  //    ForcedInterpolationMode forcedInterpolationMode() const;

  //    /// @brief Get final interpolation mode
  //    InterpolationMode finalInterpolationMode() const;

  /// Get the sized internal texture format for the color map
  static tex::SizedInternalFormat textureFormat_RGBA_F32();

  /// Load color map from CSV as non-premultiplied RGBA
  static std::optional<ImageColorMap> loadImageColorMap(std::istringstream& csv);

  /// Create a linear colormap that interpolates between given start and end colors
  static ImageColorMap createLinearImageColorMap(
    const glm::vec4& startColor,
    const glm::vec4& endColor,
    std::size_t numSteps,
    std::string briefName,
    std::string description,
    std::string technicalName
  );

private:
  std::string m_name;          //!< Short name of the color map
  std::string m_technicalName; //!< Technical name of the color map
  std::string m_description;   //!< Description of the color map

  /// Table of premultiplied alpha colors represented using
  /// 32-bit floating point values per RGBA component.
  /// Components are only meaningful if in range [0.0, 1.0]
  std::vector<glm::vec4> m_colors_RGBA_F32;

  std::vector<glm::vec4> m_preview;      //!< Preview color map
  InterpolationMode m_interpolationMode; //!< Interpolation mode
  bool m_transparentBorder;              //!< Transparent border

  //    ForcedInterpolationMode m_forcedInterpolationMode;
};

#endif // IMAGE_COLORMAP_H
