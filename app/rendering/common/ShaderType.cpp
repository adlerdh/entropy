#include "ShaderType.h"

std::string to_string(ShaderProgramType type)
{
  switch (type) {
    case ShaderProgramType::ImageGrayLinear:
      return "Image - Gray - Linear - Fixed-Point";
    case ShaderProgramType::ImageGrayLinearFloating:
      return "Image - Gray - Linear - Floating-Point";
    case ShaderProgramType::ImageGrayCubic:
      return "Image - GrayCubicTextureLookup";
    case ShaderProgramType::ImageGrayLinearWarped:
      return "Image - Gray - Linear - Fixed-Point - Warped";
    case ShaderProgramType::ImageGrayLinearFloatingWarped:
      return "Image - Gray - Linear - Floating-Point - Warped";
    case ShaderProgramType::ImageGrayCubicWarped:
      return "Image - GrayCubicTextureLookup - Warped";
    case ShaderProgramType::ImageColorLinear:
      return "Image - ColorLinearTextureLookup";
    case ShaderProgramType::ImageColorCubic:
      return "Image - ColorCubicTextureLookup";
    case ShaderProgramType::ImageColorLinearWarped:
      return "Image - ColorLinearTextureLookup - Warped";
    case ShaderProgramType::ImageColorCubicWarped:
      return "Image - ColorCubicTextureLookup - Warped";
    case ShaderProgramType::VectorDirectionColorLinear:
      return "Vector Direction Color - LinearTextureLookup";
    case ShaderProgramType::VectorDirectionColorCubic:
      return "Vector Direction Color - CubicTextureLookup";
    case ShaderProgramType::VectorSignedNormalProjectionLinear:
      return "Vector Signed Normal Projection - LinearTextureLookup";
    case ShaderProgramType::VectorSignedNormalProjectionCubic:
      return "Vector Signed Normal Projection - CubicTextureLookup";
    case ShaderProgramType::VectorPlanarProjectionColorLinear:
      return "Vector Planar Projection Color - LinearTextureLookup";
    case ShaderProgramType::VectorPlanarProjectionColorCubic:
      return "Vector Planar Projection Color - CubicTextureLookup";
    case ShaderProgramType::VectorWarpedGridLinear:
      return "Vector Warped Grid - LinearTextureLookup";
    case ShaderProgramType::VectorWarpedGridCubic:
      return "Vector Warped Grid - CubicTextureLookup";
    case ShaderProgramType::EdgeSobelLinear:
      return "Edge - Sobel - LinearTextureLookup";
    case ShaderProgramType::EdgeSobelCubic:
      return "Edge - Sobel - CubicTextureLookup";
    case ShaderProgramType::EdgeSobelLinearWarped:
      return "Edge - Sobel - LinearTextureLookup - Warped";
    case ShaderProgramType::EdgeSobelCubicWarped:
      return "Edge - Sobel - CubicTextureLookup - Warped";
    case ShaderProgramType::XrayLinear:
      return "Xray - LinearTextureLookup";
    case ShaderProgramType::XrayCubic:
      return "Xray - CubicTextureLookup";
    case ShaderProgramType::SegmentationNearest:
      return "Segmentation - Nearest - TextureLookup";
    case ShaderProgramType::SegmentationLinear:
      return "Segmentation - Linear TextureLookup";
    case ShaderProgramType::SegmentationNearestWarped:
      return "Segmentation - Nearest - TextureLookup - Warped";
    case ShaderProgramType::SegmentationLinearWarped:
      return "Segmentation - Linear TextureLookup - Warped";
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
    case ShaderProgramType::LocalNccLinear:
      return "Local NCC - Linear";
    case ShaderProgramType::LocalNccCubic:
      return "Local NCC - Cubic";
    case ShaderProgramType::LocalLinearResidualLinear:
      return "Local Linear Residual - Linear";
    case ShaderProgramType::LocalLinearResidualCubic:
      return "Local Linear Residual - Cubic";
    case ShaderProgramType::OverlapLinear:
      return "Overlap - Linear";
    case ShaderProgramType::OverlapCubic:
      return "Overlap - Cubic";
    case ShaderProgramType::AsciiPost:
      return "ASCII Post-Process";
    case ShaderProgramType::AsciiCellMean:
      return "ASCII Cell Mean Downsample";
    case ShaderProgramType::AsciiCellRegions:
      return "ASCII Cell Regions Downsample";
    case ShaderProgramType::AsciiPostSpatial:
      return "ASCII Post-Process Spatial";
    case ShaderProgramType::PixelEdgePost:
      return "Pixel Edge Post-Process";
    default:
      return "Unknown shader program type";
  }
}

std::ostream& operator<<(std::ostream& os, ShaderProgramType type)
{
  os << to_string(type);
  return os;
}
