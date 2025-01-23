#ifndef GLTEXTURE_H
#define GLTEXTURE_H

#include "rendering/utility/gl/GLErrorChecker.h"
#include "rendering/utility/gl/GLTextureTypes.h"

#include "common/Types.h"

#include <glm/fwd.hpp>
#include <glm/vec3.hpp>

#include <glad/glad.h>

#include <optional>
#include <unordered_map>

class GLTexture final
{
public:
  struct MultisampleSettings
  {
    MultisampleSettings()
      : m_numSamples(1)
      , m_fixedSampleLocations(false)
    {
    }

    MultisampleSettings(GLsizei numSamples, GLboolean fixedSampleLocation)
      : m_numSamples(numSamples)
      , m_fixedSampleLocations(fixedSampleLocation)
    {
    }

    GLsizei m_numSamples;
    GLboolean m_fixedSampleLocations;
  };

  struct PixelStoreSettings
  {
    PixelStoreSettings()
      : m_alignment(4)
      , m_skipImages(0)
      , m_skipRows(0)
      , m_skipPixels(0)
      , m_imageHeight(0)
      , m_rowLength(0)
      , m_lsbFirst(false)
      , m_swapBytes(false)
    {
    }

    PixelStoreSettings(
      GLint alignment,
      GLint skipImages,
      GLint skipRows,
      GLint skipPixels,
      GLint imageHeight,
      GLint rowLength,
      GLboolean lsbFirst,
      GLboolean swapBytes
    )
      : m_alignment(alignment)
      , m_skipImages(skipImages)
      , m_skipRows(skipRows)
      , m_skipPixels(skipPixels)
      , m_imageHeight(imageHeight)
      , m_rowLength(rowLength)
      , m_lsbFirst(lsbFirst)
      , m_swapBytes(swapBytes)
    {
    }

    /// Specifies the alignment requirements for the start of each pixel row in memory.
    /// The allowable values are
    /// 1 (byte-alignment),
    /// 2 (rows aligned to even-numbered bytes),
    /// 4 (word-alignment), and
    /// 8 (rows start on double-word boundaries).
    GLint m_alignment = 4;

    /// Setting to k is equivalent to incrementing the pointer by k*L components or indices,
    /// where L is the number of components or indices per image.
    GLint m_skipImages = 0;

    /// Setting to j is equivalent to incrementing the pointer by j*M components or indices,
    /// where M is the number of components or indices per row.
    GLint m_skipRows = 0;

    /// Setting to i is equivalent to incrementing the pointer by i*N components or indices,
    /// where N is the number of components or indices in each pixel.
    GLint m_skipPixels = 0;

    /// If greater than 0, defines the number of pixels in an image of a
    /// three-dimensional texture volume.
    GLint m_imageHeight = 0;

    /// If greater than 0, defines the number of pixels in a row.
    GLint m_rowLength = 0;

    /// If true, bits are ordered within a byte from least significant to most significant;
    /// otherwise, the first bit in each byte is the most significant one.
    GLboolean m_lsbFirst = false;

    /// If true, byte ordering for multibyte color components, depth components,
    /// or stencil indices is reversed. That is, if a four-byte component consists of
    /// bytes b0b0, b1b1, b2b2, b3b3, it is stored in memory as b3b3, b2b2, b1b1, b0b0
    /// if true. This has no effect on the memory order of components within a pixel,
    /// only on the order of bytes within components or indices.
    GLboolean m_swapBytes = false;
  };

  GLTexture(
    tex::Target target,
    MultisampleSettings multisampleSettings = MultisampleSettings(),
    std::optional<PixelStoreSettings> pixelPackSettings = std::nullopt,
    std::optional<PixelStoreSettings> pixelUnpackSettings = std::nullopt
  );

  GLTexture(const GLTexture&) = delete;
  GLTexture(GLTexture&&) noexcept;

  GLTexture& operator=(const GLTexture&) = delete;
  GLTexture& operator=(GLTexture&&) noexcept;

  ~GLTexture();

  void generate();
  void release(std::optional<uint32_t> textureUnit = std::nullopt);
  void bind(std::optional<uint32_t> textureUnit = std::nullopt);
  bool isBound(std::optional<uint32_t> textureUnit = std::nullopt);
  void unbind();

