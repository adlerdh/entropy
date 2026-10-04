#pragma once

#include "logic/app/DeformationEditSession.h"

#include <functional>
#include <memory>

namespace deformation_edit
{
/** @brief Numerical worker interface; it computes candidates but cannot publish them. */
class Backend
{
public:
  virtual ~Backend() = default;
  /** @brief Run on the backend's owning thread; poll request.cancel between passes. */
  [[nodiscard]] virtual Result evaluate(const Request& request) = 0;
};

/**
 * @brief Owner-thread coordinator that captures live dependencies at each boundary.
 * @details The synchronous append path is suitable for headless tests and a
 * GL-context thread. An asynchronous worker may instead use the session's
 * prepareStep/complete ticket API and return completion to this owner thread.
 */
class DeformationEditController final
{
public:
  using Capture = std::function<Dependencies()>;
  DeformationEditController(DeformationEditSession& session, Backend& backend, Capture capture);

  bool beginStroke();
  Completion appendStep(deformation::BrushDefinition brush);
  bool endStroke();
  void cancelStroke() noexcept;
  bool undo() noexcept;
  bool redo(deformation::RevisionId child) noexcept;

private:
  DeformationEditSession& m_session;
  Backend& m_backend;
  Capture m_capture;
};
} // namespace deformation_edit
