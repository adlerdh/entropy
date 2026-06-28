#pragma once

#include "registration/Types.h"

#include <string>
#include <vector>

namespace registration
{

/**
 * @brief Severity for a registration validation message.
 */
enum class ValidationSeverity
{
  Warning, //!< Job may run, but the user should be informed.
  Error    //!< Job cannot be launched.
};

/**
 * @brief One validation message shown in the registration setup UI.
 */
struct ValidationMessage
{
  ValidationSeverity severity = ValidationSeverity::Warning; //!< Message severity.
  std::string field;                                         //!< Related job field or UI section.
  std::string text;                                          //!< User-facing explanation.
};

/**
 * @brief Result of validating a registration job against backend capabilities.
 */
struct ValidationResult
{
  std::vector<ValidationMessage> messages; //!< Warnings and errors.

  /**
   * @brief Return whether the job has any blocking errors.
   * @return True iff the job can be launched.
   */
  [[nodiscard]] bool canLaunch() const;
};

/**
 * @brief Validate a registration job against backend capabilities.
 * @param job Backend-neutral job specification.
 * @param capabilities Capabilities of `job.backend`.
 * @return Validation messages and launch readiness.
 */
ValidationResult validateJob(const JobSpec& job, const BackendCapabilities& capabilities);

} // namespace registration