  // Bind/bind sampler object to/from a texture unit
  /// @todo Place Sampler Object in separate class.
  /// @todo Store Sampler Object as member of GPU image record
  void bindSampler(uint32_t textureUnit);
  void unbindSampler(uint32_t textureUnit);

  tex::Target target() const;

  GLuint id() const;

  glm::uvec3 size() const;

  void setSize(const glm::uvec3& size);

  /**
     * @brief Allocates mutable storage for a mipmap level of the bound texture object and
     * optionally writes pixel data to that mipmap level.
     **/
  void setData(
    GLint level,
    const tex::SizedInternalFormat& internalFormat,
    const tex::BufferPixelFormat& format,
    const tex::BufferPixelDataType& type,
    const GLvoid* data
  );

  /**
     * @brief Writes the user's pixel data to some part of the given mipmap of the bound texture object.
     **/
  void setSubData(
    GLint level,
    const glm::uvec3& offset,
    const glm::uvec3& size,
    const tex::BufferPixelFormat& format,
    const tex::BufferPixelDataType& type,
    const GLvoid* data
  );

  void setCubeMapFaceData(
    const tex::CubeMapFace& face,
    GLint level,
    const tex::SizedInternalFormat& internalFormat,
    const tex::BufferPixelFormat& format,
    const tex::BufferPixelDataType& type,
    const GLvoid* data
  );

  /**
     * @remark If the selected texture image does not contain four components, the following mappings
     * are applied. Single-component textures are treated as RGBA buffers with red set to the
     * single-component value, green set to 0, blue set to 0, and alpha set to 1.
     * Two-component textures are treated as RGBA buffers with red set to the value of component zero,
     * alpha set to the value of component one, and green and blue set to 0. Finally, three-component
     * textures are treated as RGBA buffers with red set to component zero, green set to component one,
     * blue set to component two, and alpha set to 1.
     */
  void readData(
    GLint level,
    const tex::BufferPixelFormat& format,
    const tex::BufferPixelDataType& type,
    GLvoid* data
  );

  void readCubeMapFaceData(
    const tex::CubeMapFace& face,
    GLint level,
    const tex::BufferPixelFormat& format,
    const tex::BufferPixelDataType& type,
    GLvoid* data
  );

  void setMinificationFilter(const tex::MinificationFilter& filter);
  void setMagnificationFilter(const tex::MagnificationFilter& filter);

  void setSwizzleMask(
    const tex::SwizzleValue& rValue,
    const tex::SwizzleValue& gValue,
    const tex::SwizzleValue& bValue,
    const tex::SwizzleValue& aValue
  );

  void setWrapMode(const tex::WrapMode& mode);

  void setBorderColor(const glm::vec4& color);

  void setAutoGenerateMipmaps(bool set);

  void setMultisampleSettings(const MultisampleSettings& settings);

  void setPixelPackSettings(const PixelStoreSettings& settings);
  void setPixelUnpackSettings(const PixelStoreSettings& settings);

  // Sized internal normalized formats:

  static tex::SizedInternalFormat getSizedInternalNormalizedRedFormat(
    const ComponentType& componentType
  );

  static tex::SizedInternalFormat getSizedInternalNormalizedRGFormat(
    const ComponentType& componentType
  );

  static tex::SizedInternalFormat getSizedInternalNormalizedRGBFormat(
    const ComponentType& componentType
  );

  static tex::SizedInternalFormat getSizedInternalNormalizedRGBAFormat(
    const ComponentType& componentType
  );

  // Sized internal non-normalized formats:

  static tex::SizedInternalFormat getSizedInternalRedFormat(const ComponentType& componentType);

  static tex::SizedInternalFormat getSizedInternalRGFormat(const ComponentType& componentType);

  static tex::SizedInternalFormat getSizedInternalRGBFormat(const ComponentType& componentType);

  static tex::SizedInternalFormat getSizedInternalRGBAFormat(const ComponentType& componentType);

  // Normalized buffer pixel formats:

  static tex::BufferPixelFormat getBufferPixelNormalizedRedFormat(const ComponentType& componentType
  );

  static tex::BufferPixelFormat getBufferPixelNormalizedRGFormat(const ComponentType& componentType);

  static tex::BufferPixelFormat getBufferPixelNormalizedRGBFormat(const ComponentType& componentType
  );

  static tex::BufferPixelFormat getBufferPixelNormalizedRGBAFormat(const ComponentType& componentType
  );

