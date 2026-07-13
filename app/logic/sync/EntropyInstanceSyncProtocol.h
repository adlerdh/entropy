#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace app_sync::instance_protocol
{

inline constexpr int sk_protocolVersion = 1;
inline constexpr double sk_cursorEpsilonMm = 1.0e-4;
inline constexpr std::int64_t sk_peerStaleMs = 5000;

struct PeerRecord
{
  std::string instanceId;
  std::uint16_t port = 0;
};

struct CursorMessage
{
  std::string sender;
  std::uint64_t sequence = 0;
  glm::dvec3 cursorLps{0.0};
};

bool nearlyEqual(const glm::dvec3& a, const glm::dvec3& b);

std::string encodeRegistryRecord(
  const std::string& instanceId,
  std::uint32_t processId,
  std::uint16_t port,
  const std::string& projectKey,
  std::int64_t updatedMs);

std::optional<PeerRecord> decodePeerRecord(
  const std::string& recordText,
  const std::string& localInstanceId,
  const std::string& projectKey,
  std::int64_t nowMs);

bool peerRecordIsStale(const std::string& recordText, std::int64_t nowMs);

std::string encodeCursorMessage(
  const std::string& sender,
  std::uint64_t sequence,
  const std::string& projectKey,
  const glm::dvec3& cursorLps);

std::optional<CursorMessage>
decodeCursorMessage(const std::string& messageText, const std::string& localInstanceId, const std::string& projectKey);

} // namespace app_sync::instance_protocol
