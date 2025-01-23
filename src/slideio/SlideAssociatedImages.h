#ifndef SLIDE_ASSOCIATED_IMAGES_H
#define SLIDE_ASSOCIATED_IMAGES_H

#include <glm/gtc/type_precision.hpp>
#include <glm/vec2.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace slideio
{

class SlideAssociatedImages
{
public:
  SlideAssociatedImages();
  ~SlideAssociatedImages() = default;

  std::pair<std::weak_ptr<std::vector<uint32_t> >, glm::i64vec2> thumbImage() const;
  std::pair<std::weak_ptr<std::vector<uint32_t> >, glm::i64vec2> macroImage() const;
  std::pair<std::weak_ptr<std::vector<uint32_t> >, glm::i64vec2> labelImage() const;

  void setThumbImage(std::shared_ptr<std::vector<uint32_t> > data, const glm::i64vec2& dims);
  void setMacroImage(std::shared_ptr<std::vector<uint32_t> > data, const glm::i64vec2& dims);
  void setLabelImage(std::shared_ptr<std::vector<uint32_t> > data, const glm::i64vec2& dims);

private:
  // All images are stored in pre-multiplied ARGB format.
  // If not loaded, the pointers are null.

  /// Thumbnail image from slide file. The image is generated from the lowest resolution
  /// slide layer if no thumbanil is provided in the slide.
  std::shared_ptr<std::vector<uint32_t> > m_thumbImageData;

  /// Macro image from slide file
  std::shared_ptr<std::vector<uint32_t> > m_macroImageData;

  /// Label image from slide file
  std::shared_ptr<std::vector<uint32_t> > m_labelImageData;

  glm::i64vec2 m_thumbImageDims;
  glm::i64vec2 m_macroImageDims;
  glm::i64vec2 m_labelImageDims;
};

} // namespace slideio

#endif // SLIDE_ASSOCIATED_IMAGES_H
