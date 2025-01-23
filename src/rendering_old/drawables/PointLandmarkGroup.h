#ifndef DRAWABLE_POINT_LANDMARK_GROUP_H
#define DRAWABLE_POINT_LANDMARK_GROUP_H

#include "rendering/common/ShaderProviderType.h"
#include "rendering/drawables/DrawableBase.h"

#include "logic/records/MeshRecord.h"
#include "logic_old/records/LandmarkGroupRecord.h"

#include "common/ObjectCounter.hpp"
#include "common/PublicTypes.h"

#include <memory>

class BasicMesh;
class Transformation;

class PointLandmarkGroup : public DrawableBase, public ObjectCounter<PointLandmarkGroup>
{
public:
  PointLandmarkGroup(
    std::string name,
    ShaderProgramActivatorType shaderProgramActivator,
    UniformsProviderType uniformsProvider,
    std::weak_ptr<MeshRecord> sphereMeshRecord,
    std::weak_ptr<LandmarkGroupRecord> landmarkGroupRecord,
    bool isFixedRadius = false
  );

  ~PointLandmarkGroup() override;

  void setRadius(float radius);

  /**
     * @brief Set material color as NON-premultiplied RGB
     * @param color RGB (non-premultiplied)
     */
  void setMaterialColor(const glm::vec3& color);

private:
  void doRender(const RenderStage& stage) override;

  void doUpdate(double time, const Viewport&, const camera::Camera&, const CoordinateFrame&) override;

  void setupSphere();

  float m_radius;
  bool m_isFixedRadius;

  glm::mat4 m_scaleTx;

  std::unique_ptr<BasicMesh> m_sphere;

  std::weak_ptr<LandmarkGroupRecord> m_landmarkGroupRecord;
};

#endif // DRAWABLE_POINT_LANDMARK_GROUP_H
