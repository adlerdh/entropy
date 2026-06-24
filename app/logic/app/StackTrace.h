#pragma once

#include <string>

namespace stack_trace
{
/**
 * @brief Return a best-effort stack trace for the current thread.
 * @param skipFrames Number of leading frames to omit from the trace.
 * @return Human-readable stack trace, or a short message if unavailable.
 */
std::string current(unsigned int skipFrames = 0);

/**
 * @brief Install best-effort process crash and terminate handlers.
 *
 * The handlers are intended for diagnostics only. Signal handlers write directly to stderr because
 * normal logging is not async-signal-safe after a hard crash.
 */
void installCrashHandlers();
} // namespace stack_trace
