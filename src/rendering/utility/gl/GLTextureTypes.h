#ifndef GL_TEXTURE_TYPES_H
#define GL_TEXTURE_TYPES_H

#include <glad/glad.h>

#include <cstdint>

namespace tex
{

enum class Target : uint32_t
{
  Texture1D = GL_TEXTURE_1D,
  Texture2D = GL_TEXTURE_2D,
  Texture3D = GL_TEXTURE_3D,
  TextureCubeMap = GL_TEXTURE_CUBE_MAP,
  Texture1DArray = GL_TEXTURE_1D_ARRAY,
  Texture2DArray = GL_TEXTURE_2D_ARRAY,
  Texture2DMultisample = GL_TEXTURE_2D_MULTISAMPLE,
  TextureRectangle = GL_TEXTURE_RECTANGLE,
  Texture2DMultisampleArray = GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
  TextureBuffer = GL_TEXTURE_BUFFER

  /// @internal These are not available on OpenGL 3.3:
  /// TargetCubeMapArray = GL_TEXTURE_CUBE_MAP_ARRAY
};

enum class Binding : uint32_t
{
  TextureBinding1D = GL_TEXTURE_BINDING_1D,
  TextureBinding2D = GL_TEXTURE_BINDING_2D,
  TextureBinding3D = GL_TEXTURE_BINDING_3D,
  TextureBindingCubeMap = GL_TEXTURE_BINDING_CUBE_MAP,
  TextureBinding1DArray = GL_TEXTURE_BINDING_1D_ARRAY,
  TextureBinding2DArray = GL_TEXTURE_BINDING_2D_ARRAY,
  TextureBinding2DMultisample = GL_TEXTURE_BINDING_2D_MULTISAMPLE,
  TextureBindingRectangle = GL_TEXTURE_BINDING_RECTANGLE,
  TextureBinding2DMultisampleArray = GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY
};

enum class CubeMapFace : uint32_t
{
  PositiveX = GL_TEXTURE_CUBE_MAP_POSITIVE_X,
  NegativeX = GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
  PositiveY = GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
  NegativeY = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
  PositiveZ = GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
  NegativeZ = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
};

enum class BufferPixelFormat : uint32_t
{
  /// Floating point or normalized (un)signed integer formats
  Red = GL_RED,
  RG = GL_RG,
  RGB = GL_RGB,
  BGR = GL_BGR,
  RGBA = GL_RGBA,
  BGRA = GL_BGRA,

  /// Explicit integer formats
  Red_Integer = GL_RED_INTEGER,
  RG_Integer = GL_RG_INTEGER,
  RGB_Integer = GL_RGB_INTEGER,
  BGR_Integer = GL_BGR_INTEGER,
  RGBA_Integer = GL_RGBA_INTEGER,
  BGRA_Integer = GL_BGRA_INTEGER,

  /// Formats for stencil and/or depth buffers
  StencilIndex = GL_STENCIL_INDEX,
  DepthComponent = GL_DEPTH_COMPONENT,
  DepthStencil = GL_DEPTH_STENCIL
};

/// The type​ parameter of a pixel transfer function defines how many bits each of the components
/// defined by the format​ take up.
enum class BufferPixelDataType : uint32_t
{
  /// Values that specify each component as a separate byte value:
  UInt8 = GL_UNSIGNED_BYTE,
  Int8 = GL_BYTE,
  UInt16 = GL_UNSIGNED_SHORT,
  Int16 = GL_SHORT,
  UInt32 = GL_UNSIGNED_INT,
  Int32 = GL_INT,
  Float16 = GL_HALF_FLOAT,
  Float32 = GL_FLOAT,

  UInt24_8 = GL_UNSIGNED_INT_24_8,

  Float_32_UInt_24_8_Rev = GL_FLOAT_32_UNSIGNED_INT_24_8_REV,

  /// Values that pack multiple components into a single value:

  /// @note GL_[base type​]_[size1​]_[size2​]_[size3​]_[size4​](_REV​):
  /// The base type​ is the OpenGL type enumerator name of the fully packed value.
  /// The size​ values represent the sizes of the components (in bits), in that order.
  /// By default the components are laid out from msb (most-significant bit) to lsb (least-significant bit).
  /// However, if the type​ has a _REV​ at the end of it, the component order is reversed, and they are laid out
  /// from lsb-to-msb.

