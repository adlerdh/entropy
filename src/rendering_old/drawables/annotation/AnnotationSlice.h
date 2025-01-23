#ifndef ANNOTATION_SLICE_H
#define ANNOTATION_SLICE_H

#include "rendering/common/ShaderProviderType.h"
#include "rendering/drawables/DrawableBase.h"
#include "rendering_old/records/MeshGpuRecord.h"

#include "common/ObjectCounter.hpp"
#include "common/PublicTypes.h"

#include "logic_old/records/SlideAnnotationRecord.h"

#include <uuid.h>

#include <memory>
#include <optional>

class BasicMesh;

/**
 * @brief Render the intersection of a slide annotation with the view plane
 */
class AnnotationSlice : public DrawableBase, public ObjectCounter<AnnotationSlice>
{
public:
  AnnotationSlice(
    std::string name,
    ShaderProgramActivatorType shaderActivator,
    UniformsProviderType uniformsProvider,
    GetterType<std::optional<glm::mat4> > annotToWorldTxProvider,
    std::weak_ptr<SlideAnnotationRecord> slideAnnotationRecord
  );

  ~AnnotationSlice() override = default;

  bool isOpaque() const override;

  //    DrawableOpacity opacityFlag() const override;

private:
  void doUpdate(double, const Viewport&, const camera::Camera&, const CoordinateFrame&) override;

  bool isMeshGpuRecordCurrent() const;

  void updateMeshGpuRecord();

  void setupChildren();

  ShaderProgramActivatorType m_shaderActivator;
  UniformsProviderType m_uniformsProvider;

  /// Function providing transformation from this annotation's Slide space to World space
  GetterType<std::optional<glm::mat4> > m_annotToWorldTxProvider;

  /// Slide annotation record that is rendered as a slice by this drawable
  std::weak_ptr<SlideAnnotationRecord> m_slideAnnotationRecord;

  /// GPU record of the mesh of the annotation slice
  std::shared_ptr<MeshGpuRecord> m_meshGpuRecord;

  /// Slice mesh drawable (a child of this object)
  std::shared_ptr<BasicMesh> m_mesh;

  /// UID of the current annotation. If no current annotation, then it is set to none.
  std::optional<uuids::uuid> m_currentAnnotationUid;
};

#endif // ANNOTATION_SLICE_H
