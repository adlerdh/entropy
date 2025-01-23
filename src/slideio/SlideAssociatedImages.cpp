#include "slideio/SlideAssociatedImages.h"

namespace slideio
{

SlideAssociatedImages::SlideAssociatedImages()
  : m_thumbImageData(nullptr)
  , m_macroImageData(nullptr)
  , m_labelImageData(nullptr)
  ,

  m_thumbImageDims(0, 0)
  , m_macroImageDims(0, 0)
  , m_labelImageDims(0, 0)
{
}

std::pair<std::weak_ptr<std::vector<uint32_t> >, glm::i64vec2> SlideAssociatedImages::thumbImage(
) const
{
  return std::make_pair(m_thumbImageData, m_thumbImageDims);
}

std::pair<std::weak_ptr<std::vector<uint32_t> >, glm::i64vec2> SlideAssociatedImages::macroImage(
) const
{
  return std::make_pair(m_macroImageData, m_macroImageDims);
}

std::pair<std::weak_ptr<std::vector<uint32_t> >, glm::i64vec2> SlideAssociatedImages::labelImage(
) const
{
  return std::make_pair(m_labelImageData, m_labelImageDims);
}

void SlideAssociatedImages::setThumbImage(
  std::shared_ptr<std::vector<uint32_t> > data, const glm::i64vec2& dims
)
{
  m_thumbImageData = data;
  m_thumbImageDims = dims;
}

void SlideAssociatedImages::setMacroImage(
  std::shared_ptr<std::vector<uint32_t> > data, const glm::i64vec2& dims
)
{
  m_macroImageData = data;
  m_macroImageDims = dims;
}

void SlideAssociatedImages::setLabelImage(
  std::shared_ptr<std::vector<uint32_t> > data, const glm::i64vec2& dims
)
{
  m_labelImageData = data;
  m_labelImageDims = dims;
}

} // namespace slideio
