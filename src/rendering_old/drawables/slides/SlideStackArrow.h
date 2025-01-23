#ifndef SLIDE_STACK_ARROW_H
#define SLIDE_STACK_ARROW_H

#include "rendering/common/ShaderProviderType.h"
#include "rendering/drawables/DrawableBase.h"

#include "common/ObjectCounter.hpp"
#include "common/PublicTypes.h"

#include <memory>

class BasicMesh;
class MeshGpuRecord;
class Transformation;

class SlideStackArrow : public DrawableBase, public ObjectCounter<SlideStackArrow>
{
public:
  SlideStackArrow(
    std::string name,
    ShaderProgramActivatorType shaderProgramActivator,
    UniformsProviderType uniformsProvider,
    GetterType<float> slideStackHeightProvider,
    std::weak_ptr<MeshGpuRecord> coneMeshRecord,
    std::weak_ptr<MeshGpuRecord> cylinderMeshRecord,
    std::weak_ptr<MeshGpuRecord> sphereMeshRecord,
    bool isFixedRadius = false
  );

  ~SlideStackArrow() override = default;

  void setSlideStackHeightProvider(GetterType<float> slideStackHeightProvider);
  void setRadius(float radius);

private:
  void setupChildren();

  void doUpdate(double time, const Viewport&, const camera::Camera&, const CoordinateFrame&) override;

  GetterType<float> m_slideStackHeightProvider;

  float m_cylinderRadius;
  bool m_isFixedRadius;

  std::shared_ptr<Transformation> m_coneTx;
  std::shared_ptr<Transformation> m_cylinderTx;
  std::shared_ptr<Transformation> m_sphereTx;

  std::shared_ptr<BasicMesh> m_cone;
  std::shared_ptr<BasicMesh> m_cylinder;
  std::shared_ptr<BasicMesh> m_sphere;
};

#endif // SLIDE_STACK_ARROW_H
