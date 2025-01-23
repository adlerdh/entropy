#ifndef PARCELLATION_LABEL_TABLE_H
#define PARCELLATION_LABEL_TABLE_H

#include "rendering/utility/gl/GLTextureTypes.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <vector>

/**
 * @brief Class to hold a table of image parcellation labels.
 * Labels consist of the following properties:
 * - Name
 * - Color
 * - Opacity
 * - Visibility flag for 2D views
 * - Visibility flag for 3D views
 *
 * @note Colors are indexed. These indices are NOT the label values.
 */
class ParcellationLabelTable
{
public:
  /**
     * @brief Construct the label table with good default colors. The color of label 0 is
     * fully transparent black. Labels 1 to 6 are the primary colors (red, green, blue,
     * yellow, cyan, magenta). Following this are colors randomly chosen in HSV space.
     *
     * @param labelCount Size of label table to construct. Must be at least 7,
     * in order to represent mandatory labels 0 to 6.
     */
  explicit ParcellationLabelTable(size_t labelCount);

  ParcellationLabelTable(const ParcellationLabelTable&) = default;
  ParcellationLabelTable& operator=(const ParcellationLabelTable&) = default;

  ParcellationLabelTable(ParcellationLabelTable&&) = default;
  ParcellationLabelTable& operator=(ParcellationLabelTable&&) = default;

  ~ParcellationLabelTable() = default;

  /// Get number of labels in table
  size_t numLabels() const;

  /// Get label name
  const std::string& getName(size_t index) const;

  /// Set label name
  void setName(size_t index, std::string name);

  /// Get global label visibility
  bool getVisible(size_t index) const;

  /// Set global label visibility
  void setVisible(size_t index, bool show);

  /// Get label mesh visibility (in 3D views)
  bool getShowMesh(size_t index) const;

  /// Set label mesh visibility (in 3D views)
  void setShowMesh(size_t index, bool show);

  /// Get label color (non-premultiplied RGB)
  glm::vec3 getColor(size_t index) const;

  /// Set label color (non-premultiplied RGB)
  void setColor(size_t index, const glm::vec3& color);

  /// Get label alpha
  float getAlpha(size_t index) const;

  /// Set label alpha
  void setAlpha(size_t index, float alpha);

  /// Get label color as pre-multiplied alpha RGBA with float components in [0.0, 1.0]
  glm::vec4 color_RGBA_premult_F32(size_t index) const;

  /// Get number of bytes used to represent the color table
  size_t numColorBytes_RGBA_F32() const;

  /// Get const pointer to raw label color buffer data.
  /// Colors are RGBA with pre-multiplied alpha.
  const float* colorData_RGBA_premult_F32() const;

  /// Get the sized internal texture format for the label RGBA color buffer
  static tex::SizedInternalBufferTextureFormat bufferTextureFormat_RGBA_F32();

private:
  /**
     * @brief Check if label index is valid. If not, throw exception.
     * @param index Label index
     * @throw Throw exception if label index is not valid.
     */
  void checkLabelIndex(size_t index) const;

  /**
     * @brief Update the pre-multiplied RGBA color at given label index in order to
     * match it with the label properties
     *
     * @param index Label index
     */
  void updateColorRGBA(size_t index);

  /// Vector of pre-multiplied alpha colors represented using 32-bit floating point values
  /// per RGBA component. Components are in range [0.0, 1.0]. RGBA colors in this vector
  /// account for opacity and 2D visibility. In other words, the RGBA components are
  /// modulated by label opacity and 2D visibility settings. The size of this vector
  /// matches the size of \c m_properties.
  std::vector<glm::vec4> m_colors_RGBA_F32;

  struct LabelProperties
  {
    std::string m_name;      //!< Name
    glm::vec3 m_color;       //!< RGB color (non-pre-multiplied) in [0, 1]
    float m_alpha;           //!< Alpha channel opacity in [0, 1]
    bool m_visible = true;   //!< Global visibility of label in all view types
    bool m_showMesh = false; //!< Mesh visibility in 3D views
  };

  /// Vector of label properties (size matching \c m_colors_RGBA_F32)
  std::vector<LabelProperties> m_properties;
};

#endif // PARCELLATION_LABEL_TABLE_H
