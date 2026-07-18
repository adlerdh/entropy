#include "logic/sync/ItkSnapSync.h"

#include "image/Image.h"
#include "logic/app/Data.h"
#include "logic/app/Settings.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/sync/ItkSnapSyncProtocol.h"
#include "windowing/View.h"
#include "windowing/WindowData.h"

#include <QSharedMemory>
#include <QString>

#include <glm/glm.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <csignal>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace app_sync::itk_snap_protocol;

namespace
{
constexpr double sk_cursorEpsilonMm = 1.0e-4;
constexpr double sk_zoomEpsilon = 1.0e-4;
constexpr float sk_panEpsilonMm = 1.0e-3f;

std::int64_t currentProcessId()
{
#if defined(_WIN32)
  return static_cast<std::int64_t>(GetCurrentProcessId());
#else
  return static_cast<std::int64_t>(getpid());
#endif
}

bool nearlyEqual(const glm::dvec3& a, const glm::dvec3& b)
{
  return glm::length(a - b) <= sk_cursorEpsilonMm;
}

std::optional<int> snapAnatomicalDirection(const ViewType viewType)
{
  switch (viewType) {
    case ViewType::Axial:
      return 0;
    case ViewType::Sagittal:
      return 1;
    case ViewType::Coronal:
      return 2;
    default:
      return std::nullopt;
  }
}

std::optional<double> pixelsPerMm(const WindowData& windowData, const View& view)
{
  if (!view.camera().isOrthographic()) {
    return std::nullopt;
  }

  const glm::vec2 mmPerPixel =
    helper::worldPixelSize(windowData.viewport(), view.camera(), view.viewClip_T_windowClip());
  if (mmPerPixel.x <= 0.0f || mmPerPixel.y <= 0.0f) {
    return std::nullopt;
  }

  return 2.0 / (static_cast<double>(mmPerPixel.x) + static_cast<double>(mmPerPixel.y));
}

bool isInsideImageBounds(const glm::ivec3& voxel, const glm::uvec3& dims)
{
  return glm::all(glm::greaterThanEqual(voxel, glm::ivec3{0})) && glm::all(glm::lessThan(voxel, glm::ivec3{dims}));
}

std::optional<glm::ivec3> roundedVoxelFromWorld(const Image& image, const glm::vec3& world)
{
  const glm::dvec4 pixelH = glm::dmat4{image.transformations().pixel_T_worldDef()} * glm::dvec4{glm::dvec3{world}, 1.0};
  if (0.0 == pixelH.w) {
    return std::nullopt;
  }

  const glm::dvec3 pixel = glm::dvec3{pixelH / pixelH.w};
  const glm::ivec3 rounded = glm::ivec3{glm::round(pixel)};
  if (!isInsideImageBounds(rounded, image.header().pixelDimensions())) {
    SPDLOG_TRACE(
      "Skipping ITK-SNAP cursor broadcast outside reference image: pixel=({}, {}, {}) rounded=({}, {}, {}) dims=({}, "
      "{}, "
      "{})",
      pixel.x,
      pixel.y,
      pixel.z,
      rounded.x,
      rounded.y,
      rounded.z,
      image.header().pixelDimensions().x,
      image.header().pixelDimensions().y,
      image.header().pixelDimensions().z);
    return std::nullopt;
  }

  return rounded;
}

std::optional<glm::ivec3> roundedVoxelFromSubjectLps(const Image& image, const glm::dvec3& subjectLps)
{
  const glm::dvec4 pixelH = glm::dmat4{image.transformations().pixel_T_subject()} * glm::dvec4{subjectLps, 1.0};
  if (0.0 == pixelH.w) {
    return std::nullopt;
  }

  const glm::dvec3 pixel = glm::dvec3{pixelH / pixelH.w};
  const glm::ivec3 rounded = glm::ivec3{glm::round(pixel)};
  if (!isInsideImageBounds(rounded, image.header().pixelDimensions())) {
    SPDLOG_TRACE(
      "Ignoring ITK-SNAP cursor outside reference image: lps=({}, {}, {}) pixel=({}, {}, {}) rounded=({}, {}, {}) "
      "dims=({}, {}, {})",
      subjectLps.x,
      subjectLps.y,
      subjectLps.z,
      pixel.x,
      pixel.y,
      pixel.z,
      rounded.x,
      rounded.y,
      rounded.z,
      image.header().pixelDimensions().x,
      image.header().pixelDimensions().y,
      image.header().pixelDimensions().z);
    return std::nullopt;
  }

  return rounded;
}

std::optional<glm::dvec3> subjectLpsFromVoxel(const Image& image, const glm::ivec3& voxel)
{
  const glm::dvec4 subjectH =
    glm::dmat4{image.transformations().subject_T_pixel()} * glm::dvec4{glm::dvec3{voxel}, 1.0};
  if (0.0 == subjectH.w) {
    return std::nullopt;
  }
  return glm::dvec3{subjectH / subjectH.w};
}

bool isProcessRunning(const ItkSnapIpcPid pid)
{
#if defined(_WIN32)
  if (pid <= 0 || pid > static_cast<ItkSnapIpcPid>(std::numeric_limits<DWORD>::max())) {
    return false;
  }

  const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
  if (nullptr == process) {
    return false;
  }

  DWORD exitCode = 0;
  const bool running = GetExitCodeProcess(process, &exitCode) && STILL_ACTIVE == exitCode;
  CloseHandle(process);
  return running;
#else
  return pid > 0 && 0 == kill(static_cast<pid_t>(pid), 0);
#endif
}

ItkSnapIpcLong toItkSnapIpcLong(const ItkSnapIpcPid value)
{
  if (
    value < static_cast<ItkSnapIpcPid>(std::numeric_limits<ItkSnapIpcLong>::min()) ||
    value > static_cast<ItkSnapIpcPid>(std::numeric_limits<ItkSnapIpcLong>::max()))
  {
    spdlog::warn("ITK-SNAP IPC value {} exceeds the wire integer range", value);
    return -1;
  }

  return static_cast<ItkSnapIpcLong>(value);
}
} // namespace

