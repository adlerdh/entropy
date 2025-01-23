#ifndef CROSSHAIRS_3D_DRAWABLE_H
#define CROSSHAIRS_3D_DRAWABLE_H

#include "common/ObjectCounter.hpp"
#include "rendering/common/ShaderProviderType.h"
#include "rendering/drawables/DrawableBase.h"

#include <array>
#include <memory>

class BasicMesh;
class MeshGpuRecord;
class Transformation;

class Crosshairs : public DrawableBase, public ObjectCounter<Crosshairs>
{
public:
  Crosshairs(
    std::string name,
    ShaderProgramActivatorType shaderProgramActivator,
    UniformsProviderType uniformsProvider,
    std::weak_ptr<MeshGpuRecord> meshGpuRecord,
    bool isFixedDiameter = false
  );

  ~Crosshairs() override = default;

  void setLength(float length);

  DrawableOpacity opacityFlag() const override;

private:
  void setupChildren();

  void doUpdate(double time, const Viewport&, const camera::Camera&, const CoordinateFrame&) override;

  float m_crosshairLength;
  bool m_isFixedDiameter;

  std::array<std::shared_ptr<Transformation>, 3> m_txs;
  std::array<std::shared_ptr<BasicMesh>, 3> m_crosshairs;
};

#endif // CROSSHAIRS_3D_DRAWABLE_H
