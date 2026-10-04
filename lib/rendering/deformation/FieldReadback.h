#pragma once

#include "deformation/EditHistory.h"
#include "rendering/deformation/FieldPassRunner.h"

#include <cstddef>
#include <memory>

namespace rendering::deformation
{
/** @brief Bounded, complete forward/inverse checkpoint readback on a GL context thread. */
class FieldReadback final
{
public:
  [[nodiscard]] static std::shared_ptr<const ::deformation::FieldCheckpoint> checkpoint(
    const FieldPair& pair,
    std::size_t maxHostBytes);
};
} // namespace rendering::deformation