#if defined(_WIN32)
static_assert(sizeof(ItkSnapIpcHeader) == 12);
static_assert(sizeof(ItkSnapIpcDirectoryEntry) == 2312);
#else
static_assert(sizeof(ItkSnapIpcHeader) == 24);
static_assert(sizeof(ItkSnapIpcDirectoryEntry) == 2320);
#endif
static_assert(sizeof(ItkSnapIpcCameraState) == 112);
static_assert(sizeof(ItkSnapIpcMessage) == 184);

ItkSnapSync::ItkSnapSync(AppData& appData)
  : m_appData(appData)
  , m_sharedMemory(std::make_unique<QSharedMemory>())
  , m_processId(currentProcessId())
  , m_writeProtocolVersion(sk_snapProtocolVersionCurrent)
{
}

ItkSnapSync::~ItkSnapSync()
{
  detach();
}

void ItkSnapSync::update()
{
  const AppSettings& settings = m_appData.settings();
  logOptionChanges();
  updateCrosshairsSnappingForCursorSend();
  if (!settings.cursorSyncEnabled()) {
    detach();
    return;
  }

  if (!ensureAttached()) {
    return;
  }

  if (settings.receiveCursorSync() || settings.receiveZoomSync() || settings.receivePanSync()) {
    ItkSnapIpcMessage incoming;
    std::int64_t senderPid = -1;
    std::int64_t messageId = -1;
    if (readMessage(incoming, senderPid, messageId, true)) {
      SPDLOG_TRACE(
        "Received ITK-SNAP IPC message: senderPid={} messageId={} wireCursor=({}, {}, {}) zoom=({}, {}, {}) pan=({}, "
        "{}; "
        "{}, {}; {}, {})",
        senderPid,
        messageId,
        incoming.cursor[0],
        incoming.cursor[1],
        incoming.cursor[2],
        incoming.zoomLevel[0],
        incoming.zoomLevel[1],
        incoming.zoomLevel[2],
        incoming.viewPositionRelative[0][0],
        incoming.viewPositionRelative[0][1],
        incoming.viewPositionRelative[1][0],
        incoming.viewPositionRelative[1][1],
        incoming.viewPositionRelative[2][0],
        incoming.viewPositionRelative[2][1]);
      bool cursorApplied = false;
      if (settings.receiveCursorSync()) {
        cursorApplied = applyIncomingCursor(incoming);
      }
      if (settings.receiveZoomSync() || settings.receivePanSync()) {
        applyIncomingViewState(incoming, !cursorApplied);
      }
      if (cursorApplied) {
        m_lastBroadcastViewState = currentViewSyncState();
      }
      m_lastReceivedMessageId = messageId;
      if (settings.sendCursorSync()) {
        m_lastBroadcastCursorLps = currentReferenceLps();
      }
      if (settings.sendZoomSync() || settings.sendPanSync()) {
        m_lastBroadcastViewState = currentViewSyncState();
      }
      return;
    }
  }

  const auto cursorLps = settings.sendCursorSync() ? currentReferenceLps() : std::nullopt;
  const bool cursorChanged =
    cursorLps && (!m_lastBroadcastCursorLps || !nearlyEqual(*m_lastBroadcastCursorLps, *cursorLps));

  const auto viewState = (settings.sendZoomSync() || settings.sendPanSync()) ? currentViewSyncState() : std::nullopt;
  const bool viewChanged = viewState && viewStateChanged(*viewState);

  if (cursorChanged) {
    broadcastCursor(*cursorLps);
  }
  else if (viewChanged) {
    broadcastViewState(*viewState);
  }
  else if (viewState) {
    m_lastBroadcastViewState = *viewState;
  }
}

