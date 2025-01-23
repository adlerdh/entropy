#ifndef GL_FBO_H
#define GL_FBO_H

//#include "rendering/utility/gl/GLErrorChecker.h"
#include "rendering/utility/gl/GLFBOAttachmentTypes.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/gl/GLTextureTypes.h"

#include <glad/glad.h>

#include <optional>
#include <string>

class GLFrameBufferObject final
{
public:
  /**
     * @brief GLFrameBufferObject
     */
  explicit GLFrameBufferObject(const std::string& name);

  GLFrameBufferObject(const GLFrameBufferObject&) = delete;
  GLFrameBufferObject& operator=(const GLFrameBufferObject&) = delete;

  GLFrameBufferObject(GLFrameBufferObject&&) noexcept;
  GLFrameBufferObject& operator=(GLFrameBufferObject&&) noexcept;

  /**
     * @brief Deletes the FBO, including storage on GPU
     */
  ~GLFrameBufferObject();

  /**
     * @brief Generate FBO name
     */
  void generate();

  /**
     * @brief Destroys the FBO, including all data on GPU
     */
  void destroy();

  /**
     * @brief Bind the FBO to the current context
     */
  void bind(const fbo::TargetType& target);

  void attach2DTexture(
    const fbo::TargetType& target,
    const fbo::AttachmentType& attachment,
    const GLTexture& tex,
    std::optional<int> colorAttachmentIndex = std::nullopt
  );

  void attachCubeMapTexture(
    const fbo::TargetType& target,
    const fbo::AttachmentType& attachment,
    const GLTexture& tex,
    const tex::CubeMapFace& cubeMapFace,
    GLint level,
    std::optional<int> colorAttachmentIndex = std::nullopt
  );

  GLuint id() const;

private:
  void checkStatus();

  //    GLErrorChecker m_errorChecker;

  std::string m_name;
  GLuint m_id;
};

#endif // GL_FBO_H
