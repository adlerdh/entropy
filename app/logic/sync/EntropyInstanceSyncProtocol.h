#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace app_sync::instance_protocol
{

/** @brief Cursor position tolerance in millimeters when suppressing duplicate broadcasts */
inline constexpr double sk_cursorEpsilonMm = 1.0e-4;
/** @brief Age in milliseconds after which a peer registry record is treated as stale */
inline constexpr std::int64_t sk_peerStaleMs = 5000;

/**
 * @brief Decoded heartbeat record for another running Entropy instance
 */
struct PeerRecord
{
  /** @brief Stable random ID advertised by the peer instance */
  std::string instanceId;
  /** @brief UDP loopback port on which the peer receives synchronization messages */
  std::uint16_t port = 0;
};

/**
 * @brief Decoded cursor update sent between Entropy instances
 */
struct CursorMessage
{
  /** @brief Stable random ID of the sending instance */
  std::string sender;
  /** @brief Monotonically increasing sender-local sequence number */
  std::uint64_t sequence = 0;
  /** @brief Cursor position in LPS world millimeters */
  glm::dvec3 cursorLps{0.0};
};

/**
 * @brief Compare two LPS cursor positions using the synchronization tolerance
 * @param a First cursor position in LPS millimeters
 * @param b Second cursor position in LPS millimeters
 * @return True when the two cursor positions are within `sk_cursorEpsilonMm`
 */
bool nearlyEqual(const glm::dvec3& a, const glm::dvec3& b) noexcept;

/**
 * @brief Encode a peer heartbeat registry record as JSON text
 * @param instanceId Stable random ID of this Entropy instance
 * @param processId Native process ID of this Entropy instance
 * @param port UDP loopback port used by this Entropy instance
 * @param projectKey Canonical key for the currently loaded project or image set
 * @param updatedMs Monotonic timestamp in milliseconds
 * @return Serialized registry record
 * @throw Propagates exceptions from JSON or string allocation
 */
std::string encodeRegistryRecord(
  const std::string& instanceId,
  std::uint32_t processId,
  std::uint16_t port,
  const std::string& projectKey,
  std::int64_t updatedMs);

/**
 * @brief Decode a peer heartbeat registry record if it describes a live peer for the current project
 * @param recordText JSON text read from a peer registry file
 * @param localInstanceId Stable random ID of this Entropy instance
 * @param projectKey Canonical key for the currently loaded project or image set
 * @param nowMs Current monotonic timestamp in milliseconds
 * @return Decoded peer record, or `std::nullopt` when the record is invalid, stale, local, or for another project
 */
std::optional<PeerRecord> decodePeerRecord(
  const std::string& recordText,
  const std::string& localInstanceId,
  const std::string& projectKey,
  std::int64_t nowMs);

/**
 * @brief Check whether a peer registry record is stale
 * @param recordText JSON text read from a peer registry file
 * @param nowMs Current monotonic timestamp in milliseconds
 * @return True when the record is valid JSON and older than `sk_peerStaleMs`
 */
bool peerRecordIsStale(const std::string& recordText, std::int64_t nowMs);

/**
 * @brief Encode a cursor update message as JSON text
 * @param sender Stable random ID of the sending Entropy instance
 * @param sequence Sender-local sequence number
 * @param projectKey Canonical key for the currently loaded project or image set
 * @param cursorLps Cursor position in LPS world millimeters
 * @return Serialized cursor update message
 * @throw Propagates exceptions from JSON or string allocation
 */
std::string encodeCursorMessage(
  const std::string& sender,
  std::uint64_t sequence,
  const std::string& projectKey,
  const glm::dvec3& cursorLps);

/**
 * @brief Decode a cursor update message for the current project
 * @param messageText JSON text received over UDP
 * @param localInstanceId Stable random ID of this Entropy instance
 * @param projectKey Canonical key for the currently loaded project or image set
 * @return Decoded cursor message, or `std::nullopt` when the message is invalid, local, or for another project
 */
std::optional<CursorMessage>
decodeCursorMessage(const std::string& messageText, const std::string& localInstanceId, const std::string& projectKey);

} // namespace app_sync::instance_protocol