void ItkSnapSync::logOptionChanges()
{
  const AppSettings& settings = m_appData.settings();
  const std::array<bool, 7> options{
    settings.cursorSyncEnabled(),
    settings.sendCursorSync(),
    settings.receiveCursorSync(),
    settings.sendZoomSync(),
    settings.receiveZoomSync(),
    settings.sendPanSync(),
    settings.receivePanSync()};

  if (!m_lastLoggedOptions || *m_lastLoggedOptions != options) {
    SPDLOG_TRACE(
      "ITK-SNAP sync options: enabled={} sendCursor={} receiveCursor={} sendZoom={} receiveZoom={} sendPan={} "
      "receivePan={}",
      options[0],
      options[1],
      options[2],
      options[3],
      options[4],
      options[5],
      options[6]);
    m_lastLoggedOptions = options;
  }
}

std::optional<ItkSnapViewSyncState> ItkSnapSync::currentViewSyncState() const
{
  const glm::vec3 cursorWorld = m_appData.state().worldCrosshairs().worldOrigin();
  const WindowData& windowData = m_appData.windowData();

  ItkSnapViewSyncState state;
  bool any = false;

  const auto addView = [&](const View& view) {
    const auto dir = snapAnatomicalDirection(view.viewType());
    if (!dir || !view.camera().isOrthographic()) {
      return;
    }

    const auto zoom = pixelsPerMm(windowData, view);
    if (!zoom || !std::isfinite(*zoom) || *zoom <= 0.0) {
      return;
    }

    const glm::vec3 cursorCamera = helper::camera_T_world(view.camera(), cursorWorld);
    state.valid[*dir] = true;
    state.zoomLevel[*dir] = *zoom;
    state.viewPositionRelative[*dir][0] = -cursorCamera.x;
    state.viewPositionRelative[*dir][1] = -cursorCamera.y;
    any = true;
  };

  if (const auto activeViewUid = windowData.activeViewUid()) {
    if (const View* activeView = windowData.getCurrentView(*activeViewUid)) {
      addView(*activeView);
    }
  }

  for (const auto& viewUid : windowData.currentViewUids()) {
    if (const View* view = windowData.getCurrentView(viewUid)) {
      addView(*view);
    }
  }

  return any ? std::optional<ItkSnapViewSyncState>{state} : std::nullopt;
}

