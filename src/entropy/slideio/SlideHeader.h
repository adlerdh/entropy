#ifndef SLIDE_HEADER_H
#define SLIDE_HEADER_H

#include "slideio/SlideAssociatedImages.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <memory>
#include <string>
#include <utility>

namespace slideio
{

class SlideHeader
{
public:
  SlideHeader();
  ~SlideHeader() = default;

  const std::string& fileName() const;
  const std::string& vendorId() const;
  const std::string& displayName() const;
  const glm::vec2& pixelSize() const;
  float thickness() const;
  const glm::vec3& backgroundColor() const;
  bool hasTransparency() const;

  const SlideAssociatedImages& associatedImages() const;
  SlideAssociatedImages& associatedImages();

  void setFileName(std::string name);
  void setVendorId(std::string vendorId);
  void setPixelSize(const glm::vec2& size);
  void setPixelSizeX(float x);
  void setPixelSizeY(float y);
  void setThickness(float z);
  void setBackgroundColor(const glm::vec3& color);
  void setAssociatedImages(SlideAssociatedImages);
  void setHasTransparency(bool set);

private:
  std::string m_fileName;
  std::string m_vendorId;

  glm::vec2 m_pixelSize; //!< Pixel size (x, y) in mm of the highest resolution slide level
  float m_thickness;     //!< Thickness in mm of all levels

  glm::vec3 m_backgroundColor;

  /// Flag for whether the slide contains one or more pixels with alpha < 255.
  bool m_hasTransparency;

  SlideAssociatedImages m_associatedImages;
};

} // namespace slideio

#endif // SLIDE_HEADER_H
