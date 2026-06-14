#include "logic/sync/EntropyInstanceSync.h"

#include "image/Image.h"
#include "logic/app/AppPaths.h"
#include "logic/app/Data.h"
#include "logic/app/Settings.h"
#include "logic/app/State.h"
#include "logic/sync/EntropyInstanceSyncProtocol.h"
#include "windowing/WindowData.h"

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <process.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace entropy::sync
{
namespace
{
constexpr std::int64_t sk_registryWriteIntervalMs = 1000;
constexpr std::size_t sk_receiveBufferSize = 8192;

using namespace entropy::sync::instance_protocol;

#if defined(_WIN32)
using NativeSocketHandle = SOCKET;
constexpr NativeSocketHandle sk_invalidSocket = INVALID_SOCKET;
#else
using NativeSocketHandle = int;
constexpr NativeSocketHandle sk_invalidSocket = -1;
#endif

NativeSocketHandle nativeSocket(EntropyInstanceSync::SocketHandle socket)
{
#if defined(_WIN32)
  return static_cast<SOCKET>(socket);
#else
  return socket;
#endif
}

EntropyInstanceSync::SocketHandle storedSocket(NativeSocketHandle socket)
{
#if defined(_WIN32)
  return static_cast<EntropyInstanceSync::SocketHandle>(socket);
#else
  return socket;
#endif
}

std::int64_t nowMs()
{
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

std::uint32_t processId()
{
#if defined(_WIN32)
  return static_cast<std::uint32_t>(_getpid());
#else
  return static_cast<std::uint32_t>(getpid());
#endif
}

std::string randomInstanceId()
{
  std::random_device randomDevice;
  std::mt19937_64 generator{randomDevice()};
  std::uniform_int_distribution<std::uint64_t> distribution;

  std::ostringstream stream;
  stream << std::hex << distribution(generator) << distribution(generator);
  return stream.str();
}

std::filesystem::path canonicalPath(const std::filesystem::path& path)
{
  std::error_code error;
  auto canonical = std::filesystem::weakly_canonical(path, error);
  if (!error) {
    return canonical;
  }

  auto absolute = std::filesystem::absolute(path, error);
  return error ? path : absolute;
}

std::filesystem::path syncRegistryDirectory()
{
  const std::filesystem::path userDataDirectory = app_paths::userDataDirectory();
  if (!userDataDirectory.empty() && userDataDirectory.is_absolute()) {
    return userDataDirectory / "sync" / "instances";
  }

  std::error_code error;
  std::filesystem::path tempDirectory = std::filesystem::temp_directory_path(error);
  if (!error && !tempDirectory.empty()) {
    return tempDirectory / "entropy" / "sync" / "instances";
  }

  return std::filesystem::absolute("entropy-sync") / "instances";
}

bool socketWouldBlock()
{
#if defined(_WIN32)
  const int error = WSAGetLastError();
  return WSAEWOULDBLOCK == error;
#else
  return EWOULDBLOCK == errno || EAGAIN == errno;
#endif
}

bool setNonBlocking(NativeSocketHandle socket)
{
#if defined(_WIN32)
  u_long mode = 1;
  return 0 == ioctlsocket(socket, FIONBIO, &mode);
#else
  const int flags = fcntl(socket, F_GETFL, 0);
  return flags >= 0 && 0 == fcntl(socket, F_SETFL, flags | O_NONBLOCK);
#endif
}

void closeNativeSocket(NativeSocketHandle socket)
{
  if (sk_invalidSocket == socket) {
    return;
  }
#if defined(_WIN32)
  closesocket(socket);
#else
  close(socket);
#endif
}

bool ensureWinsockStarted()
{
#if defined(_WIN32)
  static const bool started = [] {
    WSADATA data{};
    return 0 == WSAStartup(MAKEWORD(2, 2), &data);
  }();
  return started;
#else
  return true;
#endif
}
} // namespace

EntropyInstanceSync::EntropyInstanceSync(AppData& appData)
  : m_appData(appData)
  , m_instanceId(randomInstanceId())
  , m_registryDirectory(syncRegistryDirectory())
  , m_registryFile(m_registryDirectory / (m_instanceId + ".json"))
  , m_socket(storedSocket(sk_invalidSocket))
{
}

EntropyInstanceSync::~EntropyInstanceSync()
{
  stop();
}

void EntropyInstanceSync::update()
{
  logOptionChanges();

  if (!m_appData.settings().entropyInstanceSyncEnabled()) {
    stop();
    return;
  }

  if (!ensureRunning()) {
    return;
  }

  const std::string currentProjectKey = projectKey();
  if (currentProjectKey.empty()) {
    return;
  }

  if (m_projectKey != currentProjectKey) {
    m_projectKey = currentProjectKey;
    m_lastRegistryWriteMs = 0;
    m_lastBroadcastCursorLps.reset();
    m_lastReceivedSequenceByInstance.clear();
  }

  writeRegistryFile();
  receiveMessages();
  broadcastChangedState(discoverPeers());
}

bool EntropyInstanceSync::ensureRunning()
{
  if (!openSocket()) {
    return false;
  }

  std::error_code error;
  std::filesystem::create_directories(m_registryDirectory, error);
  if (error) {
    spdlog::warn(
      "Could not create Entropy sync registry directory {}: {}",
      m_registryDirectory.string(),
      error.message());
    return false;
  }

  return true;
}

void EntropyInstanceSync::stop()
{
  std::error_code error;
  std::filesystem::remove(m_registryFile, error);
  closeSocket();
  m_port = 0;
  m_projectKey.clear();
  m_lastBroadcastCursorLps.reset();
  m_lastReceivedSequenceByInstance.clear();
}

bool EntropyInstanceSync::openSocket()
{
  if (sk_invalidSocket != nativeSocket(m_socket)) {
    return true;
  }
  if (!ensureWinsockStarted()) {
    spdlog::warn("Could not initialize Windows sockets for Entropy instance synchronization");
    return false;
  }

  NativeSocketHandle socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sk_invalidSocket == socket) {
    spdlog::warn("Could not open Entropy instance synchronization socket");
    return false;
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  if (0 != bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address))) {
    spdlog::warn("Could not bind Entropy instance synchronization socket");
    closeNativeSocket(socket);
    return false;
  }

  if (!setNonBlocking(socket)) {
    spdlog::warn("Could not make Entropy instance synchronization socket nonblocking");
    closeNativeSocket(socket);
    return false;
  }

  sockaddr_in boundAddress{};
#if defined(_WIN32)
  int boundAddressSize = sizeof(boundAddress);
#else
  socklen_t boundAddressSize = sizeof(boundAddress);
#endif
  if (0 != getsockname(socket, reinterpret_cast<sockaddr*>(&boundAddress), &boundAddressSize)) {
    spdlog::warn("Could not determine Entropy instance synchronization port");
    closeNativeSocket(socket);
    return false;
  }

  m_socket = storedSocket(socket);
  m_port = ntohs(boundAddress.sin_port);
  SPDLOG_DEBUG("Entropy instance synchronization listening on 127.0.0.1:{}", m_port);
  return true;
}

