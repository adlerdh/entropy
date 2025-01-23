#ifndef CAMERA_LABEL_ASSEMBLY_H
#define CAMERA_LABEL_ASSEMBLY_H

#include "rendering/common/ShaderProviderType.h"
#include "rendering_old/interfaces/IDrawableAssembly.h"

#include "common/ObjectCounter.hpp"
#include "common/PublicTypes.h"

#include <glm/fwd.hpp>

#include <memory>
#include <optional>

class CameraLabel;
class GLTexture;

class CameraLabelAssembly final : public IDrawableAssembly,
                                  public ObjectCounter<CameraLabelAssembly>
{
public:
  explicit CameraLabelAssembly(
    ShaderProgramActivatorType,
    UniformsProviderType,
    GetterType<glm::mat4> activeSubjectToWorldProvider
  );

  ~CameraLabelAssembly() override = default;

  void initialize() override;

  std::weak_ptr<DrawableBase> getRoot(const SceneType&) override;

  void setActiveSubjectToWorldProvider(GetterType<std::optional<glm::mat4> >);

private:
  static const std::string smk_lettersImagePath;
  static const std::array<std::string, 6> smk_labels;

  ShaderProgramActivatorType m_shaderActivator;
  UniformsProviderType m_uniformsProvider;

  GetterType<std::optional<glm::mat4> > m_activeSubjectToWorldProvider;

  std::shared_ptr<CameraLabel> m_root;

  std::array<std::shared_ptr<GLTexture>, 6> m_letterTextures;
};

#endif // CAMERA_LABEL_ASSEMBLY_H
