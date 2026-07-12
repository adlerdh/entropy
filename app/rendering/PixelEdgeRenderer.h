#pragma once

#include "rendering/geometry/PixelEdgeGeometry.h"
#include "rendering/RenderData.h"
#include "rendering/common/ShaderType.h"
#include "rendering/utility/gl/GLFrameBufferObject.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/gl/GLVertexArrayObject.h"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>

/**
 * @brief Renders screen-space image edges as a post-process over one view.
 *
 * The renderer captures the normal image pass into a reusable framebuffer, runs a
 * pixel-space edge shader, and composites the result back into the view rectangle.
 */
class PixelEdgeRenderer
{
public:
  using ViewRect = entropy::rendering::pixel_edge::ViewRect;

  using DrawImageFn = std::function<void()>;
  using BindPostTexturesFn = std::function<void()>;

  /** @brief Generate GL objects that do not depend on framebuffer size. */
  void init();

  /**
   * @brief Add the pixel-edge post-process shader to the shared shader map.
   * @param programs Rendering-owned shader program map.
   * @throws Debug exception if the shader cannot be loaded, compiled, or linked.
   */
  void registerShaderPrograms(std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>>& programs);

  /**
   * @brief Capture one image pass, run pixel-space edge detection, and composite it.
   * @param shaderPrograms Rendering-owned shader program map.
   * @param defaultViewport Default-framebuffer viewport in device pixels.
   * @param viewRect View rectangle in scene and default-framebuffer coordinates.
   * @param uniforms Image uniforms that control pixel-edge rendering.
   * @param drawImage Callback that renders the image into the capture framebuffer.
   * @param bindPostTextures Callback that binds image resources needed by the post-process shader.
   */
  void render(
    std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>>& shaderPrograms,
    glm::ivec4 defaultViewport,
    const ViewRect& viewRect,
    const RenderData::ImageUniforms& uniforms,
    const DrawImageFn& drawImage,
    const BindPostTexturesFn& bindPostTextures);

private:
  void ensureSceneFboSize(glm::ivec2 deviceSize);

  GLFrameBufferObject m_sceneFbo{"ScenePixelEdgePostFbo"};
  std::optional<GLTexture> m_sceneColorTex;
  glm::ivec2 m_sceneFboSize{0, 0};
  GLVertexArrayObject m_postVao;
};
