#pragma once

#include "logic/app/DeformationEditController.h"
#include "rendering/deformation/FieldPassRunner.h"
#include "rendering/deformation/FieldWorkspace.h"

#include <cstddef>

namespace deformation_edit
{
/**
 * @brief GL-context-thread numerical backend for one immutable edit request.
 * @details All GPU resources are local to evaluate and destroyed before it
 * returns. The result contains a complete host checkpoint only after the
 * runner accepts both directions. Publication remains the session's job.
 * Current field passes require matching source and output grids.
 */
class DeformationEditBackend final : public Backend
{
public:
  DeformationEditBackend(
    std::size_t workspaceBytes,
    double controlSpacingMm,
    unsigned firstSquarings = 5,
    unsigned maxAttempts = 3);
  DeformationEditBackend(
    rendering::deformation::FieldWorkspace& workspace,
    std::size_t runnerWorkspaceBytes,
    double controlSpacingMm,
    unsigned firstSquarings = 5,
    unsigned maxAttempts = 3);

  [[nodiscard]] Result evaluate(const Request& request) override;

private:
  rendering::deformation::FieldPassRunner m_runner;
  std::size_t m_workspaceBytes;
  double m_controlSpacingMm;
  unsigned m_firstSquarings;
  unsigned m_maxAttempts;
  rendering::deformation::FieldWorkspace* m_workspace = nullptr;
};
} // namespace deformation_edit
