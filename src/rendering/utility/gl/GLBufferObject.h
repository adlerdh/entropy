#ifndef GLBUFFEROBJECT_H
#define GLBUFFEROBJECT_H

#include "rendering/utility/gl/GLBufferTypes.h"
#include "rendering/utility/gl/GLErrorChecker.h"
#include "rendering/utility/gl/GLTextureTypes.h"

#include <glad/glad.h>

#include <set>

class GLBufferObject final
{
public:
  /**
     * @param type Specifies the name of the buffer object
     * @param usage Specifies the expected usage pattern of the data store.
     */
  GLBufferObject(const BufferType& type, const BufferUsagePattern& usagePattern);

  GLBufferObject(const GLBufferObject&) = delete;
  GLBufferObject& operator=(const GLBufferObject&) = delete;

  GLBufferObject(GLBufferObject&&) noexcept;
  GLBufferObject& operator=(GLBufferObject&&) noexcept;

  /**
     * @brief Deletes the buffer object, including storage on GPU
     */
  ~GLBufferObject();

  /**
     * @brief Generate buffer object name
     */
  void generate();

  /**
     * @brief Releases the buffer.
     */
  void release();

  /**
     * @brief Destroys the buffer, including all data on GPU.
     */
  void destroy();

  /**
     * @brief Bind the buffer object to the current context
     */
  void bind();

  void unbind();

  /**
     * @brief allocate To create mutable storage for a buffer object, you use this API
     * (reallocates the buffer object's storage)
     *
     * It is assumed that create() has been called on this buffer and that it has been bound to the current context
     *
     * @param sizeInBytes Specifies the size in bytes of the buffer object's new data store
     * (how many bytes you want to allocate in this buffer object)
     *
     * @param data Specifies a pointer to data that will be copied into the data store for initialization,
     * or NULL if no data is to be copied.
     * (pointer to user memory that will be copied into the buffer object's data store.
     * If this value is NULL, then no copying will occur, and the buffer object's data will be undefined)
     *
     * @note This calls bind() to first bind the texture
     */
  void allocate(std::size_t sizeInBytes, const GLvoid* data);

  /**
     * @brief Updates a subset of a buffer object's data store
     *
     * It is assumed that create() has been called on this buffer and that it has been bound to the current context
     *
     * @param offset Specifies the offset into the buffer object's data store where data replacement will begin, measured in bytes
     *
     * @param sizeInBytes Specifies the size in bytes of the data store region being replaced
     *
     * @param data Specifies a pointer to the new data that will be copied into the data store.
     *
     * @note This calls bind()
     */
  void write(std::size_t offset, std::size_t sizeInBytes, const GLvoid* data);

  /**
     * @brief returns a subset of a buffer object's data store.
     * returns some or all of the data from the buffer object currently bound to target​.
     * Data starting at byte offset offset​ and extending for size​ bytes is copied from the
     * data store to the memory pointed to by data​. An error is thrown if the buffer object is currently mapped,
     * or if offset​ and size​ together define a range beyond the bounds of the buffer object's data store
     *
     * @param offset Specifies the offset into the buffer object's data store from which data will be returned, measured in bytes
     *
     * @param sizeInBytes Specifies the size in bytes of the data store region being returned
     *
     * @param data Specifies a pointer to the location where buffer object data is returned
     * @return
     */
  void read(std::size_t offset, std::size_t sizeInBytes, GLvoid* data);

  /**
     * @brief map map all of a buffer object data store into the client's address space
     *
     * QT: Maps the contents of this buffer into the application's memory space and returns a pointer to it.
     * Returns null if memory mapping is not possible.
     * It is assumed that create() has been called on this buffer and that it has been bound to the current context.

     * glMapBuffer maps to the client's address space the entire data store of the buffer object currently bound to target​.
     * The data can then be directly read and/or written relative to the returned pointer, depending on the specified
     * access​ policy. If the GL is unable to map the buffer object's data store, glMapBuffer generates an error and
     * returns NULL
     *
     * @param access indicates the type of access to be performed
     * Specifies a combination of access flags indicating the desired access to the mapped range
     */
  void* map(const BufferMapAccessPolicy& access);

  /**
     * @brief mapRange map all or part of a buffer object's data store into the client's address space
     * @param offset Specifies the starting offset within the buffer of the range to be mapped
     * @param length Specifies the length of the range to be mapped
     * @param accessFlags Specifies a set of access flags indicating the desired access to the mapped range
     * @return
     */
  void* mapRange(
    GLintptr offset, GLsizeiptr length, const std::set<BufferMapRangeAccessFlag>& accessFlags
  );

  /**
     * @brief release the mapping of a buffer object's data store into the client's address space
     * @return
     */
  bool unmap();

  /// @todo Change GLsizeiptr and GLintptr to std::size_t
  void copyData(
    GLBufferObject& readBuffer,
    GLBufferObject& writeBuffer,
    GLintptr readOffset,
    GLintptr writeOffset,
    GLsizeiptr size
  );

  GLuint id() const;
  BufferType type() const;
  BufferUsagePattern usagePattern() const;

  std::size_t size() const;

private:
  GLErrorChecker m_errorChecker;

  GLuint m_id;

  BufferType m_type;
  GLenum m_typeEnum;
  BufferUsagePattern m_usagePattern;

  std::size_t m_bufferSizeInBytes;
};

#endif // GLBUFFEROBJECT_H
