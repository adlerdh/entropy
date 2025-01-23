#include "slideio/SlideHeader.h"

#include <glm/glm.hpp>

#include <iostream>

namespace slideio
{

SlideHeader::SlideHeader()
  : m_fileName()
  , m_vendorId()
  , m_pixelSize(1.0f, 1.0f)
  , m_thickness(1.0f)
  , m_backgroundColor(1.0f, 1.0f, 1.0f)
  , m_hasTransparency(true)
{
}

const std::string& SlideHeader::fileName() const
{
  return m_fileName;
}

const std::string& SlideHeader::vendorId() const
{
  return m_vendorId;
}

const glm::vec2& SlideHeader::pixelSize() const
{
  return m_pixelSize;
}

float SlideHeader::thickness() const
{
  return m_thickness;
}

const glm::vec3& SlideHeader::backgroundColor() const
{
  return m_backgroundColor;
}

bool SlideHeader::hasTransparency() const
{
  return m_hasTransparency;
}

const SlideAssociatedImages& SlideHeader::associatedImages() const
{
  return m_associatedImages;
}

SlideAssociatedImages& SlideHeader::associatedImages()
{
  return m_associatedImages;
}

void SlideHeader::setFileName(std::string name)
{
  m_fileName = std::move(name);
}

void SlideHeader::setVendorId(std::string vendorId)
{
  m_vendorId = std::move(vendorId);
}

void SlideHeader::setPixelSize(const glm::vec2& size)
{
  if (glm::any(glm::lessThanEqual(size, glm::vec2{0.0f, 0.0f})))
  {
    std::cerr << "Invalid pixel size (" << size.x << ", " << size.y << ")" << std::endl;
    return;
  }

  m_pixelSize = size;
}

void SlideHeader::setPixelSizeX(float x)
{
  if (x <= 0.0f)
  {
    std::cerr << "Invalid x pixel size " << x << std::endl;
    return;
  }

  m_pixelSize.x = x;
}

void SlideHeader::setPixelSizeY(float y)
{
  if (y <= 0.0f)
  {
    std::cerr << "Invalid y pixel size " << y << std::endl;
    return;
  }

  m_pixelSize.y = y;
}

void SlideHeader::setThickness(float z)
{
  if (z <= 0.0f)
  {
    std::cerr << "Invalid thickness " << z << std::endl;
    return;
  }

  m_thickness = z;
}

void SlideHeader::setBackgroundColor(const glm::vec3& color)
{
  static const glm::vec3 sk_zero(0.0f);
  static const glm::vec3 sk_one(1.0f);

  m_backgroundColor = glm::clamp(color, sk_zero, sk_one);
}

void SlideHeader::setHasTransparency(bool set)
{
  m_hasTransparency = set;
}

void SlideHeader::setAssociatedImages(SlideAssociatedImages images)
{
  m_associatedImages = std::move(images);
}

} // namespace slideio
