#include "slideio/SlideHelper.h"
#include "common/AABB.h"
#include "common/Exception.hpp"

namespace slideio
{

glm::mat4 stack_O_slide(const SlideCpuRecord& record)
{
  return record.transformation().stack_O_slide(physicalSlideDims(record));
}

glm::mat4 stack_O_slide_rigid(const SlideCpuRecord& record)
{
  return record.transformation().stack_O_slide_rigid(physicalSlideDims(record));
}

SlideTransformation translateXyInStack(const SlideCpuRecord& record, const glm::vec2& stackVec)
{
  const glm::vec3 dims = physicalSlideDims(record);

  SlideTransformation tx = record.transformation();

  tx.setNormalizedTranslationXY(
    tx.normalizedTranslationXY() + glm::vec2{stackVec.x / dims.x, stackVec.y / dims.y}
  );

  return tx;
}

void setTranslationXyInStack(SlideCpuRecord& record, const glm::vec2& stackVec)
{
  const glm::vec3 dims = physicalSlideDims(record);

  SlideTransformation tx = record.transformation();
  tx.setNormalizedTranslationXY(glm::vec2{stackVec.x / dims.x, stackVec.y / dims.y});

  record.setTransformation(std::move(tx));
}

glm::vec2 getTranslationXyInStack(const SlideCpuRecord& record)
{
  const glm::vec3 dims = physicalSlideDims(record);

  const SlideTransformation& tx = record.transformation();

  return glm::vec2{tx.normalizedTranslationXY().x * dims.x, tx.normalizedTranslationXY().y * dims.y};
}

glm::vec3 physicalSlideDims(const SlideCpuRecord& record)
{
  if (0 == record.numFileLevels())
  {
    throw_debug("No slide data loaded");
  }

  // Highest resolution level of slide:
  const auto& baseLevel = record.fileLevel(0);

  return glm::vec3{
    baseLevel.m_dims.x * record.header().pixelSize().x,
    baseLevel.m_dims.y * record.header().pixelSize().y,
    record.header().thickness()
  };
}

glm::vec2 convertPhysicalToNormalizedSlideTranslation(
  const SlideCpuRecord& record, const glm::vec2& physicalTranslation
)
{
  const glm::vec3 physicalDims = physicalSlideDims(record);

  return glm::vec2{physicalTranslation.x / physicalDims.x, physicalTranslation.y / physicalDims.y};
}

std::array<glm::vec3, 8> slideCornersInStack(const SlideCpuRecord& record)
{
  static const std::array<glm::vec4, 8> sk_slideCorners = {
    glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
    glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
    glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
    glm::vec4(0.0f, 1.0f, 1.0f, 1.0f),
    glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
    glm::vec4(1.0f, 0.0f, 1.0f, 1.0f),
    glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
    glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
  };

  const glm::mat4 M = stack_O_slide(record);

  std::array<glm::vec3, 8> stackCorners;

  for (uint32_t i = 0; i < 8; ++i)
  {
    glm::vec4 c = M * sk_slideCorners[i];
    stackCorners[i] = (c / c.w);
  }

  return stackCorners;
}

std::optional<AABB<float> > slideStackAABBoxInWorld(
  weak_record_range_t<SlideRecord> slideRecordRange, const glm::mat4& world_O_slideStack
)
{
  AABB<float> minMaxCorners{
    glm::vec3{std::numeric_limits<float>::max()}, glm::vec3{std::numeric_limits<float>::lowest()}
  };

  if (slideRecordRange.empty())
  {
    return std::nullopt;
  }

  for (const auto& weak_record : slideRecordRange)
  {
    auto record = weak_record.lock();
    if (!record || !record->cpuData())
    {
      continue;
    }

    const std::array<glm::vec3, 8> stackCornersOfSlide = slideCornersInStack(*(record->cpuData()));

    std::array<glm::vec3, 8> worldCornersOfSlide;

    for (uint32_t i = 0; i < 8; ++i)
    {
      glm::vec4 c = world_O_slideStack * glm::vec4{stackCornersOfSlide[i], 1.0f};
      worldCornersOfSlide[i] = (c / c.w);
    }

    for (const auto& corner : worldCornersOfSlide)
    {
      minMaxCorners.first = glm::min(minMaxCorners.first, corner);
      minMaxCorners.second = glm::max(minMaxCorners.second, corner);
    }
  }

  return minMaxCorners;
}

float slideStackHeight(weak_record_range_t<SlideRecord> slideRecords)
{
  if (slideRecords.empty())
  {
    return 0.0f;
  }

  float minZ = std::numeric_limits<float>::max();
  float maxZ = std::numeric_limits<float>::lowest();

  for (const auto& weak_record : slideRecords)
  {
    auto record = weak_record.lock();
    if (!record || !record->cpuData())
    {
      continue;
    }

    // The corners of the slide, represented in Stack space coordinates
    const auto corners = slideio::slideCornersInStack(*(record->cpuData()));

    for (const auto& corner : corners)
    {
      minZ = std::min(minZ, corner.z);
      maxZ = std::max(maxZ, corner.z);
    }
  }

  return (maxZ - minZ);
}

float slideStackPositiveExtent(weak_record_range_t<SlideRecord> slideRecords)
{
  float maxZ = 0.0f;

  if (slideRecords.empty())
  {
    return maxZ;
  }

  for (const auto& weak_record : slideRecords)
  {
    auto record = weak_record.lock();
    if (!record || !record->cpuData())
    {
      continue;
    }

    // The corners of the slide, represented in Stack space coordinates
    const auto corners = slideio::slideCornersInStack(*(record->cpuData()));

    for (const auto& corner : corners)
    {
      // Ignore negative z values
      if (corner.z >= 0.0f)
      {
        maxZ = std::max(maxZ, corner.z);
      }
    }
  }

  return maxZ;
}

} // namespace slideio
