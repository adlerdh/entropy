#include "logic/app/DeformationEditController.h"

#include <stdexcept>
#include <utility>

namespace deformation_edit
{
DeformationEditController::DeformationEditController(DeformationEditSession& session, Backend& backend, Capture capture)
  : m_session(session), m_backend(backend), m_capture(std::move(capture))
{
  if (!m_capture) throw std::invalid_argument("An edit controller needs a live-state capture function");
}

bool DeformationEditController::beginStroke()
{
  return m_session.beginStroke(m_capture());
}
Completion DeformationEditController::appendStep(deformation::BrushDefinition brush)
{
  const auto request = m_session.prepareStep(std::move(brush), m_capture());
  if (!request) return Completion::Stale;
  try {
    auto result = m_backend.evaluate(*request);
    return m_session.complete(request, std::move(result), m_capture());
  }
  catch (...) {
    m_session.cancelStroke();
    throw;
  }
}
bool DeformationEditController::endStroke()
{
  return m_session.endStroke(m_capture());
}
void DeformationEditController::cancelStroke() noexcept
{
  m_session.cancelStroke();
}
bool DeformationEditController::undo() noexcept
{
  return m_session.undo();
}
bool DeformationEditController::redo(deformation::RevisionId child) noexcept
{
  return m_session.redo(child);
}
} // namespace deformation_edit