  UInt8_RG3B2 = GL_UNSIGNED_BYTE_3_3_2,
  UInt8_RG3B2_Rev = GL_UNSIGNED_BYTE_2_3_3_REV,
  UInt16_R5G6B5 = GL_UNSIGNED_SHORT_5_6_5,
  UInt16_R5G6B5_Rev = GL_UNSIGNED_SHORT_5_6_5_REV,
  UInt16_RGBA4 = GL_UNSIGNED_SHORT_4_4_4_4,
  UInt16_RGBA4_Rev = GL_UNSIGNED_SHORT_4_4_4_4_REV,
  UInt16_RGB5A1 = GL_UNSIGNED_SHORT_5_5_5_1,
  UInt16_RGB5A1_Rev = GL_UNSIGNED_SHORT_1_5_5_5_REV,
  UInt32_RGBA8 = GL_UNSIGNED_INT_8_8_8_8,
  UInt32_RGBA8_Rev = GL_UNSIGNED_INT_8_8_8_8_REV,
  UInt32_RGB10A2 = GL_UNSIGNED_INT_10_10_10_2,
  UInt32_RGB10A2_Rev = GL_UNSIGNED_INT_2_10_10_10_REV
};

/// GL_[components​][size​][type​]
///
/// The components​ field is the list of components that the format stores.
/// OpenGL only allows "R", "RG", "RGB", or "RGBA"; other combinations are not allowed as internal image formats
///
/// The following suffixes are used:
/// "": No type suffix means unsigned normalized integer format.
/// "_SNORM": Signed normalized integer format.
/// "F": Floating-point. Thus, GL_RGBA32F is a floating-point format where each component is a 32-bit IEEE floating-point value.
/// "I": Signed integral format. Thus GL_RGBA8I gives a signed integer format where each of the four components is an integer on the range [-128, 127].
/// "UI": Unsigned integral format. The values go from [0, MAX_INT] for the integer size.

/// @note There are a number of color formats that exist outside of the normal syntax described above
enum class SizedInternalFormat : uint32_t
{
  R8_UNorm = GL_R8,
  R8_SNorm = GL_R8_SNORM,
  R16_UNorm = GL_R16,
  R16_SNorm = GL_R16_SNORM,

  RG8_UNorm = GL_RG8,
  RG8_SNorm = GL_RG8_SNORM,
  RG16_UNorm = GL_RG16,
  RG16_SNorm = GL_RG16_SNORM,

  RGB8_UNorm = GL_RGB8,
  RGB8_SNorm = GL_RGB8_SNORM,
  RGB16_UNorm = GL_RGB16,
  RGB16_SNorm = GL_RGB16_SNORM,

  RGBA8_UNorm = GL_RGBA8,
  RGBA8_SNorm = GL_RGBA8_SNORM,
  RGBA16_UNorm = GL_RGBA16,
  RGBA16_SNorm = GL_RGBA16_SNORM,

  RG3B2_UNorm = GL_R3_G3_B2, //!< Normalized integer, with 3 bits for R and G, but only 2 for B
  RGB4 = GL_RGB4,
  RGB5 = GL_RGB5,

  RGB10_UNorm = GL_RGB10,
  RGB12_UNorm = GL_RGB12,
  RGBA2_UNorm = GL_RGBA2,
  RGBA4 = GL_RGBA4,
  RGB5A1 = GL_RGB5_A1, //!< 5 bits each for RGB, 1 for Alpha

  /// 10 bits each for RGB, 2 for Alpha. This can be a useful format for framebuffers,
  /// if you do not need a high-precision destination alpha value. It carries more color depth,
  /// thus preserving subtle gradations
  RGB10A2 = GL_RGB10_A2,

  /// 10 bits each for RGB, 2 for Alpha, as unsigned integers. There is no signed integral version
  RGB10A2UI = GL_RGB10_A2UI,

  RGBA12 = GL_RGBA12,

  /// sRGB image with no alpha
  SRGB8_UNorm = GL_SRGB8,

  /// sRGB image with a linear Alpha
  SRGB8_Alpha8_UNorm = GL_SRGB8_ALPHA8,

  R16F = GL_R16F,
  RG16F = GL_RG16F,
  RGB16F = GL_RGB16F,
  RGBA16F = GL_RGBA16F,
  R32F = GL_R32F,
  RG32F = GL_RG32F,
  RGB32F = GL_RGB32F,
  RGBA32F = GL_RGBA32F,

  /// This uses special 11 and 10-bit floating-point values.
  /// An 11-bit float has no sign-bit; it has 6 bits of mantissa and 5 bits of exponent.
  /// A 10-bit float has no sign-bit, 5 bits of mantissa and 5 bits of exponent.
  /// This is very economical for floating-point values (using only 32-bits per value),
  /// so long as your floating-point data will fit within the given range. And so long as you can
  /// live without the destination alpha.
  RG11B10F = GL_R11F_G11F_B10F,

  /// It is an RGB format of type floating-point. The 3 color values have 9 bits of precision,
  /// and they share a single exponent. The computation for these values is not as simple as for
  /// GL_R11F_G11F_B10F, and they aren't appropriate for everything. But they can provide better
  /// results than that format if most of the colors in the image have approximately the same exponent,
  /// or are too small to be significant.
  RGB9E5 = GL_RGB9_E5,

