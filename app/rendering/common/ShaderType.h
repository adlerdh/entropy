#pragma once

#include <iostream>

enum class ShaderProgramType
{
  ImageGrayLinear,
  ImageGrayLinearFloating,
  ImageGrayCubic,
  ImageColorLinear,
  ImageColorCubic,
  VectorDirectionColorLinear,
  VectorDirectionColorCubic,
  VectorSignedNormalProjectionLinear,
  VectorSignedNormalProjectionCubic,
  VectorPlanarProjectionColorLinear,
  VectorPlanarProjectionColorCubic,
  VectorWarpedGridLinear,
  VectorWarpedGridCubic,
  EdgeSobelLinear,
  EdgeSobelCubic,
  XrayLinear,
  XrayCubic,
  SegmentationNearest,
  SegmentationLinear,
  IsoContourLinearFloating,
  IsoContourLinearFixed,
  IsoContourCubicFixed,
  DifferenceLinear,
  DifferenceCubic,
  LocalNccLinear,
  LocalNccCubic,
  LocalLinearResidualLinear,
  LocalLinearResidualCubic,
  OverlapLinear,
  OverlapCubic,
  AsciiPost,
  AsciiCellMean,
  AsciiCellRegions,
  AsciiPostSpatial,
  PixelEdgePost
};

std::string to_string(ShaderProgramType type);
std::ostream& operator<<(std::ostream& os, ShaderProgramType type);
