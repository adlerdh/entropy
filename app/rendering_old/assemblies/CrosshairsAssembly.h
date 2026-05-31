#ifndef CROSSHAIRS_ASSEMBLY_H
#define CROSSHAIRS_ASSEMBLY_H

#include "rendering/common/ShaderProviderType.h"
#include "rendering_old/interfaces/IDrawableAssembly.h"

#include "common/ObjectCounter.hpp"

#include <memory>

class Crosshairs;
class MeshGpuRecord;

class CrosshairsAssembly final : public IDrawableAssembly, public ObjectCounter<CrosshairsAssembly>
{
public:
  explicit CrosshairsAssembly(ShaderProgramActivatorType, UniformsProviderType);

  ~CrosshairsAssembly() override = default;

  void initialize() override;

  std::weak_ptr<DrawableBase> getRoot(const SceneType&) override;

  void setCrosshairs2dLength(float worldLength);
  void setCrosshairs3dLength(float worldLength);

private:
  ShaderProgramActivatorType m_shaderActivator;
  UniformsProviderType m_uniformsProvider;

  std::shared_ptr<Crosshairs> m_crosshairs2d;
  std::shared_ptr<MeshGpuRecord> m_meshGpuRecord2d;

  std::shared_ptr<Crosshairs> m_crosshairs3d;
  std::shared_ptr<MeshGpuRecord> m_meshGpuRecord3d;
};

#endif // CROSSHAIRS_ASSEMBLY_H
