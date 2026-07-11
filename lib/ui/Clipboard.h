#pragma once

#include "common/ClipboardPayload.h"

namespace entropy::ui
{

/**
 * @brief Put a multi-format payload on the native clipboard.
 *
 * Backends should publish every format they support from \p payload. Plain text is the required fallback and is used
 * when rich formats are unavailable on a platform.
 */
bool setClipboardPayload(const entropy::ClipboardPayload& payload);

} // namespace entropy::ui