bool ItkSnapSync::viewStateChanged(const ItkSnapViewSyncState& state) const
{
  if (!m_lastBroadcastViewState) {
    return true;
  }

  const AppSettings& settings = m_appData.settings();
  if (m_lastBroadcastViewState->valid != state.valid) {
    return true;
  }

  for (int i = 0; i < 3; ++i) {
    if (!state.valid[i]) {
      continue;
    }
    if (
      settings.sendZoomSync() && std::abs(m_lastBroadcastViewState->zoomLevel[i] - state.zoomLevel[i]) > sk_zoomEpsilon)
    {
      return true;
    }
    if (
      settings.sendPanSync() &&
      (std::abs(m_lastBroadcastViewState->viewPositionRelative[i][0] - state.viewPositionRelative[i][0]) >
         sk_panEpsilonMm ||
       std::abs(m_lastBroadcastViewState->viewPositionRelative[i][1] - state.viewPositionRelative[i][1]) >
         sk_panEpsilonMm))
    {
      return true;
    }
  }

  return false;
}

bool ItkSnapSync::ensureAttached()
{
  if (m_sharedMemory->isAttached()) {
    if (!m_claimedDirectorySlot) {
      claimDirectorySlot();
    }
    return true;
  }

  m_sharedMemory->setKey(QString::fromUtf8(sk_snapSharedMemoryKey));
  if (!m_sharedMemory->attach()) {
    SPDLOG_TRACE(
      "ITK-SNAP IPC attach failed, trying create: key={} nativeKey={} error={}",
      m_sharedMemory->key().toStdString(),
      m_sharedMemory->nativeKey().toStdString(),
      m_sharedMemory->errorString().toStdString());
    const int sharedSize =
      static_cast<int>(sizeof(ItkSnapIpcHeader) + sizeof(ItkSnapIpcMessage) + sizeof(ItkSnapIpcDirectory));
    if (!m_sharedMemory->create(sharedSize)) {
      spdlog::warn(
        "Unable to attach/create ITK-SNAP IPC shared memory: key={} nativeKey={} error={}",
        m_sharedMemory->key().toStdString(),
        m_sharedMemory->nativeKey().toStdString(),
        m_sharedMemory->errorString().toStdString());
      return false;
    }

    if (m_sharedMemory->lock()) {
      auto* header = static_cast<ItkSnapIpcHeader*>(m_sharedMemory->data());
      auto* message = reinterpret_cast<ItkSnapIpcMessage*>(header + 1);
      auto* directory =
        reinterpret_cast<ItkSnapIpcDirectory*>(reinterpret_cast<char*>(message) + sizeof(ItkSnapIpcMessage));
      *header = ItkSnapIpcHeader{};
      header->version = m_writeProtocolVersion;
      *message = ItkSnapIpcMessage{};
      *directory = ItkSnapIpcDirectory{};
      m_sharedMemory->unlock();
    }
  }

  if (m_sharedMemory->isAttached() && m_sharedMemory->lock()) {
    const auto* header = static_cast<const ItkSnapIpcHeader*>(m_sharedMemory->constData());
    if (header && isSupportedProtocolVersion(header->version)) {
      m_writeProtocolVersion = header->version;
    }
    m_sharedMemory->unlock();
  }

  claimDirectorySlot();
  SPDLOG_TRACE(
    "Attached ITK-SNAP IPC shared memory: size={} pid={} key={} nativeKey={} writeProtocol=0x{:x}",
    m_sharedMemory->size(),
    static_cast<long>(m_processId),
    m_sharedMemory->key().toStdString(),
    m_sharedMemory->nativeKey().toStdString(),
    static_cast<unsigned int>(m_writeProtocolVersion));
  return true;
}

void ItkSnapSync::detach()
{
  if (m_crosshairsSnappingBeforeCursorSend) {
    m_appData.renderData().m_snapCrosshairs = *m_crosshairsSnappingBeforeCursorSend;
    SPDLOG_TRACE(
      "Restored crosshairs snapping after ITK-SNAP cursor send disabled: mode={}",
      static_cast<int>(*m_crosshairsSnappingBeforeCursorSend));
    m_crosshairsSnappingBeforeCursorSend = std::nullopt;
  }

  if (m_sharedMemory && m_sharedMemory->isAttached()) {
    releaseDirectorySlot();
    if (m_sharedMemory->lock()) {
      auto* header = static_cast<ItkSnapIpcHeader*>(m_sharedMemory->data());
      if (header && isSupportedProtocolVersion(header->version) && header->senderPid == m_processId) {
        header->senderPid = -1;
      }
      m_sharedMemory->unlock();
    }
    m_sharedMemory->detach();
  }
  m_lastBroadcastCursorLps = std::nullopt;
  m_claimedDirectorySlot = false;
}

