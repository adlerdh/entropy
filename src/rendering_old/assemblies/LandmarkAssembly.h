#ifndef LANDMARK_ASSEMBLY_H
#define LANDMARK_ASSEMBLY_H

#include "rendering_old/interfaces/IDrawableAssembly.h"

#include "common/ObjectCounter.hpp"
#include "common/PublicTypes.h"

#include "logic_old/records/LandmarkGroupRecord.h"

#include "rendering/common/ShaderProviderType.h"
#include "rendering_old/assemblies/RenderingProperties.h"
#include "rendering_old/common/DrawableScaling.h"

#include <glm/fwd.hpp>

#include <uuid.h>

#include <unordered_map>
#include <utility>

class LandmarkGroup3d;
class MeshGpuRecord;
class Transformation;

/**
 * @brief Assembles drawables for point landmarks on reference images and slides.
 */
class LandmarkAssembly final : public IDrawableAssembly, public ObjectCounter<LandmarkAssembly>
{
  /// Function that queries the transformation from a given Landmark Group (keyed by its UID)
  /// to World space. If the Landmark Group does not exist, then std::nullopt returned.
  /// If it does exist, then two matrices are returned:
  /// 1) Full affine transformation from Landmark Group to World, which includes scale and shear.
  /// 2) Rigid-body transformation from Landmark Group to World, which ignores scale and shear.
  using LmGroupToWorldTxQuerierType
    = QuerierType<std::optional<std::pair<glm::mat4, glm::mat4> >, uuids::uuid>;

public:
  explicit LandmarkAssembly(
    ShaderProgramActivatorType,
    UniformsProviderType,
    GetterType<std::unique_ptr<MeshGpuRecord> > meshGpuRecordProvider,
    LmGroupToWorldTxQuerierType landmarkGroupToWorldTxQuerier,
    QuerierType<DrawableScaling, uuids::uuid> landarkScalingQuerier
  );

  ~LandmarkAssembly() override = default;

  /// @note Assembly initialization requires an OpenGL context
  void initialize() override;

  std::weak_ptr<DrawableBase> getRoot(const SceneType&) override;

  /// Set the function that queries the transformation from landmarks to World space
  void setLandmarkGroupToWorldTxQuerier(LmGroupToWorldTxQuerierType);

  /// Set the function that queries the scaling data for landmarks
  void setLandmarkGroupScalingQuerier(QuerierType<DrawableScaling, uuids::uuid>);

  /// Add a landmark group to the assembly for rendering.
  /// If it does not yet exist in this assembly, then it is added.
  void addLandmarkGroup(std::weak_ptr<LandmarkGroupRecord> lmGroupRecord);

  /// Remove a landmark group from the assembly, so that it is no longer rendered.
  void removeLandmarkGroup(const uuids::uuid& lmGroupUid);

  /// Clear all landmark groups from the assembly.
  void clearLandmarkGroups();

  void setMasterOpacityMultiplier(float opacity);

  void setLandmarksVisibleIn2dViews(bool visible);
  void setLandmarksVisibleIn3dViews(bool visible);

  void setLandmarksPickable(bool pickable);

  const LandmarkAssemblyRenderingProperties& getRenderingProperties() const;

private:
  /// Detatch drawables for a given LM group from the assembly roots
  void detatchLandmarks(const uuids::uuid& lmGroupUid);

  void updateRenderingProperties();

  ShaderProgramActivatorType m_shaderActivator;
  UniformsProviderType m_uniformsProvider;

  /// Function providing the GPU record for the mesh used to represent landmarks
  GetterType<std::unique_ptr<MeshGpuRecord> > m_meshGpuRecordProvider;

  /// Function that queries the matrix transformation from landmarks in given group to World space.
  LmGroupToWorldTxQuerierType m_landmarkGroupToWorldTxQuerier;

  /// Function that queries the scaling information for landmarks.
  /// Key: UID of landmark group
  QuerierType<DrawableScaling, uuids::uuid> m_landmarkScalingQuerier;

  /// Mesh record passed down to all landmark drawables
  std::shared_ptr<MeshGpuRecord> m_meshGpuRecord;

  /// Root for landmark groups in 2D views.
  std::shared_ptr<Transformation> m_rootFor2dViews;

  /// Root for landmark groups in 3D views.
  std::shared_ptr<Transformation> m_rootFor3dViews;

  /// Hash map of landmark group drawables. (Key: UID of the landmark group).
  /// @todo This map will hold separate 2D and 3D versions of the drawables.
  std::unordered_map<
    uuids::uuid,
    std::pair<std::shared_ptr<LandmarkGroup3d>, std::shared_ptr<LandmarkGroup3d> > >
    m_lmDrawables;

  /// Rendering properties for all landmarks
  LandmarkAssemblyRenderingProperties m_lmProperties;
};

#endif // LANDMARK_ASSEMBLY_H
