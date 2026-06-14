#pragma once

#include "common/Types.h"
#include "logic/sync/ItkSnapSyncProtocol.h"

#include <glm/vec3.hpp>

#include <cstdint>
#include <array>
#include <memory>
#include <optional>

class AppData;
class QSharedMemory;

using ItkSnapIpcMessage = entropy::sync::itk_snap_protocol::ItkSnapIpcMessage;

/**
 * @brief Per-anatomical-view state exchanged with ITK-SNAP.
 *
 * The three array slots are axial, sagittal, and coronal. A slot is valid only
 * when Entropy can derive a stable orthographic view state for that anatomical
 * direction.
 */
struct ItkSnapViewSyncState
{
  /** @brief Whether each anatomical-view slot contains usable zoom and pan values. */
  std::array<bool, 3> valid = {false, false, false};

  /** @brief Pixels per millimeter for axial, sagittal, and coronal views. */
  std::array<double, 3> zoomLevel = {0.0, 0.0, 0.0};

  /** @brief Cursor-relative pan offset for each anatomical view, in millimeters. */
  std::array<std::array<float, 2>, 3> viewPositionRelative = {{{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}}};
};

/**
 * @brief Synchronizes Entropy cursor, zoom, and pan state with ITK-SNAP.
 *
 * ITK-SNAP exposes a shared-memory IPC block containing one live cursor/view
 * message and a small process directory. This class attaches to that block,
 * converts between Entropy's internal LPS coordinates and ITK-SNAP's RAS wire
 * coordinates, and applies the enabled send/receive options from AppSettings.
 */
class ItkSnapSync
{
public:
  /** @brief Construct an ITK-SNAP synchronizer bound to the shared application state. */
  explicit ItkSnapSync(AppData& appData);

  /** @brief Detach from shared memory and restore any temporary cursor-snap settings. */
  ~ItkSnapSync();

  ItkSnapSync(const ItkSnapSync&) = delete;
  ItkSnapSync& operator=(const ItkSnapSync&) = delete;

  /**
   * @brief Poll ITK-SNAP IPC, apply incoming state, and publish changed outgoing state.
   *
   * This is intentionally non-throwing in normal operation; unavailable shared
   * memory, stale directory entries, and unsupported peer messages are logged
   * and skipped.
   */
  void update();

private:
  /** @brief Attach to or create the ITK-SNAP shared-memory block. */
  bool ensureAttached();

  /** @brief Release shared-memory state and reset local synchronization caches. */
  void detach();

  /** @brief Force reference-image voxel snapping while cursor sending is enabled. */
  void updateCrosshairsSnappingForCursorSend();

  /** @brief Return the current crosshairs position in reference-image subject LPS coordinates. */
  std::optional<glm::dvec3> currentReferenceLps() const;

  /** @brief Capture current orthographic zoom and pan state for ITK-SNAP-compatible anatomical views. */
  std::optional<ItkSnapViewSyncState> currentViewSyncState() const;

  /** @brief Read the shared-memory message header and payload when it is valid and new. */
  bool readMessage(ItkSnapIpcMessage& message, std::int64_t& senderPid, std::int64_t& messageId, bool onlyIfNew);

  /** @brief Emit trace logging when synchronization settings change. */
  void logOptionChanges();

  /** @brief Fill outgoing zoom and pan fields from the captured Entropy view state. */
  void fillOutgoingViewState(ItkSnapIpcMessage& message, const ItkSnapViewSyncState& state) const;

  /** @brief Return whether captured view state differs enough to broadcast. */
  bool viewStateChanged(const ItkSnapViewSyncState& state) const;

  /** @brief Broadcast cursor position, plus current view state when available. */
  bool broadcastCursor(const glm::dvec3& cursorLps);

  /** @brief Broadcast zoom and pan state without changing the cursor payload. */
  bool broadcastViewState(const ItkSnapViewSyncState& state);

  /** @brief Apply an incoming ITK-SNAP cursor message to Entropy crosshairs. */
  bool applyIncomingCursor(const ItkSnapIpcMessage& message);

  /** @brief Apply incoming ITK-SNAP zoom and pan fields to compatible Entropy views. */
  void applyIncomingViewState(const ItkSnapIpcMessage& message);

  /** @brief Claim or refresh this process's directory slot in the shared-memory block. */
  void claimDirectorySlot();

  /** @brief Release this process's directory slot in the shared-memory block. */
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
  std::optional<ItkSnapViewSyncState> m_lastBroadcastViewState;
  std::optional<std::array<bool, 7>> m_lastLoggedOptions;
  std::optional<CrosshairsSnapping> m_crosshairsSnappingBeforeCursorSend;
  bool m_claimedDirectorySlot = false;
};
