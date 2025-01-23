#ifndef IMAGE_COLORMAP_H
#define IMAGE_COLORMAP_H

#include "rendering/utility/gl/GLTextureTypes.h"

#include <glm/fwd.hpp>

#include <string>
#include <vector>

/**
 * @brief Class for an image color map. Each color is stored as an RGBA float tuple.
 */
class ImageColorMap
{
public:
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
  explicit ImageColorMap(
    std::string name,
    std::string technicalName,
    std::string description,
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
  explicit ImageColorMap(
    std::string name,
    std::string technicalName,
    std::string description,
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
  size_t numPreviewMapColors() const;

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
  size_t numColors() const;

  /// Get the color at a given index of the color map
  glm::vec4 color_RGBA_F32(size_t index) const;

  /// Get the total number of bytes occupied by the color map
  size_t numBytes_RGBA_F32() const;

  /// Get a constant raw pointer to the color map RGBA 32-bit floating point data buffer.
  /// The buffer is guaranteed to have length 4 * numColors()
  const float* data_RGBA_F32() const;

  /// Slope and intercept used to map texels to range [0.0, 1.0]:
  glm::vec2 slopeIntercept() const;

  /**
     * @brief Cyclically rotate the color map by a fractional amount of its total length
     * @param fraction Fractional amount (in [0.0, 1.0]) by which to rotate the map
     */
  void cyclicRotate(float fraction);

  /// Reverse the color map
  void reverse();

  /// Get the sized internal texture format for the color map
  static tex::SizedInternalFormat textureFormat_RGBA_F32();

private:
  /// Short name of the color map
  std::string m_name;

  /// Technical name of the color map
  std::string m_technicalName;

  /// Description of the color map
  std::string m_description;

  /// Table of premultiplied alpha colors represented using
  /// 32-bit floating point values per RGBA component.
  /// Components are only meaningful if in range [0.0, 1.0]
  std::vector<glm::vec4> m_colors_RGBA_F32;

  /// Preview color map
  std::vector<glm::vec4> m_preview;
};

#endif // IMAGE_COLORMAP_H