void EntropyInstanceSync::closeSocket()
{
  closeNativeSocket(nativeSocket(m_socket));
  m_socket = storedSocket(sk_invalidSocket);
}

void EntropyInstanceSync::writeRegistryFile()
{
  const std::int64_t now = nowMs();
  if (now - m_lastRegistryWriteMs < sk_registryWriteIntervalMs) {
    return;
  }

  const std::filesystem::path tempFile = m_registryFile.string() + ".tmp";
  {
    std::ofstream stream{tempFile};
    if (!stream) {
      spdlog::warn("Could not write Entropy sync registry file {}", tempFile.string());
      return;
    }
    stream << encodeRegistryRecord(m_instanceId, processId(), m_port, m_projectKey, now);
  }

  std::error_code error;
  std::filesystem::rename(tempFile, m_registryFile, error);
  if (error) {
    std::filesystem::remove(m_registryFile, error);
    error.clear();
    std::filesystem::rename(tempFile, m_registryFile, error);
  }
  if (error) {
    spdlog::warn("Could not publish Entropy sync registry file {}: {}", m_registryFile.string(), error.message());
    return;
  }

  m_lastRegistryWriteMs = now;
}

std::vector<EntropyInstanceSync::Peer> EntropyInstanceSync::discoverPeers()
{
  std::vector<Peer> peers;
  std::error_code error;
  if (!std::filesystem::exists(m_registryDirectory, error)) {
    return peers;
  }

  const std::int64_t now = nowMs();
  for (const auto& entry : std::filesystem::directory_iterator{m_registryDirectory, error}) {
    if (error || !entry.is_regular_file(error) || entry.path() == m_registryFile) {
      continue;
    }

    std::ifstream stream{entry.path()};
    std::ostringstream recordStream;
    recordStream << stream.rdbuf();
    const std::string record = recordStream.str();
    if (record.empty()) {
      continue;
    }

    if (peerRecordIsStale(record, now)) {
      std::error_code removeError;
      std::filesystem::remove(entry.path(), removeError);
      continue;
    }

    const auto peer = decodePeerRecord(record, m_instanceId, m_projectKey, now);
    if (peer) {
      peers.push_back(Peer{peer->instanceId, peer->port});
    }
  }

  return peers;
}

