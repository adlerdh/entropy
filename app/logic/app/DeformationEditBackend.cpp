#include "logic/app/DeformationEditBackend.h"

#include "deformation/VelocityLattice.h"
#include "rendering/deformation/FieldReadback.h"
#include "rendering/deformation/FieldTextures.h"

#include <cmath>
#include <memory>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <utility>

namespace deformation_edit
{
namespace
{
bool sameGrid(const deformation::FieldDomain& a, const deformation::FieldDomain& b)
{
  if (
    a.dimension() != b.dimension() || a.size() != b.size() || a.validExtent().begin != b.validExtent().begin ||
    a.validExtent().end != b.validExtent().end || a.origin() != b.origin() || a.spacing() != b.spacing())
  {
    return false;
  }
  for (int axis = 0; axis < 3; ++axis) {
    if (a.directions()[axis] != b.directions()[axis]) return false;
  }
  return true;
}
} // namespace

DeformationEditBackend::DeformationEditBackend(
  std::size_t workspaceBytes,
  double controlSpacingMm,
  unsigned firstSquarings,
  unsigned maxAttempts)
  : m_runner(workspaceBytes)
  , m_workspaceBytes(workspaceBytes)
  , m_controlSpacingMm(controlSpacingMm)
  , m_firstSquarings(firstSquarings)
  , m_maxAttempts(maxAttempts)
{
  if (!std::isfinite(controlSpacingMm) || controlSpacingMm <= 0.0) {
    throw std::invalid_argument("Edit control spacing must be positive and finite");
  }
  if (firstSquarings >= 20 || maxAttempts == 0 || maxAttempts > 20) {
    throw std::invalid_argument("Edit integration attempt limits are invalid");
  }
}

DeformationEditBackend::DeformationEditBackend(
  rendering::deformation::FieldWorkspace& workspace,
  std::size_t runnerWorkspaceBytes,
  double controlSpacingMm,
  unsigned firstSquarings,
  unsigned maxAttempts)
  : DeformationEditBackend(runnerWorkspaceBytes, controlSpacingMm, firstSquarings, maxAttempts)
{
  m_workspace = &workspace;
}

Result DeformationEditBackend::evaluate(const Request& request)
{
  Result result;
  if (request.cancel && request.cancel->load()) return result;
  if (request.maps.revision() != request.acceptedRevision || !request.base) {
    throw std::invalid_argument("Edit request has no matching base revision");
  }
  const auto& source = request.maps.sourceDomain();
  const auto& output = request.maps.outputDomain();
  if (!sameGrid(source, output)) {
    throw std::invalid_argument("GL edits currently require matching source and output grids");
  }
  if (request.base->forward.size() != source.sampleCount() || request.base->inverse.size() != output.sampleCount()) {
    throw std::invalid_argument("Edit request has an incomplete base checkpoint");
  }

  using Use = rendering::deformation::FieldBudgetUse;
  using Reservation = rendering::deformation::FieldWorkspace::Reservation;
  std::optional<Reservation> acceptedBudget, candidateBudget, scratchBudget, readbackBudget;
  if (m_workspace) {
    if (source.sampleCount() > std::numeric_limits<std::size_t>::max() / (2 * sizeof(glm::vec4)))
      throw std::bad_alloc();
    const std::size_t pairBytes = source.sampleCount() * 2 * sizeof(glm::vec4);
    acceptedBudget.emplace(m_workspace->reserve(Use::AcceptedMaps, pairBytes));
    candidateBudget.emplace(m_workspace->reserve(Use::Candidate, pairBytes / 2));
    scratchBudget.emplace(m_workspace->reserve(Use::Scratch, m_workspaceBytes));
    readbackBudget.emplace(m_workspace->reserve(Use::Readback, pairBytes));
  }

  rendering::deformation::FieldPair previous;
  previous.forward = std::make_unique<rendering::deformation::FieldTexture>(source, m_workspaceBytes);
  previous.inverse = std::make_unique<rendering::deformation::FieldTexture>(output, m_workspaceBytes);
  previous.forward->upload(request.base->forward);
  previous.inverse->upload(request.base->inverse);
  rendering::deformation::FieldTexture velocity(source, m_workspaceBytes);
  const deformation::VelocityLattice lattice(deformation::BrushStep(request.brush), m_controlSpacingMm);
  m_runner.velocity(lattice, velocity);
  const auto stopped = [&request] {
    return request.cancel && request.cancel->load();
  };
  auto candidate = m_runner.acceptVelocity(
    velocity,
    &previous,
    request.brush.protection,
    request.policy,
    m_firstSquarings,
    m_maxAttempts,
    stopped);
  result.report = std::move(candidate.report);
  result.assessment = candidate.assessment;
  if (candidate.canceled || stopped() || !candidate.pair.forward || !candidate.pair.inverse) return result;
  auto checkpoint = rendering::deformation::FieldReadback::checkpoint(candidate.pair, m_workspaceBytes);
  if (!stopped()) result.checkpoint = std::move(checkpoint);
  return result;
}
} // namespace deformation_edit
