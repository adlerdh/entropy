#pragma once

#include "common/Types.h"

#include <glm/vec3.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class AppData;

namespace entropy::sync
{

/**
 * @brief Synchronizes cursor state between running Entropy instances.
 *
 * Instances discover each other through short-lived registry files in the user
 * data directory and exchange live updates through UDP on the loopback
 * interface. Cursor coordinates are encoded in Entropy's internal LPS world
 * coordinates.
 */
class EntropyInstanceSync
{
public:
#if defined(_WIN32)
  /** @brief Platform-independent storage type for a native Windows socket handle. */
  using SocketHandle = std::uintptr_t;
#else
  /** @brief Platform-independent storage type for a POSIX socket descriptor. */
  using SocketHandle = int;
#endif

  /** @brief Construct a cursor synchronizer bound to the shared application state. */
  explicit EntropyInstanceSync(AppData& appData);

  /** @brief Stop synchronization, close sockets, and remove this instance's registry file. */
  ~EntropyInstanceSync();

  EntropyInstanceSync(const EntropyInstanceSync&) = delete;
  EntropyInstanceSync& operator=(const EntropyInstanceSync&) = delete;

  /**
   * @brief Poll peer messages, publish this instance heartbeat, and broadcast changed state.
   *
   * Filesystem and socket failures are treated as transient synchronization
   * failures; they are logged and otherwise ignored.
   */
  void update();

private:
  struct Peer
  {
    /** @brief Stable random ID advertised by the peer instance. */
    std::string instanceId;

    /** @brief UDP loopback port on which the peer receives sync messages. */
    std::uint16_t port = 0;
  };

  /** @brief Ensure the UDP socket and registry directory are available. */
  bool ensureRunning();

  /** @brief Stop synchronization and clear all transient peer/broadcast state. */
  void stop();

  /** @brief Open and bind the non-blocking UDP socket if it is not already open. */
  bool openSocket();

  /** @brief Close the native UDP socket. */
  void closeSocket();

  /** @brief Write this process's heartbeat file into the sync registry directory. */
  void writeRegistryFile();

  /** @brief Discover live peer registry files for the currently loaded project/image list. */
  std::vector<Peer> discoverPeers();

  /** @brief Drain pending UDP messages and apply valid peer cursor updates. */
  void receiveMessages();

  /** @brief Broadcast current cursor state to peers when it has changed. */
  void broadcastChangedState(const std::vector<Peer>& peers);

  /** @brief Send a serialized sync message to one peer. */
  bool sendMessage(const Peer& peer, const std::string& message);

  /** @brief Serialize the current cursor state into the Entropy-instance sync JSON message. */
  std::string encodeMessage();

  /** @brief Parse and apply an incoming Entropy-instance sync JSON message. */
  void applyMessage(const std::string& message);

  /** @brief Return the project/image-list key used to match compatible Entropy instances. */
  std::string projectKey() const;

  /** @brief Emit trace logging when the Entropy-instance sync enabled state changes. */
  void logOptionChanges();

  AppData& m_appData;
  std::string m_instanceId;
  std::filesystem::path m_registryDirectory;
  std::filesystem::path m_registryFile;
  std::string m_projectKey;
  std::uint16_t m_port = 0;
  std::uint64_t m_nextSequence = 0;
  std::int64_t m_lastRegistryWriteMs = 0;
  std::optional<glm::dvec3> m_lastBroadcastCursorLps;
  std::optional<bool> m_lastLoggedEnabled;
  std::unordered_map<std::string, std::uint64_t> m_lastReceivedSequenceByInstance;

  SocketHandle m_socket = 0;
};

} // namespace entropy::sync
