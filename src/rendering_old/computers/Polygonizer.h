#ifndef POLYGONIZER_H
#define POLYGONIZER_H

#include "rendering/common/ShaderProviderType.h"
#include "rendering/utility/containers/Uniforms.h"
#include "rendering/utility/gl/GLErrorChecker.h"
#include "rendering/utility/gl/GLVertexArrayObject.h"
#include "rendering_old/computers/ComputerBase.h"

#include <glm/fwd.hpp>

#include <memory>
#include <string>

class GLBufferObject;
class GLBufferTexture;
class GLTexture;
class VertexAttributeInfo;
class VertexIndicesInfo;

class Polygonizer : public ComputerBase
{
public:
  Polygonizer(
    ShaderProgramActivatorType shaderProgramActivator, UniformsProviderType uniformsProvider
  );

  Polygonizer(const Polygonizer&) = delete;
  Polygonizer& operator=(const Polygonizer&) = delete;

  ~Polygonizer() override;

  void initialize() override;
  void execute() override;

  void setVolumeTexture(std::weak_ptr<GLTexture> texture);
  void setIsoValue(float value);

private:
  // Types used for vertices and vertex indices in VBO
  using PositionType = glm::vec3;
  using PositionIndexType = uint32_t;

  void createTriangleTableTexture();

  GLErrorChecker m_errorChecker;

  std::string m_name;

  ShaderProgramActivatorType m_shaderProgramActivator;
  UniformsProviderType m_uniformsProvider;

  GLVertexArrayObject m_vao;
  std::unique_ptr<GLVertexArrayObject::IndexedDrawParams> m_vaoParams;

  std::unique_ptr<VertexIndicesInfo> m_indicesInfo;
  std::unique_ptr<VertexAttributeInfo> m_positionsInfo;

  std::unique_ptr<GLBufferObject> m_indicesObject;
  std::unique_ptr<GLBufferObject> m_positionsObject;
  std::unique_ptr<GLBufferObject> m_txFeedbackObject;

  Uniforms m_uniforms;

  // Scalar volume to polygonize
  //    std::unique_ptr<GLTexture> m_scalarVolumeTex;
  std::weak_ptr<GLTexture> m_volumeTexture;

  float m_isoValue;

  // Marching Cubes triangle table
  std::unique_ptr<GLTexture> m_triTableBufferTex;

  // We originally tried to use a buffer texture to hold the triangle table.
  // Due to an apparent bug in our graphics driver, the buffer texture did not
  // work with transform feedback.
  //std::unique_ptr<GLBufferTexture> m_triTableBufferTex;

  // Marching Cube indices and corner positions
  std::vector<PositionIndexType> m_cubeIndices;
  std::vector<PositionType> m_cubeCorners;

  // Triangle buffer (holding interleaved vertices and normal vectors)
  // read back from the GL pipeline following the Geometry Shader stage.
  std::vector<glm::vec3> m_feedbackTriangles;
};

#endif // POLYGONIZER_H
