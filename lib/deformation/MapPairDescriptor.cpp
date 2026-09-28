#include "deformation/MapPairDescriptor.h"

#include "deformation/ContractError.h"

#include <utility>

namespace deformation
{

MapPairDescriptor::MapPairDescriptor(
  FieldDomain sourceDomain,
  FieldDomain outputDomain,
  RevisionId revision,
  NumericalPolicyVersion policyVersion)
  : m_sourceDomain(std::move(sourceDomain))
  , m_outputDomain(std::move(outputDomain))
  , m_revision(revision)
  , m_policyVersion(policyVersion)
{
  if (m_sourceDomain.dimension() != m_outputDomain.dimension()) {
    throw ContractError(ContractFailure::IncompatibleDomains, "Forward and inverse domains must have equal dimensions");
  }
  if (revision.value == 0) {
    throw ContractError(ContractFailure::InvalidRevision, "A paired map requires a nonzero revision identity");
  }
  if (policyVersion.value == 0) {
    throw ContractError(
      ContractFailure::InvalidPolicyVersion,
      "A paired map requires a nonzero numerical policy version");
  }
}

const FieldDomain& MapPairDescriptor::sourceDomain() const noexcept
{
  return m_sourceDomain;
}

const FieldDomain& MapPairDescriptor::outputDomain() const noexcept
{
  return m_outputDomain;
}

const FieldDomain& MapPairDescriptor::samplingDomain(MapDirection direction) const
{
  switch (direction) {
    case MapDirection::Forward:
      return m_sourceDomain;
    case MapDirection::Inverse:
      return m_outputDomain;
  }
  throw ContractError(ContractFailure::InvalidMapDirection, "Unknown map direction");
}

RevisionId MapPairDescriptor::revision() const noexcept
{
  return m_revision;
}

NumericalPolicyVersion MapPairDescriptor::policyVersion() const noexcept
{
  return m_policyVersion;
}

} // namespace deformation
