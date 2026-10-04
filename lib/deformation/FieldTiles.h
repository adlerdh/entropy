#pragma once

#include "deformation/FieldEvidence.h"

#include <cstddef>
#include <optional>

namespace deformation
{
/**
 * @brief Conservative stored-sample box needed from an outer field to compose one output tile.
 * @details The inner field is sampled at tile centers. Displaced positions can cross
 * tile boundaries; the returned box includes every bilinear/trilinear contributor
 * plus one sample of rounding margin. Invalid or out-of-coverage positions need
 * no outer samples. Throws before returning a box over maxSamples.
 */
[[nodiscard]] std::optional<IndexExtent>
compositionDependencies(SampledFieldView inner, const IndexExtent& outputTile, std::size_t maxSamples);
} // namespace deformation
