#pragma once
#include "logic/annotation/PointRecord.h"
#include <glm/vec3.hpp>
#include <map>
#include <ostream>

namespace serialize
{
/// Write landmark records as CSV and flush the destination before reporting success.
/// @param out Destination stream, also usable with fault-injecting test sinks.
/// @param landmarks Records ordered by landmark index.
/// @return Whether the stream remains in a successful state after writing and flushing.
/// @throws std::ios_base::failure If the stream's exception mask enables an encountered I/O error.
bool writeLandmarkCsv(std::ostream& out, const std::map<std::size_t, PointRecord<glm::vec3>>& landmarks);
} // namespace serialize
