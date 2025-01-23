#ifndef DRAWABLE_LANDMARK_GROUP_H
#define DRAWABLE_LANDMARK_GROUP_H

#include "rendering/common/ShaderProviderType.h"
#include "rendering/drawables/DrawableBase.h"
#include "rendering_old/common/DrawableScaling.h"

#include "common/ObjectCounter.hpp"
#include "common/PublicTypes.h"

#include "logic_old/records/LandmarkGroupRecord.h"

#include <glm/fwd.hpp>

#include <uuid.h>

#include <memory>
#include <unordered_map>

class BasicMesh;
class MeshGpuRecord;
class Transformation;

/**
 * @brief A group of point landmark drawables for 3D views. Each landmark is rendered as a 3D mesh
 * (e.g. sphere for reference image landmarks and cylinder for slide landmarks).
 */
class LandmarkGroup3d : public DrawableBase, public ObjectCounter<LandmarkGroup3d>
{
public:
  LandmarkGroup3d(
    std::string name,
    ShaderProgramActivatorType shaderActivator,
    UniformsProviderType uniformsProvider,
    std::weak_ptr<LandmarkGroupRecord> landmarkGroupRecord,
    std::weak_ptr<MeshGpuRecord> meshGpuRecord,
    GetterType<std::optional<DrawableScaling> > scalingProvider,
    GetterType<std::optional<std::pair<glm::mat4, glm::mat4> > > landmarkToWorldTxProvider
  );

  ~LandmarkGroup3d() override = default;

  /// Set function that provides scaling information for the landmark
  void setScalingInfoProvider(GetterType<std::optional<DrawableScaling> >);

  /// Set function that provides the transformation of the landmark from its local coordinates
  /// to World space. The value returned is a pair consisting of
  /// 1) Full affine transformation
  /// 2) Rigid-body transformation
  void setLandmarkToWorldTxProvider(GetterType<std::optional<std::pair<glm::mat4, glm::mat4> > >);

private:
  /// Drawables for each child landmark belonging to the landmark group
  struct Landmark
  {
    Landmark(LandmarkGroup3d& lmGroup);
    ~Landmark() = default;

    /// Parent modeling transformation atop the landmark mesh
    std::shared_ptr<Transformation> m_tx;

    /// Landmark drawable mesh
    std::shared_ptr<BasicMesh> m_mesh;
  };

  void doUpdate(double, const Viewport&, const camera::Camera&, const CoordinateFrame&) override;

  ShaderProgramActivatorType m_shaderActivator;
  UniformsProviderType m_uniformsProvider;

  /// The landmark group rendered by this drawable
  std::weak_ptr<LandmarkGroupRecord> m_landmarkGroupRecord;

  /// Mesh GPU record of the landmark
  std::weak_ptr<MeshGpuRecord> m_meshGpuRecord;

  /// Function providing scaling information
  GetterType<std::optional<DrawableScaling> > m_scalingProvider;

  /// Function providing transformation from landmark to World space:
  /// 1) Affine tx
  /// 2) Rigid-body tx
  GetterType<std::optional<std::pair<glm::mat4, glm::mat4> > > m_landmarkToWorldTxProvider;

  /// Child landmarks
  std::unordered_map<uuids::uuid, Landmark> m_landmarks;
};

#endif // DRAWABLE_LANDMARK_GROUP_H
