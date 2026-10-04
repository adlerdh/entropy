#include "rendering/deformation/FieldReadback.h"

#include <new>
#include <stdexcept>
#include <utility>

namespace rendering::deformation
{
std::shared_ptr<const ::deformation::FieldCheckpoint> FieldReadback::checkpoint(
  const FieldPair& pair,
  const std::size_t maxHostBytes)
{
  if (!pair.forward || !pair.inverse) throw std::invalid_argument("A complete pair is required for readback");
  if (pair.forward->bytes() > maxHostBytes || pair.inverse->bytes() > maxHostBytes - pair.forward->bytes())
    throw std::bad_alloc();
  auto result = std::make_shared<::deformation::FieldCheckpoint>();
  result->forward = pair.forward->readbackLayers();
  result->inverse = pair.inverse->readbackLayers();
  return result;
}
} // namespace rendering::deformation
