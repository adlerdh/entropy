#pragma once

#include <uuid.h>

/**
 * @brief Generate a random RFC 4122 UUID.
 * @return Newly generated UUID using the platform random device to seed the generator.
 */
uuids::uuid generateRandomUuid();

#include <spdlog/fmt/ostr.h>
#if FMT_VERSION >= 90000
/// fmt formatter for stduuid values via their stream operator.
template<>
struct fmt::formatter<uuids::uuid> : ostream_formatter
{
};
#endif
