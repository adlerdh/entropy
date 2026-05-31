#pragma once

#include <vector>
#include <uuid.h>

/// Convenience alias for an owning range of UIDs.
using uuid_range_t = std::vector<uuids::uuid>;
