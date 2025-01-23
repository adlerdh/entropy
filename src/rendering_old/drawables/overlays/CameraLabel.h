#ifndef CAMERA_LABEL_H
#define CAMERA_LABEL_H

#include "rendering/common/ShaderProviderType.h"
#include "rendering/drawables/DrawableBase.h"
#include "rendering/utility/gl/GLBufferObject.h"
#include "rendering/utility/gl/GLVertexArrayObject.h"

#include "common/ObjectCounter.hpp"
#include "common/PublicTypes.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <memory>
#include <optional>

class GLTexture;
class MeshGpuRecord;

/**
 * @brief Class that renders a subject's anatomical labels (S, P, I, R, A, L).
 */
class CameraLabel : public DrawableBase, public ObjectCounter<CameraLabel>
{
public:
  /**
     * @brief Construct CameraLabel
     * @param name
     * @param shaderProgramActivator
     * @param uniformsProvider
     * @param subjectToWorldProvider Function providing the transformation from the Subject to World space
     * @param letterTextures Array of pointers to textures for the S, P, I, R, A and L labels (in that order)
     */
  CameraLabel(
    std::string name,
    ShaderProgramActivatorType shaderProgramActivator,
    UniformsProviderType uniformsProvider,
    GetterType<std::optional<glm::mat4> > subjectToWorldProvider,
    std::array<std::weak_ptr<GLTexture>, 6> letterTextures
  );

  CameraLabel(const CameraLabel&) = delete;
  CameraLabel& operator=(const CameraLabel&) = delete;

  CameraLabel(CameraLabel&&) = delete;
  CameraLabel& operator=(CameraLabel&&) = delete;

  ~CameraLabel() override;

  DrawableOpacity opacityFlag() const override;

  void setSubjectToWorldProvider(GetterType<std::optional<glm::mat4> >);

private:
  /// Index of the texture sampler for the Simple shader
  static const Uniforms::SamplerIndexType TexSamplerIndex;

  /// Shortcuts for the six anatomical directions
  static constexpr int S = 0;
  static constexpr int P = 1;
  static constexpr int I = 2;
  static constexpr int R = 3;
  static constexpr int A = 4;
  static constexpr int L = 5;

  /// Label data structure
  struct LabelData
  {
    std::weak_ptr<GLTexture> m_texture; //!< Label texture
    glm::mat4 m_world_O_model{1.0f};    //!< Transformation from label quad to world space
    glm::vec3 m_solidColor{0.0f};       //!< Label color (currently not used)
    bool m_visible = true; //!< Visibility flag for label: only visible labels are rendered
  };

  void doRender(const RenderStage&) override;
  void doUpdate(double time, const Viewport&, const camera::Camera&, const CoordinateFrame&) override;

  void initBuffer();
  void initVao();

  ShaderProgramActivatorType m_shaderProgramActivator;
  UniformsProviderType m_uniformsProvider;

  GetterType<std::optional<glm::mat4> > m_subjectToWorldProvider;

  GLVertexArrayObject m_vao;

  std::unique_ptr<GLVertexArrayObject::IndexedDrawParams> m_vaoParams;

  /// Quad mesh that is textured by the letter
  std::unique_ptr<MeshGpuRecord> m_meshGpuRecord;

  Uniforms m_uniforms;

  /// Textures of the letters in order {S,P,I,R,A,L}
  std::array<LabelData, 6> m_labels;

  /// Fixed orthogonal transformation for renering the labels
  const glm::mat4 m_clip_O_camera;
};

#endif // CAMERA_LABEL_H
