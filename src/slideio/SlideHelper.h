#ifndef SLIDE_HELPER_H
#define SLIDE_HELPER_H

#include "common/AABB.h"
#include "logic_old/records/SlideRecord.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <boost/range/any_range.hpp>

#include <array>
#include <memory>
#include <optional>
#include <utility>

template<class Record>
using weak_record_range_t = boost::
  any_range<std::weak_ptr<Record>, boost::forward_traversal_tag, std::weak_ptr<Record>&, std::ptrdiff_t>;

namespace slideio
{

/**
 * @brief Compute and return the affine transformation from local Slide space
 * (i.e. normalized coordinates [0,1]^3) to Slide Stack space.
 */
glm::mat4 stack_O_slide(const SlideCpuRecord&);

/**
 * @brief Compute and return the rigid-body transformation from Slide space
 * (i.e. normalized coordinates [0,1]^3) to Slide Stack space.
 * This transformation ignores scale and shear.
 */
glm::mat4 stack_O_slide_rigid(const SlideCpuRecord&);

/**
 * @brief Compute and return the transformation of a slide following a translation in Stack space
 * @param stackVec Stack-space translation vector applied to the slide
 */
SlideTransformation translateXyInStack(const SlideCpuRecord&, const glm::vec2& stackVec);

/**
 * @brief Translate a slide by a vector defined in Stack space
 * @param stackVec Stack-space translation vector applied to the slide
 */
void setTranslationXyInStack(SlideCpuRecord&, const glm::vec2& stackVec);

/**
 * @brief Get the translation of a slide relative to Stack space
 */
glm::vec2 getTranslationXyInStack(const SlideCpuRecord&);

/**
 * @brief Get the physical (World-space) dimensions of a slide
 */
glm::vec3 physicalSlideDims(const SlideCpuRecord&);

/**
 * @brief Convert a physical (World-space) slide translation vector to normalized [0,1]^3 Slide-space
 * @param physicalTranslation Physical translation vector
 * @return Slide-space translation vector
 */
glm::vec2 convertPhysicalToNormalizedSlideTranslation(
  const SlideCpuRecord&, const glm::vec2& physicalTranslation
);

/**
 * @brief Return the eight corners of a slide in Stack space coodinates
 */
std::array<glm::vec3, 8> slideCornersInStack(const SlideCpuRecord&);

/**
 * @brief Compute the AABB of the Slide Stack in World space.
 *
 * @param[in] slideRecords Range of all slide records
 * @param[in] world_O_slideStack Transformation from Slide Stack to World space.
 *
 * @return Optional AABB of the Stack in World space. If there are no slides in the stack,
 * then std::nullopt is returned.
 */
std::optional<AABB<float> > slideStackAABBoxInWorld(
  weak_record_range_t<SlideRecord> slideRecords, const glm::mat4& world_O_slideStack
);

/**
 * @brief Computes physical height of the slide stack, measured along the stack's z axis.
 */
float slideStackHeight(weak_record_range_t<SlideRecord>);

/**
 * @brief Computes the positive extent of the slide stack, measured along the stack's z axis.
 */
float slideStackPositiveExtent(weak_record_range_t<SlideRecord>);

} // namespace slideio

#endif // SLIDE_HELPER_H
