#pragma once

#include <glm/vec3.hpp>

#include <cstdint>

namespace app_sync::itk_snap_protocol
{

/** @brief Qt shared-memory key used by ITK-SNAP for cross-process synchronization. */
inline constexpr const char* sk_snapSharedMemoryKey = "5A636Q488E.itksnap";

/** @brief Current ITK-SNAP IPC protocol version written by Entropy. */
inline constexpr std::int16_t sk_snapProtocolVersionCurrent = 0x1006;

/** @brief Older ITK-SNAP IPC protocol version that Entropy can still read and write after attaching. */
inline constexpr std::int16_t sk_snapProtocolVersionLegacy = 0x1005;

/** @brief Maximum number of process directory entries stored in the ITK-SNAP shared-memory segment. */
inline constexpr int sk_maxInstances = 16;

#if defined(_WIN32)
/** @brief Integer type used by ITK-SNAP IPC on Windows shared-memory payloads. */
using ItkSnapIpcLong = std::int32_t;
#else
/** @brief Integer type used by ITK-SNAP IPC on POSIX shared-memory payloads. */
using ItkSnapIpcLong = long;
#endif

/** @brief Process identifier type used by Entropy before conversion to ITK-SNAP's wire integer. */
using ItkSnapIpcPid = std::int64_t;

/** @brief Monotonic message identifier type used by Entropy before conversion to ITK-SNAP's wire integer. */
using ItkSnapIpcMessageId = std::int64_t;

/** @brief Header stored at the start of the ITK-SNAP shared-memory block. */
struct ItkSnapIpcHeader
{
  std::int16_t version = 0;      //!< ITK-SNAP IPC protocol version
  ItkSnapIpcLong senderPid = -1; //!< Process ID that wrote the current message
  ItkSnapIpcLong messageId = 0;  //!< Sender-local message sequence number
};

/** @brief Legacy camera fields retained for binary compatibility with ITK-SNAP's IPC message layout. */
struct ItkSnapIpcCameraState
{
  double position[3] = {0.0, 0.0, 0.0};   //!< Camera position in ITK-SNAP's wire format
  double focalPoint[3] = {0.0, 0.0, 0.0}; //!< Camera focal point in ITK-SNAP's wire format
  double viewUp[3] = {0.0, 0.0, 0.0};     //!< Camera up direction in ITK-SNAP's wire format
  double clippingRange[2] = {0.0, 0.0};   //!< Camera clipping range in ITK-SNAP's wire format
  double viewAngle = 0.0;                 //!< Perspective view angle
  double parallelScale = 0.0;             //!< Orthographic parallel scale
  int parallelProjection = 0;             //!< Non-zero when ITK-SNAP encoded an orthographic camera
  int padding = 0;                        //!< Padding retained to match ITK-SNAP's binary struct size
};

/** @brief Live ITK-SNAP IPC payload containing cursor and per-anatomical-view camera synchronization state. */
struct ItkSnapIpcMessage
{
  double cursor[3] = {0.0, 0.0, 0.0};    //!< Cursor position in ITK-SNAP RAS subject coordinates
  double zoomLevel[3] = {0.0, 0.0, 0.0}; //!< Axial, sagittal, and coronal zoom levels in pixels/mm
  float viewPositionRelative[3][2] = {
    {0.0f, 0.0f},
    {0.0f, 0.0f},
    {0.0f, 0.0f}};              //!< Cursor-relative pan offsets for axial, sagittal, and coronal views
  ItkSnapIpcCameraState camera; //!< Legacy camera payload retained for wire compatibility
};

/** @brief Directory entry advertised by an ITK-SNAP-compatible process in the shared-memory block. */
struct ItkSnapIpcDirectoryEntry
{
  ItkSnapIpcLong pid = 0;           //!< Process ID owning this directory slot, or zero when unused
  char title[256] = {};             //!< Human-readable process/window title
  ItkSnapIpcLong pendingDropId = 0; //!< Identifier for pending file-drop IPC payloads
  char pendingDrop[2048] = {};      //!< Pending file-drop payload retained for ITK-SNAP compatibility
};

/** @brief Fixed-size process directory stored after the live IPC message. */
struct ItkSnapIpcDirectory
{
  ItkSnapIpcDirectoryEntry entries[sk_maxInstances] = {}; //!< Process directory slots
};

/** @brief Convert Entropy's internal LPS coordinates to ITK-SNAP RAS coordinates. */
glm::dvec3 rasFromLps(const glm::dvec3& lps);

/** @brief Convert ITK-SNAP RAS coordinates to Entropy's internal LPS coordinates. */
glm::dvec3 lpsFromRas(const glm::dvec3& ras);

/** @brief Return whether an ITK-SNAP IPC protocol version can be read by Entropy. */
bool isSupportedProtocolVersion(std::int16_t version);

} // namespace app_sync::itk_snap_protocol
