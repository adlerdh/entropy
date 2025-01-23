#ifndef LINE_H
#define LINE_H

#include "rendering/common/ShaderProviderType.h"
#include "rendering/drawables/DrawableBase.h"
#include "rendering/utility/containers/VertexAttributeInfo.h"
#include "rendering/utility/containers/VertexIndicesInfo.h"
#include "rendering/utility/gl/GLBufferObject.h"
#include "rendering/utility/gl/GLVertexArrayObject.h"

#include "common/ObjectCounter.hpp"

#include <memory>

class Line : public DrawableBase, public ObjectCounter<Line>
{
public:
  Line(
    const std::string& name,
    ShaderProgramActivatorType shaderProgramActivator,
    UniformsProviderType uniformsProvider,
    const PrimitiveMode& primitiveMode
  );

  Line(const Line&) = delete;
  Line& operator=(const Line&) = delete;

  ~Line() override = default;

  void setColor(const glm::vec3& color);

  // clears existing buffer if size changes
  void setVertices(const float* vertexBuffer, size_t numVertices);

  DrawableOpacity opacityFlag() const override;

private:
  void doRender(const RenderStage& stage) override;

  void doUpdate(double time, const Viewport&, const camera::Camera&, const CoordinateFrame&) override;

  void initBuffers();
  void initVaos();

  void generateBuffers(const float* vertexBuffer);
  void fillPositionsBuffer(const float* vertexBuffer);

  ShaderProgramActivatorType m_shaderProgramActivator;
  UniformsProviderType m_uniformsProvider;

  Uniforms m_stdUniforms;
  Uniforms m_peelUniforms;
  Uniforms m_initUniforms;

  GLVertexArrayObject m_vao;

  std::unique_ptr<GLVertexArrayObject::IndexedDrawParams> m_vaoParams;

  GLBufferObject m_positionBuffer;
  GLBufferObject m_indexBuffer;

  size_t m_numVertices;
  VertexAttributeInfo m_positionInfo;
  VertexIndicesInfo m_indexInfo;

  glm::mat4 m_clip_O_camera;
  glm::mat4 m_camera_O_world;

  glm::vec3 m_solidColor;
};

#endif // LINE_H
