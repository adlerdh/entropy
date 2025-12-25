#pragma once

#include <boost/range/any_range.hpp>
#include <uuid.h>

uuids::uuid generateRandomUuid();

#include <spdlog/fmt/ostr.h>
#if FMT_VERSION >= 90000
template<>
struct fmt::formatter<uuids::uuid> : ostream_formatter {};
#endif
