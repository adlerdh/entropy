#include "logic/sync/ItkSnapSyncProtocol.h"

#include <catch2/catch_test_macros.hpp>

using namespace entropy::sync::itk_snap_protocol;

TEST_CASE("ITK-SNAP sync converts between Entropy LPS and ITK-SNAP RAS", "[sync][snap]")
{
  const glm::dvec3 lps{12.5, -4.0, 8.25};
  const glm::dvec3 ras = rasFromLps(lps);

  CHECK(ras.x == -12.5);
  CHECK(ras.y == 4.0);
  CHECK(ras.z == 8.25);
  CHECK(lpsFromRas(ras) == lps);
}

TEST_CASE("ITK-SNAP sync accepts current and legacy protocol versions", "[sync][snap]")
{
  CHECK(isSupportedProtocolVersion(sk_snapProtocolVersionCurrent));
  CHECK(isSupportedProtocolVersion(sk_snapProtocolVersionLegacy));
  CHECK_FALSE(isSupportedProtocolVersion(0));
  CHECK_FALSE(isSupportedProtocolVersion(0x1004));
  CHECK_FALSE(isSupportedProtocolVersion(0x1007));
}

TEST_CASE("ITK-SNAP sync wire structures keep the expected binary layout", "[sync][snap]")
{
#if defined(_WIN32)
  CHECK(sizeof(ItkSnapIpcHeader) == 12);
  CHECK(sizeof(ItkSnapIpcDirectoryEntry) == 2312);
#else
  CHECK(sizeof(ItkSnapIpcHeader) == 24);
  CHECK(sizeof(ItkSnapIpcDirectoryEntry) == 2320);
#endif
  CHECK(sizeof(ItkSnapIpcCameraState) == 112);
  CHECK(sizeof(ItkSnapIpcMessage) == 184);
  CHECK(sizeof(ItkSnapIpcDirectory) == static_cast<std::size_t>(sk_maxInstances) * sizeof(ItkSnapIpcDirectoryEntry));
}
