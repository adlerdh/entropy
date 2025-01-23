#ifndef MESH_ASSEMBLY_H
#define MESH_ASSEMBLY_H

#include "rendering/common/ShaderProviderType.h"
#include "rendering_old/assemblies/RenderingProperties.h"
#include "rendering_old/interfaces/IDrawableAssembly.h"
#include "rendering_old/interfaces/ITexturable3D.h"

#include "logic/records/MeshRecord.h"

#include "common/ObjectCounter.hpp"

#include <uuid.h>

#include <optional>
#include <unordered_map>

class BlankTextures;
class DynamicTransformation;
class TexturedMesh;
class Transformation;

class MeshAssembly final : public IDrawableAssembly,
                           public ITexturable3d,
                           public ObjectCounter<MeshAssembly>
{
public:
  explicit MeshAssembly(
    ShaderProgramActivatorType, UniformsProviderType, std::weak_ptr<BlankTextures> blankTextures
  );

  ~MeshAssembly() override = default;

  void initialize() override;

  std::weak_ptr<DrawableBase> getRoot(const SceneType&) override;

  void setImage3dRecord(std::weak_ptr<ImageRecord>) override;
  void setParcellationRecord(std::weak_ptr<ParcellationRecord>) override;

  /**
     * @brief Set the image color map record. This also updates iso-surface mesh properties
     * according to the image color map.
     */
  void setImageColorMapRecord(std::weak_ptr<ImageColorMapRecord>) override;

  /**
     * @brief Set the label table record. This also updates label mesh properties
     * according to the label table properties.
     */
  void setLabelTableRecord(std::weak_ptr<LabelTableRecord>) override;

  /**
     * @brief Set the function that queries the transformation mapping mesh to World space.
     *
     * @param querier Function with single input being the mesh UID and output the
     * transformation world_O_mesh that transforms the mesh vertices to World space.
     */
  void setMeshSubjectToWorldTxQuerier(QuerierType<std::optional<glm::mat4>, uuids::uuid>);

  void addMesh(const uuids::uuid& meshUid, std::weak_ptr<MeshRecord> meshRecord);
  void removeMesh(const uuids::uuid& meshUid);
  void clearMeshes();

  void setMasterOpacityMultiplier(const uuids::uuid& meshUid, const float multiplier);
  void setMasterOpacityMultiplier(float multiplier);

  void setUseOctantClipPlanes(bool use);

  void setShowIn2dViews(bool visible);
  void setShowIn3dViews(bool visible);

  void setUseXrayMode(bool set);
  void setXrayPower(float power);

  void setPickable(bool pickable);

  const MeshAssemblyRenderingProperties& getRenderingProperties() const;

private:
  void updateMeshRenderingProperties();
  void updateMeshColors();

  ShaderProgramActivatorType m_shaderActivator;
  UniformsProviderType m_uniformsProvider;
  std::weak_ptr<BlankTextures> m_blankTextures;

  QuerierType<std::optional<glm::mat4>, uuids::uuid> m_meshSubjectToWorldQuerier;

  std::weak_ptr<ImageRecord> m_image3dRecord;
  std::weak_ptr<ParcellationRecord> m_parcelRecord;
  std::weak_ptr<ImageColorMapRecord> m_imageColorMapRecord;
  std::weak_ptr<LabelTableRecord> m_labelTableRecord;

  /// Root Drawables for 2D and 3D views. The 2D and 3D views have separate trees of Drawables.
  std::shared_ptr<Transformation> m_rootTx2d;
  std::shared_ptr<Transformation> m_rootTx3d;

  // For each mesh, the Assembly internally holds separate TexturedMesh drawable
  // objects that are rendered specifically for 2D and 3D view types
  struct MeshDrawables
  {
    /// Parent tx for mesh in 2D view
    std::shared_ptr<DynamicTransformation> m_world_O_subject_for2d = nullptr;

    /// Parent tx for mesh in 3D view
    std::shared_ptr<DynamicTransformation> m_world_O_subject_for3d = nullptr;

    /// Mesh in 2D view
    std::shared_ptr<TexturedMesh> m_meshFor2d = nullptr;

    /// Mesh in 3D view
    std::shared_ptr<TexturedMesh> m_meshFor3d = nullptr;

    /// Mesh record
    std::weak_ptr<MeshRecord> m_meshRecord;
  };

  /// Hash map of Mesh Drawables to render in both 2D and 3D views
  std::unordered_map<uuids::uuid, MeshDrawables> m_meshes;

  /// Rendering properties for all meshes:
  MeshAssemblyRenderingProperties m_properties;
};

#endif // MESH_ASSEMBLY_H