void ItkSnapSync::updateCrosshairsSnappingForCursorSend()
{
  const AppSettings& settings = m_appData.settings();
  const bool forceReferenceVoxelSnapping = settings.cursorSyncEnabled() && settings.sendCursorSync();
  CrosshairsSnapping& snapMode = m_appData.renderData().m_snapCrosshairs;

  if (forceReferenceVoxelSnapping) {
    if (CrosshairsSnapping::ReferenceImage != snapMode) {
      if (!m_crosshairsSnappingBeforeCursorSend) {
        m_crosshairsSnappingBeforeCursorSend = snapMode;
      }
      snapMode = CrosshairsSnapping::ReferenceImage;
      SPDLOG_TRACE("Forced crosshairs snapping to reference image voxels for ITK-SNAP cursor send");
    }
    return;
  }

  if (m_crosshairsSnappingBeforeCursorSend) {
    snapMode = *m_crosshairsSnappingBeforeCursorSend;
    SPDLOG_TRACE(
      "Restored crosshairs snapping after ITK-SNAP cursor send disabled: mode={}",
      static_cast<int>(*m_crosshairsSnappingBeforeCursorSend));
    m_crosshairsSnappingBeforeCursorSend = std::nullopt;
  }
}

std::optional<glm::dvec3> ItkSnapSync::currentReferenceLps() const
{
  const Image* refImage = m_appData.refImage();
  if (!refImage) {
    return std::nullopt;
  }

  const std::optional<glm::ivec3> voxel =
    roundedVoxelFromWorld(*refImage, m_appData.state().worldCrosshairs().worldOrigin());
  if (!voxel) {
    return std::nullopt;
  }

  return subjectLpsFromVoxel(*refImage, *voxel);
}

bool ItkSnapSync::readMessage(
  ItkSnapIpcMessage& message,
  std::int64_t& senderPid,
  std::int64_t& messageId,
  bool onlyIfNew)
{
  if (!m_sharedMemory->isAttached() || !m_sharedMemory->lock()) {
    return false;
  }

  bool success = false;
  const auto* header = static_cast<const ItkSnapIpcHeader*>(m_sharedMemory->constData());
  if (header) {
    const bool changed = m_lastObservedSenderPid != header->senderPid || m_lastObservedMessageId != header->messageId;
    m_lastObservedSenderPid = header->senderPid;
    m_lastObservedMessageId = header->messageId;

    if (!isSupportedProtocolVersion(header->version)) {
      if (changed) {
        SPDLOG_TRACE(
          "Ignoring ITK-SNAP IPC header with protocol version 0x{:x}: senderPid={} messageId={}",
          static_cast<unsigned int>(header->version),
          header->senderPid,
          header->messageId);
      }
    }
    else if (header->senderPid == m_processId) {
      if (changed) {
        SPDLOG_TRACE(
          "Ignoring own ITK-SNAP IPC header: senderPid={} messageId={}",
          header->senderPid,
          header->messageId);
      }
    }
    else if (header->senderPid == -1) {
      if (changed) {
        SPDLOG_TRACE("ITK-SNAP IPC header has no active sender: messageId={}", header->messageId);
      }
    }
    else {
      if (m_writeProtocolVersion != header->version) {
        m_writeProtocolVersion = header->version;
        SPDLOG_TRACE("Using ITK-SNAP IPC protocol version 0x{:x}", static_cast<unsigned int>(m_writeProtocolVersion));
      }
      const bool alreadySeen = m_lastSenderPid == header->senderPid && m_lastReceivedMessageId == header->messageId;
      if (!onlyIfNew || !alreadySeen) {
        const auto* source = reinterpret_cast<const ItkSnapIpcMessage*>(header + 1);
        message = *source;
        senderPid = header->senderPid;
        messageId = header->messageId;
        m_lastSenderPid = senderPid;
        success = true;
      }
      else if (changed) {
        SPDLOG_TRACE(
          "ITK-SNAP IPC header already received: senderPid={} messageId={}",
          header->senderPid,
          header->messageId);
      }
    }
  }

  m_sharedMemory->unlock();
  return success;
}

