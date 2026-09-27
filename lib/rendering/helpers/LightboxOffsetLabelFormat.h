#pragma once

#include <string>

namespace rendering::lightbox
{

/**
 * @brief Format a lightbox offset distance using units chosen from a reference offset.
 * @param offsetMm Offset distance to label, in millimeters.
 * @param unitReferenceOffsetMm Offset whose magnitude selects the unit for every label in the lightbox.
 * @param precision Number of significant digits for non-integer length values.
 * @return Signed offset label with a metric unit suffix.
 */
std::string formatOffsetDistanceMm(double offsetMm, double unitReferenceOffsetMm, int precision = 6);

} // namespace rendering::lightbox
