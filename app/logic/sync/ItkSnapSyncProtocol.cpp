#include "logic/sync/ItkSnapSyncProtocol.h"

namespace entropy::sync::itk_snap_protocol
{

glm::dvec3 rasFromLps(const glm::dvec3& lps)
{
  return {-lps.x, -lps.y, lps.z};
}

glm::dvec3 lpsFromRas(const glm::dvec3& ras)
{
  return {-ras.x, -ras.y, ras.z};
}

bool isSupportedProtocolVersion(const std::int16_t version)
{
  return sk_snapProtocolVersionCurrent == version || sk_snapProtocolVersionLegacy == version;
}

} // namespace entropy::sync::itk_snap_protocol
