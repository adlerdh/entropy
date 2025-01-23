#ifndef PARCELLATION_LABEL_TABLE_H
#define PARCELLATION_LABEL_TABLE_H

#include "rendering/utility/gl/GLTextureTypes.h"

#include <glm/gtc/type_precision.hpp>

#include <string>
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
 * Colors are stored with uchar components
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
  explicit ParcellationLabelTable(std::size_t labelCount, std::size_t maxLabelCount);

  ParcellationLabelTable(const ParcellationLabelTable&) = default;
  ParcellationLabelTable& operator=(const ParcellationLabelTable&) = default;

  ParcellationLabelTable(ParcellationLabelTable&&) = default;
  ParcellationLabelTable& operator=(ParcellationLabelTable&&) = default;

  ~ParcellationLabelTable() = default;

  /// Get number of labels in table
  std::size_t numLabels() const;

  /// Get the maximum number of labels in table
  std::size_t maxNumLabels() const;

  /// Get label name
  const std::string& getName(std::size_t index) const;

  /// Set label name
  void setName(std::size_t index, std::string name);

  /// Get global label visibility
  bool getVisible(std::size_t index) const;

  /// Set global label visibility
  void setVisible(std::size_t index, bool show);

  /// Get label mesh visibility (in 3D views)
  bool getShowMesh(std::size_t index) const;

  /// Set label mesh visibility (in 3D views)
  void setShowMesh(std::size_t index, bool show);

  /// Get label color (non-pre-multiplied RGB)
  glm::u8vec3 getColor(std::size_t index) const;

  /// Set label color (non-pre-multiplied RGB)
  void setColor(std::size_t index, const glm::u8vec3& color);

  /// Get label alpha
  uint8_t getAlpha(std::size_t index) const;

  /// Set label alpha
  void setAlpha(std::size_t index, uint8_t alpha);

  /// Add new labels to the table, returning the new label indices
  std::vector<std::size_t> addLabels(std::size_t count);

  /// Get label color as non-premultiplied alpha RGBA with uchar components in [0, 255]
  glm::u8vec4 color_RGBA_nonpremult_U8(std::size_t index) const;

  /// Get number of bytes used to represent the uchar color table
  std::size_t numColorBytes_RGBA_U8() const;

  /// Get const pointer to raw label color buffer uchar data.
  /// Colors are RGBA with NON-premultiplied alpha.
  const uint8_t* colorData_RGBA_nonpremult_U8() const;

  /// Get the sized internal texture format for the label RGBA uchar color buffer
  static tex::SizedInternalBufferTextureFormat bufferTextureFormat_RGBA_U8();

  static std::size_t numBytesPerLabel_U8();

  static std::size_t labelCountUpperBound();

private:
  /**
     * @brief Check if label index is valid. If not, throw exception.
     * @param index Label index
     * @throw Throw exception if label index is not valid.
     */
  void checkLabelIndex(std::size_t index) const;

  /**
     * @brief Update the non-pre-multiplied RGBA color at given label index in order to
     * match it with the label properties
     *
     * @param index Label index
     */
  void updateVector(std::size_t index);

  /// Vector of NON-pre-multiplied alpha colors represented using unsigned char values
  /// per RGBA component. Components are in range [0, 255]. The size of this vector
  /// matches the size of \c m_properties.
  std::vector<glm::u8vec4> m_colors_RGBA_U8;

  struct LabelProperties
  {
    std::string m_name; //!< Name

    glm::u8vec3 m_color;   //!< RGB color (NON-premultiplied)
    uint8_t m_alpha = 255; //!< Alpha channel opacity

    bool m_visible = true;   //!< Global visibility of label in all view types
    bool m_showMesh = false; //!< Mesh visibility in 3D views
  };

  /// Vector of label properties (size matching \c m_colors_RGBA_U8 )
  std::vector<LabelProperties> m_properties;

  /// Upper bound on the number of labels that this table can hold
  std::size_t m_maxLabelCount;
};

#endif // PARCELLATION_LABEL_TABLE_H
