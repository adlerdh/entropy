#include "logic/sync/EntropyInstanceSyncProtocol.h"

#include <glm/geometric.hpp>
#include <nlohmann/json.hpp>

#include <limits>

namespace app_sync::instance_protocol
{
namespace
{
constexpr int sk_protocolMajorVersion = 1;
constexpr int sk_protocolMinorVersion = 0;

nlohmann::json protocolVersionJson()
{
  return nlohmann::json{{"major", sk_protocolMajorVersion}, {"minor", sk_protocolMinorVersion}};
}

bool isSupportedProtocolVersion(const nlohmann::json& root)
{
  const auto version = root.find("version");
  if (version == root.end() || !version->is_object()) {
    return false;
  }

  const auto major = version->find("major");
  const auto minor = version->find("minor");
  return major != version->end() && minor != version->end() && major->is_number_integer() &&
         minor->is_number_integer() && major->get<int>() == sk_protocolMajorVersion &&
         minor->get<int>() == sk_protocolMinorVersion;
}
} // namespace

bool nearlyEqual(const glm::dvec3& a, const glm::dvec3& b)
{
  return glm::length(a - b) <= sk_cursorEpsilonMm;
}

std::string encodeRegistryRecord(
  const std::string& instanceId,
  std::uint32_t processId,
  std::uint16_t port,
  const std::string& projectKey,
  std::int64_t updatedMs)
{
  const nlohmann::json record{
    {"version", protocolVersionJson()},
    {"instanceId", instanceId},
    {"pid", processId},
    {"port", port},
    {"projectKey", projectKey},
    {"updatedMs", updatedMs}};

  return record.dump(2);
}

std::optional<PeerRecord> decodePeerRecord(
  const std::string& recordText,
  const std::string& localInstanceId,
  const std::string& projectKey,
  std::int64_t nowMs)
{
  const auto record = nlohmann::json::parse(recordText, nullptr, false);
  if (record.is_discarded() || !record.is_object()) {
    return std::nullopt;
  }

  const std::int64_t updatedMs = record.value("updatedMs", std::int64_t{0});
  if (nowMs - updatedMs > sk_peerStaleMs) {
    return std::nullopt;
  }

  const std::string instanceId = record.value("instanceId", std::string{});
  if (
    !isSupportedProtocolVersion(record) || instanceId.empty() || instanceId == localInstanceId ||
    record.value("projectKey", std::string{}) != projectKey)
  {
    return std::nullopt;
  }

  const int port = record.value("port", 0);
  if (port <= 0 || port > std::numeric_limits<std::uint16_t>::max()) {
    return std::nullopt;
  }

  return PeerRecord{instanceId, static_cast<std::uint16_t>(port)};
}

bool peerRecordIsStale(const std::string& recordText, std::int64_t nowMs)
{
  const auto record = nlohmann::json::parse(recordText, nullptr, false);
  if (record.is_discarded() || !record.is_object()) {
    return false;
  }

  const std::int64_t updatedMs = record.value("updatedMs", std::int64_t{0});
  return nowMs - updatedMs > sk_peerStaleMs;
}

std::string encodeCursorMessage(
  const std::string& sender,
  std::uint64_t sequence,
  const std::string& projectKey,
  const glm::dvec3& cursorLps)
{
  nlohmann::json message{
    {"version", protocolVersionJson()},
    {"sender", sender},
    {"sequence", sequence},
    {"projectKey", projectKey},
    {"cursorLps", {cursorLps.x, cursorLps.y, cursorLps.z}}};

  return message.dump();
}

std::optional<CursorMessage>
decodeCursorMessage(const std::string& messageText, const std::string& localInstanceId, const std::string& projectKey)
{
  const auto root = nlohmann::json::parse(messageText, nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    return std::nullopt;
  }

  const std::string sender = root.value("sender", std::string{});
  if (
    !isSupportedProtocolVersion(root) || root.value("projectKey", std::string{}) != projectKey || sender.empty() ||
    sender == localInstanceId)
  {
    return std::nullopt;
  }

  const std::uint64_t sequence = root.value("sequence", std::uint64_t{0});
  const auto cursor = root.find("cursorLps");
  if (0 == sequence || cursor == root.end() || !cursor->is_array() || cursor->size() != 3) {
    return std::nullopt;
  }

  try {
    return CursorMessage{
      sender,
      sequence,
      glm::dvec3{cursor->at(0).get<double>(), cursor->at(1).get<double>(), cursor->at(2).get<double>()}};
  }
  catch (const nlohmann::json::exception&) {
    return std::nullopt;
  }
}

} // namespace app_sync::instance_protocol
