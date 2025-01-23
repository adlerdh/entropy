#ifndef DUAL_DEPTH_PEEL_RENDERER_H
#define DUAL_DEPTH_PEEL_RENDERER_H

#include "rendering/common/ShaderProviderType.h"
#include "rendering/interfaces/IRenderer.h"

#include <glm/fwd.hpp>

#include <memory>
#include <string>

class IDrawable;

class DepthPeelRenderer : public IRenderer
{
public:
  /**
     * @brief DepthPeelRenderer
     *
     * @param[in] name
     * @param[in] shaderProgramActivator
     * @param[in] uniformsProvider
     *
     * @param[in] rootProvider Function returning the root IDrawable of the scene to be rendered.
     * The scene must be managed externally to this class.
     *
     * @param[in] overlayProvider Function returning the root IDrawable of the overlay to be rendered.
     * The overlay must be managed externally to this class.
     */
  DepthPeelRenderer(
    const std::string& name,
    ShaderProgramActivatorType shaderProgramActivator,
    UniformsProviderType uniformsProvider,
    GetterType<IDrawable*> sceneRootProvider,
    GetterType<IDrawable*> overlayRootProvider
  );

  DepthPeelRenderer(const DepthPeelRenderer&) = delete;
  DepthPeelRenderer& operator=(const DepthPeelRenderer&) = delete;

  DepthPeelRenderer(DepthPeelRenderer&&) = default;
  DepthPeelRenderer& operator=(DepthPeelRenderer&&) = default;

  ~DepthPeelRenderer() override;

  void initialize() override;
  void render() override;
  void teardown() override;
  void resize(const Viewport&) override;
  void update(const camera::Camera&, const CoordinateFrame&) override;

  void setEnablePointPicking(bool enable) override;
  std::pair<uint16_t, float> pickObjectIdAndNdcDepth(const glm::vec2& ndcPos) override;

  void setMaxNumberOfPeels(uint32_t num);
  void setOcclusionRatio(float ratio);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

#endif // DUAL_DEPTH_PEEL_RENDERER_H