void ItkSnapSync::fillOutgoingViewState(ItkSnapIpcMessage& message, const ItkSnapViewSyncState& state) const
{
  const AppSettings& settings = m_appData.settings();
  for (int i = 0; i < 3; ++i) {
    if (settings.sendZoomSync() && state.valid[i]) {
      message.zoomLevel[i] = state.zoomLevel[i];
    }

    if (settings.sendPanSync() && state.valid[i]) {
      message.viewPositionRelative[i][0] = state.viewPositionRelative[i][0];
      message.viewPositionRelative[i][1] = state.viewPositionRelative[i][1];
    }
  }
}

bool ItkSnapSync::broadcastCursor(const glm::dvec3& cursorLps)
{
  if (!m_sharedMemory->isAttached() || !m_sharedMemory->lock()) {
    return false;
  }

  auto* header = static_cast<ItkSnapIpcHeader*>(m_sharedMemory->data());
  auto* message = reinterpret_cast<ItkSnapIpcMessage*>(header + 1);

  const glm::dvec3 cursor = rasFromLps(cursorLps);

  if (const auto viewState = currentViewSyncState()) {
    fillOutgoingViewState(*message, *viewState);
    m_lastBroadcastViewState = *viewState;
  }
  else {
    fillOutgoingViewState(*message, ItkSnapViewSyncState{});
  }

  message->cursor[0] = cursor.x;
  message->cursor[1] = cursor.y;
  message->cursor[2] = cursor.z;
  header->version = m_writeProtocolVersion;
  header->senderPid = toItkSnapIpcLong(m_processId);
  header->messageId = toItkSnapIpcLong(++m_nextMessageId);
  m_lastBroadcastCursorLps = cursorLps;

#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
  const glm::dvec4 pixelH =
    glm::dmat4{m_appData.refImage()->transformations().pixel_T_subject()} * glm::dvec4{cursorLps, 1.0};
  const glm::dvec3 pixel = 0.0 == pixelH.w ? glm::dvec3{0.0} : glm::dvec3{pixelH / pixelH.w};

  SPDLOG_TRACE(
    "Broadcast ITK-SNAP cursor: voxel=({}, {}, {}) lps=({}, {}, {}) wire=({}, {}, {}) zoom=({}, {}, {}) pan=({}, {}; "
    "{}, "
    "{}; {}, {}) messageId={}",
    std::lround(pixel.x),
    std::lround(pixel.y),
    std::lround(pixel.z),
    cursorLps.x,
    cursorLps.y,
    cursorLps.z,
    cursor.x,
    cursor.y,
    cursor.z,
    message->zoomLevel[0],
    message->zoomLevel[1],
    message->zoomLevel[2],
    message->viewPositionRelative[0][0],
    message->viewPositionRelative[0][1],
    message->viewPositionRelative[1][0],
    message->viewPositionRelative[1][1],
    message->viewPositionRelative[2][0],
    message->viewPositionRelative[2][1],
    header->messageId);
#endif

  m_sharedMemory->unlock();
  return true;
}

bool ItkSnapSync::broadcastViewState(const ItkSnapViewSyncState& state)
{
  if (!m_sharedMemory->isAttached() || !m_sharedMemory->lock()) {
    return false;
  }

  auto* header = static_cast<ItkSnapIpcHeader*>(m_sharedMemory->data());
  auto* message = reinterpret_cast<ItkSnapIpcMessage*>(header + 1);

  std::optional<glm::dvec3> cursorLps;
  if (m_appData.settings().sendCursorSync()) {
    cursorLps = currentReferenceLps();
    if (cursorLps) {
      const glm::dvec3 cursor = rasFromLps(*cursorLps);
      message->cursor[0] = cursor.x;
      message->cursor[1] = cursor.y;
      message->cursor[2] = cursor.z;
      m_lastBroadcastCursorLps = *cursorLps;
    }
  }

  fillOutgoingViewState(*message, state);

  header->version = m_writeProtocolVersion;
  header->senderPid = toItkSnapIpcLong(m_processId);
  header->messageId = toItkSnapIpcLong(++m_nextMessageId);
  m_lastBroadcastViewState = state;

  SPDLOG_TRACE(
    "Broadcast ITK-SNAP view state: cursorUpdated={} zoom=({}, {}, {}) pan=({}, {}; {}, {}; {}, {}) messageId={}",
    cursorLps.has_value(),
    message->zoomLevel[0],
    message->zoomLevel[1],
    message->zoomLevel[2],
    message->viewPositionRelative[0][0],
    message->viewPositionRelative[0][1],
    message->viewPositionRelative[1][0],
    message->viewPositionRelative[1][1],
    message->viewPositionRelative[2][0],
    message->viewPositionRelative[2][1],
    header->messageId);

  m_sharedMemory->unlock();
  return true;
}