  // Non-normalized buffer pixel formats:

  static tex::BufferPixelFormat getBufferPixelRedFormat(const ComponentType& componentType);

  static tex::BufferPixelFormat getBufferPixelRGFormat(const ComponentType& componentType);

  static tex::BufferPixelFormat getBufferPixelRGBFormat(const ComponentType& componentType);

  static tex::BufferPixelFormat getBufferPixelRGBAFormat(const ComponentType& componentType);

  // Buffer pixel data type:

  static tex::BufferPixelDataType getBufferPixelDataType(const ComponentType& componentType);

private:
  static const std::unordered_map<tex::Target, tex::Binding> s_bindingMap;

  // Sized internal normalized formats:
  static const std::unordered_map<ComponentType, tex::SizedInternalFormat>
    s_componentTypeToSizedInternalNormalizedRedFormatMap;

  static const std::unordered_map<ComponentType, tex::SizedInternalFormat>
    s_componentTypeToSizedInternalNormalizedRGFormatMap;

  static const std::unordered_map<ComponentType, tex::SizedInternalFormat>
    s_componentTypeToSizedInternalNormalizedRGBFormatMap;

  static const std::unordered_map<ComponentType, tex::SizedInternalFormat>
    s_componentTypeToSizedInternalNormalizedRGBAFormatMap;

  // Sized internal non-normalized formats:
  static const std::unordered_map<ComponentType, tex::SizedInternalFormat>
    s_componentTypeToSizedInternalRedFormatMap;

  static const std::unordered_map<ComponentType, tex::SizedInternalFormat>
    s_componentTypeToSizedInternalRGFormatMap;

  static const std::unordered_map<ComponentType, tex::SizedInternalFormat>
    s_componentTypeToSizedInternalRGBFormatMap;

  static const std::unordered_map<ComponentType, tex::SizedInternalFormat>
    s_componentTypeToSizedInternalRGBAFormatMap;

  // Normalized buffer pixel formats:
  static const std::unordered_map<ComponentType, tex::BufferPixelFormat>
    s_componentTypeToBufferPixelRedNormalizedFormatMap;

  static const std::unordered_map<ComponentType, tex::BufferPixelFormat>
    s_componentTypeToBufferPixelRGNormalizedFormatMap;

  static const std::unordered_map<ComponentType, tex::BufferPixelFormat>
    s_componentTypeToBufferPixelRGBNormalizedFormatMap;

  static const std::unordered_map<ComponentType, tex::BufferPixelFormat>
    s_componentTypeToBufferPixelRGBANormalizedFormatMap;

  // Non-normalized buffer pixel formats:
  static const std::unordered_map<ComponentType, tex::BufferPixelFormat>
    s_componentTypeToBufferPixelRedFormatMap;

  static const std::unordered_map<ComponentType, tex::BufferPixelFormat>
    s_componentTypeToBufferPixelRGFormatMap;

  static const std::unordered_map<ComponentType, tex::BufferPixelFormat>
    s_componentTypeToBufferPixelRGBFormatMap;

  static const std::unordered_map<ComponentType, tex::BufferPixelFormat>
    s_componentTypeToBufferPixelRGBAFormatMap;

  // Buffer pixel data type:
  static const std::unordered_map<ComponentType, tex::BufferPixelDataType>
    s_componentTypeToBufferPixelDataTypeMap;

  GLErrorChecker m_errorChecker;

  const tex::Target m_target;
  const GLenum m_targetEnum;
  GLuint m_id;
  glm::uvec3 m_size{0u};
  bool m_autoGenerateMipmaps = false;

  GLuint m_samplerID = 0u;

  MultisampleSettings m_multisampleSettings;
  std::optional<PixelStoreSettings> m_pixelPackSettings;
  std::optional<PixelStoreSettings> m_pixelUnpackSettings;

  class Binder
  {
  public:
    Binder(GLTexture& tex);
    ~Binder();

  private:
    GLTexture& m_texture;
    GLint m_boundID;
  };

  PixelStoreSettings getPixelPackSettings();
  PixelStoreSettings getPixelUnpackSettings();

  void applyPixelPackSettings(const PixelStoreSettings& settings);
  void applyPixelUnpackSettings(const PixelStoreSettings& settings);
};

#endif // GLTEXTURE_H
