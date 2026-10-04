#pragma once

#include "logic/app/DeformationEditController.h"
#include "rendering/deformation/FieldPassRunner.h"

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

  [[nodiscard]] Result evaluate(const Request& request) override;

private:
  rendering::deformation::FieldPassRunner m_runner;
  std::size_t m_workspaceBytes;
  double m_controlSpacingMm;
  unsigned m_firstSquarings;
  unsigned m_maxAttempts;
};
} // namespace deformation_edit
