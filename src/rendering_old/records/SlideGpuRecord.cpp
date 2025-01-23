#include "rendering_old/records/SlideGpuRecord.h"
#include "rendering/utility/gl/GLTexture.h"

SlideGpuRecord::SlideGpuRecord(std::shared_ptr<GLTexture> texture)
  : m_texture(texture)
//    ,
//      m_activeLevel( 0 )
{
}

std::weak_ptr<GLTexture> SlideGpuRecord::texture()
{
  return m_texture;
}
