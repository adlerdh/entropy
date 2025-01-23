#include "rendering_old/records/ImageGpuRecord.h"
#include "rendering/utility/gl/GLTexture.h"

ImageGpuRecord::ImageGpuRecord(std::shared_ptr<GLTexture> texture)
  : m_texture(texture)
{
}

std::weak_ptr<GLTexture> ImageGpuRecord::texture()
{
  return m_texture;
}
