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

namespace app_sync
{

/**
 * @brief Synchronizes cursor state between running Entropy instances
 *
 * Instances discover each other through short-lived registry files in the user
 * data directory and exchange live updates through UDP on the loopback
 * interface. Cursor coordinates are encoded in Entropy's internal LPS world
 * coordinates
 */
class EntropyInstanceSync
{
public:
#if defined(_WIN32)
  /** @brief Platform-independent storage type for a native Windows socket handle */
  using SocketHandle = std::uintptr_t;
#else
  /** @brief Platform-independent storage type for a POSIX socket descriptor */
  using SocketHandle = int;
#endif

  /**
   * @brief Construct a cursor synchronizer bound to the shared application state
   * @param appData Shared application data whose cursor state is synchronized
   * @throw Propagates exceptions from filesystem path construction or random ID generation
   */
  explicit EntropyInstanceSync(AppData& appData);

  /**
   * @brief Stop synchronization, close sockets, and remove this instance's registry file
   */
  ~EntropyInstanceSync();

  EntropyInstanceSync(const EntropyInstanceSync&) = delete;
  EntropyInstanceSync& operator=(const EntropyInstanceSync&) = delete;

  /**
   * @brief Poll peer messages, publish this instance heartbeat, and broadcast changed state
   *
   * Filesystem and socket failures are treated as transient synchronization
   * failures; they are logged and otherwise ignored
   *
   */
  void update();

private:
  /**
   * @brief Peer endpoint discovered from a registry file
   */
  struct Peer
  {
    /** @brief Stable random ID advertised by the peer instance */
    std::string instanceId;

    /** @brief UDP loopback port on which the peer receives sync messages */
    std::uint16_t port = 0;
  };

  /**
   * @brief Ensure the UDP socket and registry directory are available
   * @return True when synchronization can proceed
   */
  bool ensureRunning();

  /**
   * @brief Stop synchronization and clear all transient peer and broadcast state
   */
  void stop();

  /**
   * @brief Open and bind the non-blocking UDP socket if it is not already open
   * @return True when a socket is open and bound
   */
  bool openSocket();

  /**
   * @brief Close the native UDP socket
   */
  void closeSocket();

  /**
   * @brief Write this process's heartbeat file into the sync registry directory
   */
  void writeRegistryFile();

  /**
   * @brief Discover live peer registry files for the currently loaded project or image list
   * @return Peer endpoints that match the current project key
   */
  std::vector<Peer> discoverPeers();

  /**
   * @brief Drain pending UDP messages and apply valid peer cursor updates
   */
  void receiveMessages();

  /**
   * @brief Broadcast current cursor state to peers when it has changed
   * @param peers Peer endpoints for the current project
   */
  void broadcastChangedState(const std::vector<Peer>& peers);

  /**
   * @brief Send a serialized sync message to one peer
   * @param peer Peer endpoint that should receive the message
   * @param message Serialized cursor update message
   * @return True when the full message was sent
   */
  bool sendMessage(const Peer& peer, const std::string& message) const;

  /**
   * @brief Serialize the current cursor state into the Entropy-instance sync JSON message
   * @return Serialized cursor update message
   * @throw Propagates exceptions from JSON or string allocation
   */
  std::string encodeMessage();

  /**
   * @brief Parse and apply an incoming Entropy-instance sync JSON message
   * @param message Serialized cursor update message
   */
  void applyMessage(const std::string& message);

  /**
   * @brief Return the project or image-list key used to match compatible Entropy instances
   * @return Canonical project key, or an empty string when no project or images are loaded
   * @throw Propagates exceptions from image access or string allocation
   */
  std::string projectKey() const;

  /**
   * @brief Emit trace logging when the Entropy-instance sync enabled state changes
   */
  void logOptionChanges();

  /** @brief Shared application data containing settings, images, and cursor state */
  AppData& m_appData;
  /** @brief Stable random ID for this running Entropy instance */
  std::string m_instanceId;
  /** @brief Directory containing peer heartbeat registry files */
  std::filesystem::path m_registryDirectory;
  /** @brief Heartbeat registry file written by this instance */
  std::filesystem::path m_registryFile;
  /** @brief Current project or image-list key used to match peers */
  std::string m_projectKey;
  /** @brief UDP loopback port bound by this instance */
  std::uint16_t m_port = 0;
  /** @brief Next sender-local cursor message sequence number */
  std::uint64_t m_nextSequence = 0;
  /** @brief Last monotonic timestamp at which the registry file was written */
  std::int64_t m_lastRegistryWriteMs = 0;
  /** @brief Last cursor position broadcast by this instance */
  std::optional<glm::dvec3> m_lastBroadcastCursorLps;
  /** @brief Last logged enabled state used to avoid repeated trace messages */
  std::optional<bool> m_lastLoggedEnabled;
  /** @brief Highest received sequence number for each peer instance ID */
  std::unordered_map<std::string, std::uint64_t> m_lastReceivedSequenceByInstance;

  /** @brief Native socket handle stored in a platform-independent type */
  SocketHandle m_socket = 0;
};

} // namespace app_sync