void EntropyInstanceSync::receiveMessages()
{
  std::array<char, sk_receiveBufferSize> buffer{};
  while (true) {
    sockaddr_in sourceAddress{};
#if defined(_WIN32)
    int sourceAddressSize = sizeof(sourceAddress);
    const int received = recvfrom(
      nativeSocket(m_socket),
      buffer.data(),
      static_cast<int>(buffer.size()),
      0,
      reinterpret_cast<sockaddr*>(&sourceAddress),
      &sourceAddressSize);
#else
    socklen_t sourceAddressSize = sizeof(sourceAddress);
    const ssize_t received = recvfrom(
      nativeSocket(m_socket),
      buffer.data(),
      buffer.size(),
      0,
      reinterpret_cast<sockaddr*>(&sourceAddress),
      &sourceAddressSize);
#endif

    if (received < 0) {
      if (!socketWouldBlock()) {
        SPDLOG_TRACE("Entropy instance synchronization receive failed");
      }
      return;
    }
    if (0 == received) {
      return;
    }

    applyMessage(std::string{buffer.data(), static_cast<std::size_t>(received)});
  }
}

void EntropyInstanceSync::broadcastChangedState(const std::vector<Peer>& peers)
{
  if (peers.empty()) {
    return;
  }

  const glm::dvec3 cursorLps{m_appData.state().worldCrosshairs().worldOrigin()};
  if (m_lastBroadcastCursorLps && nearlyEqual(*m_lastBroadcastCursorLps, cursorLps)) {
    return;
  }

  const std::string message = encodeMessage();
  bool sent = false;
  for (const auto& peer : peers) {
    sent = sendMessage(peer, message) || sent;
  }

  if (sent) {
    m_lastBroadcastCursorLps = cursorLps;
  }
}

bool EntropyInstanceSync::sendMessage(const Peer& peer, const std::string& message)
{
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(peer.port);

#if defined(_WIN32)
  const int sent = sendto(
    nativeSocket(m_socket),
    message.data(),
    static_cast<int>(message.size()),
    0,
    reinterpret_cast<const sockaddr*>(&address),
    sizeof(address));
  const bool ok = sent == static_cast<int>(message.size());
#else
  const ssize_t sent = sendto(
    nativeSocket(m_socket),
    message.data(),
    message.size(),
    0,
    reinterpret_cast<const sockaddr*>(&address),
    sizeof(address));
  const bool ok = sent == static_cast<ssize_t>(message.size());
#endif

  if (!ok) {
    SPDLOG_TRACE("Could not send Entropy synchronization message to peer {} on port {}", peer.instanceId, peer.port);
  }
  return ok;
}

std::string EntropyInstanceSync::encodeMessage()
{
  ++m_nextSequence;
  const glm::vec3 cursor = m_appData.state().worldCrosshairs().worldOrigin();
  return encodeCursorMessage(m_instanceId, m_nextSequence, m_projectKey, glm::dvec3{cursor});
}

void EntropyInstanceSync::applyMessage(const std::string& message)
{
  const auto decoded = decodeCursorMessage(message, m_instanceId, m_projectKey);
  if (!decoded) {
    return;
  }

  auto& lastSequence = m_lastReceivedSequenceByInstance[decoded->sender];
  if (decoded->sequence <= lastSequence) {
    return;
  }
  lastSequence = decoded->sequence;

  const glm::dvec3 current{m_appData.state().worldCrosshairs().worldOrigin()};
  if (!nearlyEqual(decoded->cursorLps, current)) {
    m_appData.state().setWorldCrosshairsPos(glm::vec3{decoded->cursorLps});
    m_lastBroadcastCursorLps = decoded->cursorLps;
    SPDLOG_TRACE(
      "Applied Entropy instance cursor from {}: lps=({}, {}, {})",
      decoded->sender,
      decoded->cursorLps.x,
      decoded->cursorLps.y,
      decoded->cursorLps.z);
  }
}

std::string EntropyInstanceSync::projectKey() const
{
  if (const auto& fileName = m_appData.projectFileName()) {
    return canonicalPath(*fileName).string();
  }

  std::vector<std::string> imageFiles;
  imageFiles.reserve(m_appData.numImages());
  for (const auto& imageUid : m_appData.imageUidsOrdered()) {
    const Image* image = m_appData.image(imageUid);
    if (!image) {
      continue;
    }
    imageFiles.push_back(canonicalPath(image->header().fileName()).string());
  }

  if (imageFiles.empty()) {
    return {};
  }

  std::ostringstream stream;
  for (const auto& file : imageFiles) {
    stream << file << '\n';
  }
  return stream.str();
}

void EntropyInstanceSync::logOptionChanges()
{
  const bool enabled = m_appData.settings().entropyInstanceSyncEnabled();
  if (!m_lastLoggedEnabled || *m_lastLoggedEnabled != enabled) {
    SPDLOG_TRACE("Entropy instance cursor sync enabled={}", enabled);
    m_lastLoggedEnabled = enabled;
  }
}

} // namespace entropy::sync
