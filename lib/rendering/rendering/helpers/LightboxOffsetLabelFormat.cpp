#include "rendering/helpers/LightboxOffsetLabelFormat.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace rendering::lightbox
{
namespace
{

struct DistanceUnit
{
  double scale = 1.0;
  const char* label = "mm";
};

std::string trimTrailingDecimalZeros(std::string value)
{
  const auto decimalPos = value.find('.');
  if (std::string::npos == decimalPos) {
    return value;
  }

  while (!value.empty() && value.back() == '0') {
    value.pop_back();
  }
  if (!value.empty() && value.back() == '.') {
    value.pop_back();
  }
  return value;
}

DistanceUnit distanceUnitForLightboxOffsetReferenceMm(double referenceMm)
{
  const double absReferenceMm = std::abs(referenceMm);

  if (absReferenceMm <= std::numeric_limits<float>::epsilon()) {
    return {1.0, "mm"};
  }
  if (absReferenceMm >= 1.0e6) {
    return {1.0 / 1.0e6, "km"};
  }
  if (absReferenceMm >= 1000.0) {
    return {1.0 / 1000.0, "m"};
  }
  if (absReferenceMm >= 10.0) {
    return {1.0 / 10.0, "cm"};
  }
  if (absReferenceMm >= 0.1) {
    return {1.0, "mm"};
  }
  if (absReferenceMm >= 1.0e-6) {
    return {1000.0, "µm"};
  }
  return {1.0e6, "nm"};
}

} // namespace

std::string formatOffsetDistanceMm(double offsetMm, double unitReferenceOffsetMm, int precision)
{
  const DistanceUnit unit = distanceUnitForLightboxOffsetReferenceMm(unitReferenceOffsetMm);
  const double value = offsetMm * unit.scale;

  if (std::abs(value) <= std::numeric_limits<float>::epsilon()) {
    return std::string("0 ") + unit.label;
  }

  const double roundedValue = std::round(value);
  const double integerTolerance = 64.0 * std::numeric_limits<float>::epsilon() * std::max(1.0, std::abs(value));

  std::ostringstream out;
  out << std::showpos;
  if (std::abs(value - roundedValue) <= integerTolerance) {
    out << static_cast<int>(roundedValue);
  }
  else {
    out << std::fixed << std::setprecision(std::max(0, precision)) << value;
  }
  return trimTrailingDecimalZeros(out.str()) + " " + unit.label;
}

} // namespace rendering::lightbox
