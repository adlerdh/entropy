#pragma once

#include "logic/serialization/ProjectSerialization.h"

namespace project_snapshot
{
/**
 * @brief Compare two serialized project snapshots for saved-state equivalence.
 *
 * This comparison is intentionally structural rather than JSON-text based. It ignores no
 * project fields that affect Entropy's current saved/dirty decision.
 *
 * @param a First project snapshot.
 * @param b Second project snapshot.
 * @return True when the snapshots represent the same saved project state.
 */
bool equivalent(const serialize::EntropyProject& a, const serialize::EntropyProject& b);
} // namespace project_snapshot
