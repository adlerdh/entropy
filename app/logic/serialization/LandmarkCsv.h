#pragma once
#include "logic/annotation/PointRecord.h"
#include <glm/vec3.hpp>
#include <map>
#include <ostream>

namespace serialize
{
/// Stream boundary shared by atomic file publication and fault-injecting sinks.
/// Flushes before reporting success. Stream exceptions propagate to the caller.
bool writeLandmarkCsv(std::ostream& out, const std::map<std::size_t, PointRecord<glm::vec3>>& landmarks);
} // namespace serialize
