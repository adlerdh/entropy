#pragma once

#include <glad/glad.h>

#include <cstdint>

/**
 * @brief OpenGL buffer targets used by the renderer.
 */
enum class BufferType : uint32_t
{
  CopyRead = GL_COPY_READ_BUFFER,                   //!< Source buffer for GL-to-GL copy operations
  CopyWrite = GL_COPY_WRITE_BUFFER,                 //!< Destination buffer for GL-to-GL copy operations
  DrawIndirect = GL_DRAW_INDIRECT_BUFFER,           //!< Buffer containing indirect draw commands
  Index = GL_ELEMENT_ARRAY_BUFFER,                  //!< Element/index buffer for indexed draws
  PixelPack = GL_PIXEL_PACK_BUFFER,                 //!< Destination buffer for pixel readback
  PixelUnpack = GL_PIXEL_UNPACK_BUFFER,             //!< Source buffer for pixel uploads
  Texture = GL_TEXTURE_BUFFER,                      //!< Storage backing a buffer texture
  TransformFeedback = GL_TRANSFORM_FEEDBACK_BUFFER, //!< Transform feedback capture buffer
  Uniform = GL_UNIFORM_BUFFER,                      //!< Uniform block storage buffer
  VertexArray = GL_ARRAY_BUFFER                     //!< Vertex attribute data buffer

  // Not supported in GL 3.3:
  // GL_ATOMIC_COUNTER_BUFFER
  // GL_DISPATCH_INDIRECT_BUFFER
  // GL_QUERY_BUFFER
  // GL_SHADER_STORAGE_BUFFER
};

/**
 * @brief OpenGL buffer storage usage hints.
 *
 * `Stream`, `Static`, and `Dynamic` describe expected update frequency. `Draw`, `Read`, and `Copy` describe whether
 * data is mainly written by the CPU, read back to the CPU, or copied between GL objects.
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

/**
 * @brief Access mode for mapping an entire buffer object.
 */
enum class BufferMapAccessPolicy : uint32_t
{
  ReadOnly = GL_READ_ONLY,   //!< CPU reads mapped buffer storage
  WriteOnly = GL_WRITE_ONLY, //!< CPU writes mapped buffer storage
  ReadWrite = GL_READ_WRITE  //!< CPU reads and writes mapped buffer storage
};

/**
 * @brief Access flags for mapping a subrange of a buffer object.
 */
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

/**
 * @brief Scalar component types accepted by vertex attribute buffers.
 */
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

/**
 * @brief Whether fixed-point vertex attribute values should be normalized on shader fetch.
 */
enum class BufferNormalizeValues : bool
{
  True = true,  //!< Normalize fixed-point attribute values
  False = false //!< Preserve raw fixed-point attribute values
};
