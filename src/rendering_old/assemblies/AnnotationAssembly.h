#ifndef ANNOTATION_ASSEMBLY_H
#define ANNOTATION_ASSEMBLY_H

#include "rendering_old/interfaces/IDrawableAssembly.h"

#include "common/ObjectCounter.hpp"
#include "common/PublicTypes.h"

#include "logic_old/records/SlideAnnotationRecord.h"

#include "rendering/common/ShaderProviderType.h"
#include "rendering_old/assemblies/RenderingProperties.h"

#include <glm/fwd.hpp>

#include <uuid.h>

#include <unordered_map>
#include <utility>

class AnnotationExtrusion;
class AnnotationSlice;
class DynamicTransformation;
class LandmarkGroup3d;
class MeshGpuRecord;
class Transformation;

/**
 * @brief Assembles drawables for slide annotations.
 */
class AnnotationAssembly final : public IDrawableAssembly, public ObjectCounter<AnnotationAssembly>
{
public:
  explicit AnnotationAssembly(
    ShaderProgramActivatorType,
    UniformsProviderType,
    QuerierType<std::optional<std::pair<glm::mat4, glm::mat4> >, uuids::uuid>
      annotationToWorldTxQuerier,
    QuerierType<std::optional<float>, uuids::uuid> annotationThicknessQuerier
  );

  ~AnnotationAssembly() override = default;

  /// @note Initialization requires an OpenGL context
  void initialize() override;

  std::weak_ptr<DrawableBase> getRoot(const SceneType&) override;

  void setAnnotationToWorldTxQuerier(QuerierType<
                                     std::optional<std::pair<glm::mat4, glm::mat4> >,
                                     uuids::uuid>);

  void setAnnotationThicknessQuerier(QuerierType<std::optional<float>, uuids::uuid>);

  /// Set/replace an annotation. If it does not yet exist in this assembly, then it is added.
  void setAnnotation(std::weak_ptr<SlideAnnotationRecord>);

  /// Remove an annotation.
  //    void removeAnnotation( const UID& annotUid );

  /// Clear all annotations.
  //    void clearAnnotations();

  /// Set/replace a single point within an annotation.
  //    void setAnnotationPoint( const UID& annotUid, const PointRecord<3>& pointRecord );

  void setMasterOpacityMultiplier(float opacity);

  void setVisibleIn2dViews(bool visible);
  void setVisibleIn3dViews(bool visible);

  void setPickable(bool pickable);

  const AnnotationAssemblyRenderingProperties& getRenderingProperties() const;

private:
  /// Structure that holds separate versions of annotation drawables that are intended to be
  /// rendered in the 2D and 3D views.
  struct Annotations
  {
    /// Root for 2D views that maps annotation into World space
    std::shared_ptr<DynamicTransformation> m_world_O_annot_root2d = nullptr;

    /// Root for 3D views that maps annotation into World space
    std::shared_ptr<DynamicTransformation> m_world_O_annot_root3d = nullptr;

    /// AnnotationSlice is renderd in 2D views
    std::shared_ptr<AnnotationSlice> m_annot2d = nullptr;

    /// AnnotationExtrusion is rendered in 3D views
    std::shared_ptr<AnnotationExtrusion> m_annot3d = nullptr;
  };

  //    void detatchAnnotations( const Annotations& annot );

  void updateRenderingProperties();

  ShaderProgramActivatorType m_shaderActivator;
  UniformsProviderType m_uniformsProvider;

  /// Function that queries the matrix transformation from an annotation to World space.
  /// Key: UID of annotation
  QuerierType<std::optional<std::pair<glm::mat4, glm::mat4> >, uuids::uuid>
    m_annotationToWorldTxQuerier;

  /// Function that queries the thickness of the slide associated with a slide annotation
  /// Key: UID of annotation
  QuerierType<std::optional<float>, uuids::uuid> m_annotationThicknessQuerier;

  /// Roots for all annotations in 2D views
  std::shared_ptr<Transformation> m_rootFor2dViews;

  /// Roots for all annotations in 3D views
  std::shared_ptr<Transformation> m_rootFor3dViews;

  /// Hash map of annotation drawables. (Key: UID of the annotation)
  std::unordered_map<uuids::uuid, Annotations> m_annotations;

  /// Rendering properties for all annotations
  AnnotationAssemblyRenderingProperties m_properties;
};

#endif // LANDMARK_ASSEMBLY_H
