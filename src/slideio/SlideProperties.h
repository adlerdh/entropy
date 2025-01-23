#ifndef SLIDEIO_SLIDE_PROPERTIES_H
#define SLIDEIO_SLIDE_PROPERTIES_H

#include <glm/vec3.hpp>

#include <string>
#include <utility>

namespace slideio
{

class SlideProperties
{
public:
  SlideProperties();
  ~SlideProperties() = default;

  const std::string& displayName() const;
  const glm::vec3& borderColor() const;
  bool visible() const;
  float opacity() const;
  std::pair<uint8_t, uint8_t> intensityThresholds() const;
  bool thresholdsActive() const;
  bool edgesVisible() const;
  float edgesMagnitude() const;
  float edgesSmoothing() const;

  void setDisplayName(std::string name);
  void setBorderColor(const glm::vec3& color);
  void setVisible(bool visible);
  void setOpacity(float opacity);
  void setIntensityThresholdLow(uint8_t low);
  void setIntensityThresholdHigh(uint8_t high);
  void setIntensityThresholds(const std::pair<uint8_t, uint8_t>& intensityThresholds);
  void setEdgesVisible(bool visible);
  void setEdgesMagnitude(float mag);
  void setEdgesSmoothing(float sigma);

  bool annotVisible() const;
  float annotOpacity() const;

  void setAnnotVisible(bool visible);
  void setAnnotOpacity(float opacity);

private:
  std::string m_displayName; //!< Display name for slide

  glm::vec3 m_borderColor; //!< Slide border color (non-pre-multiplied RGB)

  bool m_visible;  //!< Global slide visibility
  float m_opacity; //! Global slide opacity

  std::pair<uint8_t, uint8_t> m_intensityThresholds; //!< Slide thresholds (low, high)

  bool m_edgesVisible;    //!< Edge visibility
  float m_edgesMagnitude; //!< Edge magnitude
  float m_edgesSmoothing; //!< Edge smoothing

  bool m_annotVisible;  //!< Slide annotation visibility
  float m_annotOpacity; //!< Slide annotation opacity
};

} // namespace slideio

#endif // SLIDEIO_SLIDE_PROPERTIES_H
