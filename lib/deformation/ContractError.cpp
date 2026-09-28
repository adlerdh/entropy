#include "deformation/ContractError.h"

namespace deformation
{

ContractError::ContractError(ContractFailure reason, const char* message)
  : std::invalid_argument(message), m_reason(reason)
{
}

ContractFailure ContractError::reason() const noexcept
{
  return m_reason;
}

} // namespace deformation
