#pragma once

#include <iostream>

enum class ShaderProgramType
{
  ImageGrayLinear,
  ImageGrayCubic,
  ImageColorLinear,
  ImageColorCubic,
  EdgeLinear,
  EdgeCubic,
  XrayLinear,
  XrayCubic,
  SegmentationNearest,
  SegmentationLinear,
  IsoContourLinearFloating,
  IsoContourLinearFixed,
  IsoContourCubicFixed,
  DifferenceLinear,
  DifferenceCubic,
  OverlapLinear,
  OverlapCubic
};

std::string to_string(ShaderProgramType type);
std::ostream& operator<<(std::ostream& os, ShaderProgramType type);
