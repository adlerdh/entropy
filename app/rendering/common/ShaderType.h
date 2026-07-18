#pragma once

#include <iostream>
#include <string>

// Abbreviations used below: linear interpolation (linear), cubic B-spline interpolation (cubic), nearest-neighbor
// interpolation (nearest), and inverse deformation field sampling (warped)

/// Identifies a linked OpenGL shader program variant used by the image rendering pipeline
enum class ShaderProgramType
{
  AsciiCellMean,                      //!< ASCII cell mean reduction pass
  AsciiCellRegions,                   //!< ASCII cell region classification pass
  AsciiPost,                          //!< ASCII post-process pass
  AsciiPostSpatial,                   //!< ASCII spatial matching post-process pass
  DifferenceCubic,                    //!< Difference comparison with cubic
  DifferenceCubicWarped,              //!< Warped difference comparison with cubic
  DifferenceLinear,                   //!< Difference comparison with linear
  DifferenceLinearWarped,             //!< Warped difference comparison with linear
  EdgeSobelCubic,                     //!< Sobel edge with cubic
  EdgeSobelCubicWarped,               //!< Warped Sobel edge with cubic
  EdgeSobelLinear,                    //!< Sobel edge with linear
  EdgeSobelLinearWarped,              //!< Warped Sobel edge with linear
  ImageColorCubic,                    //!< RGB or RGBA image sampling with cubic
  ImageColorCubicWarped,              //!< Warped RGB or RGBA image sampling with cubic
  ImageColorLinear,                   //!< RGB or RGBA image sampling with linear
  ImageColorLinearWarped,             //!< Warped RGB or RGBA image sampling with linear
  ImageGrayCubic,                     //!< Scalar grayscale image sampling with cubic
  ImageGrayCubicWarped,               //!< Warped scalar grayscale image sampling with cubic
  ImageGrayLinear,                    //!< Scalar grayscale image sampling with linear
  ImageGrayLinearFloating,            //!< Floating-point grayscale image sampling with linear
  ImageGrayLinearFloatingWarped,      //!< Warped floating-point grayscale image sampling with linear
  ImageGrayLinearWarped,              //!< Warped scalar grayscale image sampling with linear
  IsoContourCubicFixed,               //!< Fixed-point image isocontour with cubic
  IsoContourCubicFixedWarped,         //!< Warped fixed-point image isocontour with cubic
  IsoContourLinearFixed,              //!< Fixed-point image isocontour with linear
  IsoContourLinearFixedWarped,        //!< Warped fixed-point image isocontour with linear
  IsoContourLinearFloating,           //!< Floating-point image isocontour with linear
  IsoContourLinearFloatingWarped,     //!< Warped floating-point image isocontour with linear
  LocalLinearResidualCubic,           //!< Local linear residual comparison with cubic
  LocalLinearResidualCubicWarped,     //!< Warped local linear residual comparison with cubic
  LocalLinearResidualLinear,          //!< Local linear residual comparison with linear
  LocalLinearResidualLinearWarped,    //!< Warped local linear residual comparison with linear
  LocalNccCubic,                      //!< Local NCC comparison with cubic
  LocalNccCubicWarped,                //!< Warped local NCC comparison with cubic
  LocalNccLinear,                     //!< Local NCC comparison with linear
  LocalNccLinearWarped,               //!< Warped local NCC comparison with linear
  OverlapCubic,                       //!< Two-image overlap comparison with cubic
  OverlapCubicWarped,                 //!< Warped two-image overlap comparison with cubic
  OverlapLinear,                      //!< Two-image overlap comparison with linear
  OverlapLinearWarped,                //!< Warped two-image overlap comparison with linear
  PixelEdgePost,                      //!< Pixel edge overlay post-process pass
  SegmentationLinear,                 //!< Segmentation with linear
  SegmentationLinearWarped,           //!< Warped segmentation with linear
  SegmentationNearest,                //!< Segmentation label with nearest
  SegmentationNearestWarped,          //!< Warped segmentation label with nearest
  VectorDirectionColorCubic,          //!< Vector direction color with cubic
  VectorDirectionColorLinear,         //!< Vector direction color with linear
  VectorPlanarProjectionColorCubic,   //!< In-plane vector projection color with cubic
  VectorPlanarProjectionColorLinear,  //!< In-plane vector projection color with linear
  VectorSignedNormalProjectionCubic,  //!< Signed vector projection onto the slice normal with cubic
  VectorSignedNormalProjectionLinear, //!< Signed vector projection onto the slice normal with linear
  VectorWarpedGridCubic,              //!< Warped grid overlay from a vector field with cubic
  VectorWarpedGridLinear,             //!< Warped grid overlay from a vector field with linear
  XrayCubic,                          //!< Intensity projection or X-ray with cubic
  XrayCubicWarped,                    //!< Warped intensity projection or X-ray with cubic
  XrayLinear,                         //!< Intensity projection or X-ray with linear
  XrayLinearWarped                    //!< Warped intensity projection or X-ray with linear
};

std::string to_string(ShaderProgramType type);
std::ostream& operator<<(std::ostream& os, ShaderProgramType type);
