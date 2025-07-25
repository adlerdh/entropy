#pragma once

#include <iostream>

enum class ShaderProgramType
{
  ImageGrayLinearTextureLookup,
  ImageGrayCubicTextureLookup,
  ImageColorLinearTextureLookup,
  ImageColorCubicTextureLookup,
  SegmentationNearestTextureLookup,
  SegmentationLinearTextureLookup
};

std::string to_string(ShaderProgramType type);
std::ostream& operator<<(std::ostream& os, ShaderProgramType type);
