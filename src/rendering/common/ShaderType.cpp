#include "ShaderType.h"

std::string to_string(ShaderProgramType type) {
  switch (type) {
  case ShaderProgramType::ImageGrayLinearTextureLookup:
    return "ImageGrayLinearTextureLookup";
  case ShaderProgramType::ImageGrayCubicTextureLookup:
    return "ImageGrayCubicTextureLookup";
  case ShaderProgramType::ImageColorLinearTextureLookup:
    return "ImageColorLinearTextureLookup";
  case ShaderProgramType::ImageColorCubicTextureLookup:
    return "ImageColorCubicTextureLookup";
  case ShaderProgramType::SegmentationNearestTextureLookup:
    return "SegmentationNearestTextureLookup";
  case ShaderProgramType::SegmentationLinearTextureLookup:
    return "SegmentationLinearTextureLookup";
  default:
    return "Unknown shader program type";
  }
}

std::ostream& operator<<(std::ostream& os, ShaderProgramType type)
{
  os << to_string(type);
  return os;
}
