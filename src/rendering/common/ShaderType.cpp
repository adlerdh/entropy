#include "ShaderType.h"

std::string to_string(ShaderProgramType type) {
  switch (type) {
  case ShaderProgramType::ImageGrayLinear:
    return "Image - Gray - Linear - Fixed-Point";
  case ShaderProgramType::ImageGrayLinearFloating:
    return "Image - Gray - Linear - Floating-Point";
  case ShaderProgramType::ImageGrayCubic:
    return "Image - GrayCubicTextureLookup";
  case ShaderProgramType::ImageColorLinear:
    return "Image - ColorLinearTextureLookup";
  case ShaderProgramType::ImageColorCubic:
    return "Image - ColorCubicTextureLookup";
  case ShaderProgramType::EdgeSobelLinear:
    return "Edge - Sobel - LinearTextureLookup";
  case ShaderProgramType::EdgeSobelCubic:
    return "Edge - Sobel - CubicTextureLookup";
  case ShaderProgramType::XrayLinear:
    return "Xray - LinearTextureLookup";
  case ShaderProgramType::XrayCubic:
    return "Xray - CubicTextureLookup";
  case ShaderProgramType::SegmentationNearest:
    return "Segmentation - Nearest - TextureLookup";
  case ShaderProgramType::SegmentationLinear:
    return "Segmentation - Linear TextureLookup";
  case ShaderProgramType::IsoContourLinearFloating:
    return "IsoContour - Linear - Floating-Point";
  case ShaderProgramType::IsoContourLinearFixed:
    return "IsoContour - Linear - Fixed-Point";
  case ShaderProgramType::IsoContourCubicFixed:
    return "IsoContour - Cubic - Fixed-Point";
  case ShaderProgramType::DifferenceLinear:
    return "Difference - Linear";
  case ShaderProgramType::DifferenceCubic:
    return "Difference - Cubic";
  case ShaderProgramType::OverlapLinear:
    return "Overlap - Linear";
  case ShaderProgramType::OverlapCubic:
    return "Overlap - Cubic";
  default:
    return "Unknown shader program type";
  }
}

std::ostream& operator<<(std::ostream& os, ShaderProgramType type)
{
  os << to_string(type);
  return os;
}