  R8I = GL_R8I,
  R8U = GL_R8UI,
  R16I = GL_R16I,
  R16U = GL_R16UI,
  R32I = GL_R32I,
  R32U = GL_R32UI,

  RG8I = GL_RG8I,
  RG8U = GL_RG8UI,
  RG16I = GL_RG16I,
  RG16U = GL_RG16UI,
  RG32I = GL_RG32I,
  RG32U = GL_RG32UI,

  RGB8I = GL_RGB8I,
  RGB8U = GL_RGB8UI,
  RGB16I = GL_RGB16I,
  RGB16U = GL_RGB16UI,
  RGB32I = GL_RGB32I,
  RGB32U = GL_RGB32UI,

  RGBA8I = GL_RGBA8I,
  RGBA8U = GL_RGBA8UI,
  RGBA16I = GL_RGBA16I,
  RGBA16U = GL_RGBA16UI,
  RGBA32I = GL_RGBA32I,
  RGBA32U = GL_RGBA32UI,

  /// Depth buffer formats
  Depth16_UNorm = GL_DEPTH_COMPONENT16,
  Depth24_UNorm = GL_DEPTH_COMPONENT24,
  Depth32_UNorm = GL_DEPTH_COMPONENT32,
  Depth32F = GL_DEPTH_COMPONENT32F,

  /// Stencil buffer formats
  Stencil1U = GL_STENCIL_INDEX1,
  Stencil4U = GL_STENCIL_INDEX4,
  Stencil8U = GL_STENCIL_INDEX8,
  Stencil16U = GL_STENCIL_INDEX16,

  /// Combined depth/stencil buffer formats
  Depth24_UNorm_Stencil8_UNorm = GL_DEPTH24_STENCIL8,
  Depth32F_Stencil8_UNorm = GL_DEPTH32F_STENCIL8
};

/**
 * @see https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glTexBuffer.xhtml
 */
enum class SizedInternalBufferTextureFormat : uint32_t
{
  R8_UNorm = GL_R8,
  R16_UNorm = GL_R16,
  R16F = GL_R16F,
  R32F = GL_R32F,
  R8I = GL_R8I,
  R16I = GL_R16I,
  R32I = GL_R32I,
  R8U = GL_R8UI,
  R16U = GL_R16UI,
  R32U = GL_R32UI,
  RG8_UNorm = GL_RG8,
  RG16_UNorm = GL_RG16,
  RG16F = GL_RG16F,
  RG32F = GL_RG32F,
  RG8I = GL_RG8I,
  RG16I = GL_RG16I,
  RG32I = GL_RG32I,
  RG8U = GL_RG8UI,
  RG16U = GL_RG16UI,
  RG32U = GL_RG32UI,
  RGB32F = GL_RGB32F,
  RGB32I = GL_RGB32I,
  RGB32UI = GL_RGB32UI,
  RGBA8_UNorm = GL_RGBA8,
  RGBA16_UNorm = GL_RGBA16,
  RGBA16F = GL_RGBA16F,
  RGBA32F = GL_RGBA32F,
  RGBA8I = GL_RGBA8I,
  RGBA16I = GL_RGBA16I,
  RGBA32I = GL_RGBA32I,
  RGBA8U = GL_RGBA8UI,
  RGBA16U = GL_RGBA16UI,
  RGBA32U = GL_RGBA32UI
};

enum class MinificationFilter : uint32_t
{
  Nearest = GL_NEAREST,
  Linear = GL_LINEAR,
  NearestWithMipmapNearest = GL_NEAREST_MIPMAP_NEAREST,
  NearestWithMipmapLinear = GL_NEAREST_MIPMAP_LINEAR,
  LinearWithMipmapNearest = GL_LINEAR_MIPMAP_NEAREST

  // Note: We have had problems with this interpolating to black slide texture mipmap:
  // LinearWithMipmapLinear = GL_LINEAR_MIPMAP_LINEAR
};

enum class MagnificationFilter : uint32_t
{
  Nearest = GL_NEAREST,
  Linear = GL_LINEAR
};

enum class SwizzleValue : uint32_t
{
  Red = GL_RED,
  Green = GL_GREEN,
  Blue = GL_BLUE,
  Alpha = GL_ALPHA,
  Zero = GL_ZERO,
  One = GL_ONE
};

enum class WrapMode : uint32_t
{
  ClampToBorder = GL_CLAMP_TO_BORDER,
  ClampToEdge = GL_CLAMP_TO_EDGE,
  Repeat = GL_REPEAT,
  MirroredRepeat = GL_MIRRORED_REPEAT
};

enum class SamplingDirection : uint32_t
{
  X = GL_TEXTURE_WRAP_S,
  Y = GL_TEXTURE_WRAP_T,
  Z = GL_TEXTURE_WRAP_R
};

} // namespace tex

#endif // GL_TEXTURE_TYPES_H
