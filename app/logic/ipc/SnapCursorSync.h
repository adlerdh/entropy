#pragma once

#include "common/Types.h"

#include <glm/vec3.hpp>

#include <cstdint>
#include <array>
#include <memory>
#include <optional>

class AppData;
class QSharedMemory;
struct SnapIpcMessage;

struct SnapViewSyncState
{
  std::array<bool, 3> valid = {false, false, false};
  std::array<double, 3> zoomLevel = {0.0, 0.0, 0.0};
  std::array<std::array<float, 2>, 3> viewPositionRelative = {{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}}};
};

class SnapCursorSync
{
public:
  explicit SnapCursorSync(AppData& appData);
  ~SnapCursorSync();

  SnapCursorSync(const SnapCursorSync&) = delete;
  SnapCursorSync& operator=(const SnapCursorSync&) = delete;

  void update();

private:
  bool ensureAttached();
  void detach();

  void updateCrosshairsSnappingForCursorSend();
  std::optional<glm::dvec3> currentReferenceLps() const;
  std::optional<SnapViewSyncState> currentViewSyncState() const;
  bool readMessage(SnapIpcMessage& message, std::int64_t& senderPid, std::int64_t& messageId, bool onlyIfNew);
  void logOptionChanges();
  void fillOutgoingViewState(SnapIpcMessage& message, const SnapViewSyncState& state) const;
  bool viewStateChanged(const SnapViewSyncState& state) const;
  bool broadcastCursor(const glm::dvec3& cursorLps);
  bool broadcastViewState(const SnapViewSyncState& state);
  bool applyIncomingCursor(const SnapIpcMessage& message);
  void applyIncomingViewState(const SnapIpcMessage& message);
  void claimDirectorySlot();
  void releaseDirectorySlot();

  AppData& m_appData;
  std::unique_ptr<QSharedMemory> m_sharedMemory;
  std::int64_t m_processId = 0;
  std::int64_t m_nextMessageId = 0;
  std::int64_t m_lastSenderPid = -1;
  std::int64_t m_lastReceivedMessageId = -1;
  std::int64_t m_lastObservedSenderPid = -2;
  std::int64_t m_lastObservedMessageId = -2;
  std::int16_t m_writeProtocolVersion = 0;
  std::optional<glm::dvec3> m_lastBroadcastCursorLps;
  std::optional<SnapViewSyncState> m_lastBroadcastViewState;
  std::optional<std::array<bool, 7>> m_lastLoggedOptions;
  std::optional<CrosshairsSnapping> m_crosshairsSnappingBeforeCursorSend;
  bool m_claimedDirectorySlot = false;
};
