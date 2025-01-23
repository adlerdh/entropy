#ifndef GLBUFFERTYPES_H
#define GLBUFFERTYPES_H

#include <glad/glad.h>

#include <cstdint>

enum class BufferType : uint32_t
{
  CopyRead = GL_COPY_READ_BUFFER,
  CopyWrite = GL_COPY_WRITE_BUFFER,
  DrawIndirect = GL_DRAW_INDIRECT_BUFFER,
  Index = GL_ELEMENT_ARRAY_BUFFER,
  PixelPack = GL_PIXEL_PACK_BUFFER,
  PixelUnpack = GL_PIXEL_UNPACK_BUFFER,
  Texture = GL_TEXTURE_BUFFER,
  TransformFeedback = GL_TRANSFORM_FEEDBACK_BUFFER,
  Uniform = GL_UNIFORM_BUFFER,
  VertexArray = GL_ARRAY_BUFFER

  // Not supported in GL 3.3:
  // GL_ATOMIC_COUNTER_BUFFER
  // GL_DISPATCH_INDIRECT_BUFFER
  // GL_QUERY_BUFFER
  // GL_SHADER_STORAGE_BUFFER
};

/*
The frequency of access may be one of these:
STREAM The data store contents will be modified once and used at most a few times.
STATIC The data store contents will be modified once and used many times.
DYNAMIC The data store contents will be modified repeatedly and used many times.

The nature of access may be one of these:
DRAW The data store contents are modified by the application, and used as the source for GL drawing and image specification commands.
READ The data store contents are modified by reading data from the GL, and used to return that data when queried by the application.
COPY The data store contents are modified by reading data from the GL, and used as the source for GL drawing and image specification commands.
*/

enum class BufferUsagePattern : uint32_t
{
  DynamicDraw = GL_DYNAMIC_DRAW,
  DynamicRead = GL_DYNAMIC_READ,
  DynamicCopy = GL_DYNAMIC_COPY,
  StaticDraw = GL_STATIC_DRAW,
  StaticRead = GL_STATIC_READ,
  StaticCopy = GL_STATIC_COPY,
  StreamDraw = GL_STREAM_DRAW,
  StreamRead = GL_STREAM_READ,
  StreamCopy = GL_STREAM_COPY
};

enum class BufferMapAccessPolicy : uint32_t
{
  ReadOnly = GL_READ_ONLY,
  WriteOnly = GL_WRITE_ONLY,
  ReadWrite = GL_READ_WRITE
};

enum class BufferMapRangeAccessFlag : uint32_t
{
  MapReadBit = GL_MAP_READ_BIT,
  MapWriteBit = GL_MAP_WRITE_BIT,
  InvalidateRangeBit = GL_MAP_INVALIDATE_RANGE_BIT,
  InvalidateBufferBit = GL_MAP_INVALIDATE_BUFFER_BIT,
  FlushExplicitBit = GL_MAP_FLUSH_EXPLICIT_BIT,
  UnsynchronizedBit = GL_MAP_UNSYNCHRONIZED_BIT

  // Not supported in GL 3.3:
  // MapPersistentBit = GL_MAP_PERSISTENT_BIT,
  // MapCoherentBit = GL_MAP_COHERENT_BIT,
};

enum class BufferComponentType : uint32_t
{
  Byte = GL_BYTE,
  UByte = GL_UNSIGNED_BYTE,
  Short = GL_SHORT,
  UShort = GL_UNSIGNED_SHORT,
  Int = GL_INT,
  UInt = GL_UNSIGNED_INT,
  HFloat = GL_HALF_FLOAT,
  Float = GL_FLOAT,
  Double = GL_DOUBLE,
  Int_2_10_10_10 = GL_INT_2_10_10_10_REV,
  UInt_2_10_10_10 = GL_UNSIGNED_INT_2_10_10_10_REV,
  UInt_10F_11F_11F = GL_UNSIGNED_INT_10F_11F_11F_REV

  // Not supported in GL 3.3:
  // Fixed = GL_FIXED
};

enum class BufferNormalizeValues : bool
{
  True = true,
  False = false
};

#endif // GLBUFFERTYPES_H
