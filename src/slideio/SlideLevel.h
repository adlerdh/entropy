#ifndef SLIDE_LEVEL_H
#define SLIDE_LEVEL_H

#include <glm/gtc/type_precision.hpp>
#include <glm/vec2.hpp>

#include <memory>

namespace slideio
{

struct SlideLevel
{
  int m_level;
  glm::i64vec2 m_dims;
  glm::dvec2 m_downsampleFactors;
  std::unique_ptr<uint32_t[]> m_data;
};

} // namespace slideio

#endif // SLIDE_LEVEL_H