bool ItkSnapSync::applyIncomingCursor(const ItkSnapIpcMessage& message)
{
  const Image* refImage = m_appData.refImage();
  if (!refImage) {
    return false;
  }

  const glm::dvec3 wireCursor{message.cursor[0], message.cursor[1], message.cursor[2]};
  glm::dvec3 cursor = lpsFromRas(wireCursor);

  const std::optional<glm::ivec3> voxel = roundedVoxelFromSubjectLps(*refImage, cursor);
  if (!voxel) {
    return false;
  }

  const std::optional<glm::dvec3> voxelSubjectLps = subjectLpsFromVoxel(*refImage, *voxel);
  if (!voxelSubjectLps) {
    return false;
  }

  if (const auto current = currentReferenceLps(); current && nearlyEqual(*current, *voxelSubjectLps)) {
    m_lastBroadcastCursorLps = *voxelSubjectLps;
    return false;
  }

  const glm::dvec4 worldPos =
    glm::dmat4{refImage->transformations().worldDef_T_pixel()} * glm::dvec4{glm::dvec3{*voxel}, 1.0};
  if (0.0 == worldPos.w) {
    return false;
  }

  m_appData.state().setWorldCrosshairsPos(glm::vec3{worldPos / worldPos.w});
  m_lastBroadcastCursorLps = *voxelSubjectLps;
  SPDLOG_TRACE(
    "Applied ITK-SNAP cursor: wire=({}, {}, {}) lps=({}, {}, {}) voxel=({}, {}, {})",
    wireCursor.x,
    wireCursor.y,
    wireCursor.z,
    voxelSubjectLps->x,
    voxelSubjectLps->y,
    voxelSubjectLps->z,
    voxel->x,
    voxel->y,
    voxel->z);
  return true;
}

void ItkSnapSync::applyIncomingViewState(const ItkSnapIpcMessage& message, bool applyPan)
{
  const AppSettings& settings = m_appData.settings();
  WindowData& windowData = m_appData.windowData();
  const glm::vec3 cursorWorld = m_appData.state().worldCrosshairs().worldOrigin();

  for (const auto& viewUid : windowData.currentViewUids()) {
    View* view = windowData.getCurrentView(viewUid);
    if (!view || !view->camera().isOrthographic()) {
      continue;
    }

    const auto dir = snapAnatomicalDirection(view->viewType());
    if (!dir) {
      continue;
    }

    if (settings.receiveZoomSync()) {
      const double targetPixelsPerMm = message.zoomLevel[*dir];
      const auto currentPixelsPerMm = pixelsPerMm(windowData, *view);
      if (
        currentPixelsPerMm && std::isfinite(targetPixelsPerMm) && targetPixelsPerMm > 0.0 &&
        std::abs(*currentPixelsPerMm - targetPixelsPerMm) > sk_zoomEpsilon)
      {
        const double zoomFactor = targetPixelsPerMm / *currentPixelsPerMm;
        view->camera().setZoom(static_cast<float>(zoomFactor * view->camera().getZoom()));
        SPDLOG_TRACE(
          "Applied ITK-SNAP zoom: view={} dir={} targetPixelsPerMm={} previousPixelsPerMm={} cameraZoom={}",
          to_string(view->viewType(), false),
          *dir,
          targetPixelsPerMm,
          *currentPixelsPerMm,
          view->camera().getZoom());
      }
    }

    if (settings.receivePanSync() && applyPan) {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
      const glm::vec3 cursorCameraBefore = helper::camera_T_world(view->camera(), cursorWorld);
#endif
      const glm::vec2 targetCursorCamera{
        -message.viewPositionRelative[*dir][0],
        -message.viewPositionRelative[*dir][1]};
      const glm::vec3 cursorCamera = helper::camera_T_world(view->camera(), cursorWorld);
      const glm::vec2 delta = glm::vec2{cursorCamera} - targetCursorCamera;

      if (glm::length(delta) > sk_panEpsilonMm) {
        helper::translateAboutCamera(view->camera(), glm::vec3{delta, 0.0f});
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
        const glm::vec3 cursorCameraAfter = helper::camera_T_world(view->camera(), cursorWorld);
        SPDLOG_TRACE(
          "Applied ITK-SNAP pan: view={} dir={} targetOffset=({}, {}) previousCursorCamera=({}, {}) "
          "newCursorCamera=({}, "
          "{})",
          to_string(view->viewType(), false),
          *dir,
          message.viewPositionRelative[*dir][0],
          message.viewPositionRelative[*dir][1],
          cursorCameraBefore.x,
          cursorCameraBefore.y,
          cursorCameraAfter.x,
          cursorCameraAfter.y);
#endif
      }
    }
  }
}

