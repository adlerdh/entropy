#pragma once

#include <uuid.h>
#include <vector>

/// Convenience alias for an owning range of UIDs.
using uuid_range_t = std::vector<uuids::uuid>;
