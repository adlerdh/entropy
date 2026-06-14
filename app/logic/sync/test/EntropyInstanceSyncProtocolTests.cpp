#include "logic/sync/EntropyInstanceSyncProtocol.h"

#include <catch2/catch_test_macros.hpp>

using namespace entropy::sync::instance_protocol;

TEST_CASE("Entropy instance sync encodes and decodes peer registry records", "[sync][entropy]")
{
  const std::string record = encodeRegistryRecord("peer-a", 1234, 48123, "project-key", 10000);

  const auto peer = decodePeerRecord(record, "local", "project-key", 10025);

  REQUIRE(peer);
  CHECK(peer->instanceId == "peer-a");
  CHECK(peer->port == 48123);
  CHECK_FALSE(peerRecordIsStale(record, 10025));
  CHECK(peerRecordIsStale(record, 16000));
}

TEST_CASE("Entropy instance sync rejects incompatible peer registry records", "[sync][entropy]")
{
  CHECK_FALSE(decodePeerRecord("not json", "local", "project-key", 10025));
  CHECK_FALSE(
    decodePeerRecord(encodeRegistryRecord("local", 1234, 48123, "project-key", 10000), "local", "project-key", 10025));
  CHECK_FALSE(decodePeerRecord(
    encodeRegistryRecord("peer-a", 1234, 48123, "other-project", 10000),
    "local",
    "project-key",
    10025));
  CHECK_FALSE(
    decodePeerRecord(encodeRegistryRecord("peer-a", 1234, 48123, "project-key", 1), "local", "project-key", 10025));
  CHECK_FALSE(decodePeerRecord(
    R"({"version":1,"instanceId":"peer-a","port":0,"projectKey":"project-key","updatedMs":10000})",
    "local",
    "project-key",
    10025));
}

TEST_CASE("Entropy instance sync encodes and decodes cursor messages", "[sync][entropy]")
{
  const glm::dvec3 cursorLps{12.0, -3.5, 9.25};
  const std::string message = encodeCursorMessage("peer-a", 7, "project-key", cursorLps);

  const auto decoded = decodeCursorMessage(message, "local", "project-key");

  REQUIRE(decoded);
  CHECK(decoded->sender == "peer-a");
  CHECK(decoded->sequence == 7);
  CHECK(decoded->cursorLps == cursorLps);
}

TEST_CASE("Entropy instance sync rejects incompatible cursor messages", "[sync][entropy]")
{
  CHECK_FALSE(decodeCursorMessage("not json", "local", "project-key"));
  CHECK_FALSE(
    decodeCursorMessage(encodeCursorMessage("local", 7, "project-key", {1.0, 2.0, 3.0}), "local", "project-key"));
  CHECK_FALSE(
    decodeCursorMessage(encodeCursorMessage("peer-a", 7, "other-project", {1.0, 2.0, 3.0}), "local", "project-key"));
  CHECK_FALSE(decodeCursorMessage(
    R"({"version":1,"sender":"peer-a","sequence":7,"projectKey":"project-key","cursorLps":[1.0,2.0]})",
    "local",
    "project-key"));
  CHECK_FALSE(decodeCursorMessage(
    R"({"version":1,"sender":"peer-a","sequence":7,"projectKey":"project-key","cursorLps":[1.0,"bad",3.0]})",
    "local",
    "project-key"));
  CHECK_FALSE(decodeCursorMessage(
    R"({"version":1,"sender":"peer-a","sequence":0,"projectKey":"project-key","cursorLps":[1.0,2.0,3.0]})",
    "local",
    "project-key"));
}

TEST_CASE("Entropy instance sync cursor equality uses the synchronization epsilon", "[sync][entropy]")
{
  const glm::dvec3 cursor{1.0, 2.0, 3.0};

  CHECK(nearlyEqual(cursor, cursor + glm::dvec3{sk_cursorEpsilonMm * 0.25, 0.0, 0.0}));
  CHECK_FALSE(nearlyEqual(cursor, cursor + glm::dvec3{sk_cursorEpsilonMm * 2.0, 0.0, 0.0}));
}