void ItkSnapSync::claimDirectorySlot()
{
  if (m_claimedDirectorySlot || !m_sharedMemory->isAttached() || !m_sharedMemory->lock()) {
    return;
  }

  auto* header = static_cast<ItkSnapIpcHeader*>(m_sharedMemory->data());
  auto* message = reinterpret_cast<ItkSnapIpcMessage*>(header + 1);
  auto* directory =
    reinterpret_cast<ItkSnapIpcDirectory*>(reinterpret_cast<char*>(message) + sizeof(ItkSnapIpcMessage));

  if (
    header && m_sharedMemory->size() >=
                static_cast<int>(sizeof(ItkSnapIpcHeader) + sizeof(ItkSnapIpcMessage) + sizeof(ItkSnapIpcDirectory)))
  {
    int target = -1;
    for (int i = 0; i < sk_maxInstances; ++i) {
      const ItkSnapIpcPid pid = directory->entries[i].pid;
      if (0 == pid || pid == m_processId || !isProcessRunning(pid)) {
        target = i;
        break;
      }
    }

    if (target >= 0) {
      auto& entry = directory->entries[target];
      entry.pid = toItkSnapIpcLong(m_processId);
      constexpr const char* title = "Entropy";
      std::fill(std::begin(entry.title), std::end(entry.title), '\0');
      std::copy_n(title, std::min(std::strlen(title), sizeof(entry.title) - 1), entry.title);
      entry.pendingDropId = 0;
      entry.pendingDrop[0] = '\0';
      m_claimedDirectorySlot = true;
      SPDLOG_TRACE("Claimed ITK-SNAP IPC directory slot {}", target);
    }
  }

  m_sharedMemory->unlock();
}

void ItkSnapSync::releaseDirectorySlot()
{
  if (!m_claimedDirectorySlot || !m_sharedMemory->isAttached() || !m_sharedMemory->lock()) {
    return;
  }

  auto* header = static_cast<ItkSnapIpcHeader*>(m_sharedMemory->data());
  auto* message = reinterpret_cast<ItkSnapIpcMessage*>(header + 1);
  auto* directory =
    reinterpret_cast<ItkSnapIpcDirectory*>(reinterpret_cast<char*>(message) + sizeof(ItkSnapIpcMessage));

  if (
    header && m_sharedMemory->size() >=
                static_cast<int>(sizeof(ItkSnapIpcHeader) + sizeof(ItkSnapIpcMessage) + sizeof(ItkSnapIpcDirectory)))
  {
    for (auto& entry : directory->entries) {
      if (entry.pid == m_processId) {
        entry.pid = 0;
        entry.title[0] = '\0';
        entry.pendingDropId = 0;
        entry.pendingDrop[0] = '\0';
        break;
      }
    }
  }

  m_claimedDirectorySlot = false;
  m_sharedMemory->unlock();
}
