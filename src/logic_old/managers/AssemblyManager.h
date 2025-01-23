#ifndef ASSEMBLY_MANAGER_H
#define ASSEMBLY_MANAGER_H

#include "common/PublicTypes.h"
#include "common/UID.h"
#include "common/UIDRange.h"

#include "gui/layout/ViewType.h"

#include "logic/records/ImageRecord.h"
#include "logic/records/LabelTableRecord.h"
#include "logic/records/MeshRecord.h"
#include "logic/records/SlideRecord.h"

#include "rendering/assemblies/RenderingProperties.h"
#include "rendering/common/DrawableScaling.h"
#include "rendering/common/SceneType.h"
#include "rendering/common/ShaderProviderType.h"

#include <glm/fwd.hpp>

#include <list>
#include <map>
#include <memory>
#include <unordered_map>

class BlankTextures;
class DataManager;
class IDrawable;

/**
 * @brief Class that manages and owns all roots of assemblies that get rendered.
 * These include the assemblies of image slices, meshes, slides, and crosshairs.
 *
 * Each assembly drawable needs its own MeshRecord. These can be reused amongst
 * views, but not within same view.
 */
class AssemblyManager
{
public:
  AssemblyManager(DataManager&, ShaderProgramActivatorType, UniformsProviderType, std::weak_ptr<BlankTextures>);

  AssemblyManager(const AssemblyManager&) = delete;
  AssemblyManager& operator=(const AssemblyManager&) = delete;

  AssemblyManager(AssemblyManager&&) = default;
  AssemblyManager& operator=(AssemblyManager&&) = default;

  ~AssemblyManager();

  /// Set the function that updates all views.
  void setAllViewsUpdater(AllViewsUpdaterType);

  /// Set the function that provides the height of the slide stack, which is used for setting the
  /// length of the slide stack arrow.
  void setSlideStackHeightProvider(GetterType<float>);

  /// Set the function that provides the transformation from Slide Stack to World space.
  void setSlideStackToWorldTxProvider(GetterType<glm::mat4>);

  /// Set the function that queries the transformation from a given reference image landmark
  /// group to World space.
  void setRefImageLandmarkGroupToWorldTxQuerier(QuerierType<
                                                std::optional<std::pair<glm::mat4, glm::mat4> >,
                                                UID>);

  /// Set the function that queries the transformation from a given slide landmark group
  /// to World space.
  void setSlideLandmarkGroupToWorldTxQuerier(QuerierType<
                                             std::optional<std::pair<glm::mat4, glm::mat4> >,
                                             UID>);

  /// Set the function that queries the transformation from a given slide annotation to World space.
  void setSlideAnnotationToWorldTxQuerier(QuerierType<
                                          std::optional<std::pair<glm::mat4, glm::mat4> >,
                                          UID>);

  /// Set the function that queries scaling information for a given reference image landmark group.
  void setRefImageLandmarkGroupScalingQuerier(QuerierType<DrawableScaling, UID>);

  /// Set the function that queries scaling information for a given slide landmark group.
  void setSlideLandmarkGroupScalingQuerier(QuerierType<DrawableScaling, UID>);

  /// Set the function that queries the thickness of the slide associated with an annotation.
  void setSlideAnnotationThicknessQuerier(QuerierType<std::optional<float>, UID>);

  /// Set the function that queries whether a slide is active or not.
  /// (Used because the active slide is rendered differently.)
  void setActiveSlideQuerier(QuerierType<bool, UID>);

  /// Set the function that provides the transformation from the active image's Subject to World space.
  void setActiveSubjectToWorldProvider(GetterType<std::optional<glm::mat4> >);

  /// Set the function that provides the transformation from a label mesh's "Subject" to World space.
  void setLabelMeshSubjectToWorldTxQuerier(QuerierType<std::optional<glm::mat4>, UID>);

  /// Set the function that provides the transformation from an isosurface mesh's "Subject" to World space.
  void setIsoSurfaceMeshSubjectToWorldTxQuerier(QuerierType<std::optional<glm::mat4>, UID>);

  /// Initialize the assemblies. This call requires an OpenGL context.
  void initializeGL();

  /**
     * @brief Set the type of scene to be rendered in a given type of view
     * @param[in] viewType View type
     * @param[in] sceneType Scene type
     */
  void setSceneType(const gui::ViewType& viewType, const SceneType& sceneType);

  /**
     * @brief Get the type of scene to be rendered in a given type of view
     * @param[in] viewType View type
     */
  SceneType getSceneType(const gui::ViewType& viewType) const;

  /**
     * @brief Get the root of the tree of scene Drawables for all assemblies for a given view type.
     * @param[in] viewType View type for which to retrieve the root
     * @return Pointer to root drawable
     */
  std::weak_ptr<IDrawable> getRootDrawable(const gui::ViewType& viewType);

  /**
     * @brief Get the root of the tree of overlay Drawables for a given view type.
     *
     * @param[in] viewType View type for which to retrieve the root
     * @return Pointer to root drawable
     */
  std::weak_ptr<IDrawable> getOverlayRootDrawable(const gui::ViewType& viewType);

  /**
     * @brief Update the 3D image and parcellation to be rendered across all image
     * slices, meshes, and slides. This function takes care of updating the image
     * and parcellation data for all assemblies.
     *
     * @param[in] imageUid Image UID
     * @param[in] parcelUid Parcellation UID
     * @param[in] imageColorMapUid
     * @param[in] labelTableUid
     */
  void updateImages(
    const UID& imageUid, const UID& parcelUid, const UID& imageColorMapUid, const UID& labelTableUid
  );

  void updateIsoSurfaceMeshes(uid_range_t meshUids);
  void updateLabelMeshes(uid_range_t meshUids, const UID& labelTableUid);
  void updateSlideStack(uid_range_t slideUids);
  void updatedSlideTransformations(const std::list<UID>& slideUids);
  void updateImageColorMap(const UID& colorMapUid, bool updateViews);
  void updateLabelColorTable(const UID& colorTableUid, bool updateViews);

  void updateRefImageLandmarkGroups(const UID& imageUid);
  void updateSlideLandmarkGroups(uid_range_t slideUids);
  void updateSlideAnnotations(uid_range_t slideUids);

  void setImageSlicesVisibleIn2dViews(bool visible);
  void setImageSlicesVisibleIn3dViews(bool visible);
  void setImageSlicesAutoHiding(bool autoHide);

  void setParcellationVisibleIn2dViews(bool visible);
  void setParcellationVisibleIn3dViews(bool visible);

  void setIsoMeshMasterOpacity(float opacity);
  void setIsoMeshesVisibleIn2dViews(bool visible);
  void setIsoMeshesVisibleIn3dViews(bool visible);
  void setIsoMeshesUseXrayMode(bool useXrayMode);
  void setIsoMeshesXrayPower(float power);

  void setLabelMeshMasterOpacity(float opacity);
  void setLabelMeshesVisibleIn2dViews(bool visible);
  void setLabelMeshesVisibleIn3dViews(bool visible);
  void setLabelMeshesUseXrayMode(bool useXrayMode);
  void setLabelMeshesXrayPower(float power);

  void setSlideStackMasterOpacityMultiplier(float opacity);
  void setSlideStackImage3dLayerOpacity(float opacity);
  void setSlideStackVisibleIn2dViews(bool visible);
  void setSlideStackVisibleIn3dViews(bool visible);
  void setActiveSlideViewShows2dSlides(bool show2d);

  const ImageSliceAssemblyRenderingProperties& getImageSliceRenderingProperties() const;
  const MeshAssemblyRenderingProperties& getIsoMeshRenderingProperties() const;
  const MeshAssemblyRenderingProperties& getLabelMeshRenderingProperties() const;
  const SlideStackAssemblyRenderingProperties& getSlideRenderingProperties() const;
  const LandmarkAssemblyRenderingProperties& getRefImageLandmarkRenderingProperties() const;
  const LandmarkAssemblyRenderingProperties& getSlideLandmarkRenderingProperties() const;
  const AnnotationAssemblyRenderingProperties& getSlideAnnotationRenderingProperties() const;

  /// Connect an external slot to the signal that image slice assembly has changed
  void connectToImageSliceAssemblyRenderingPropertiesChangedSignal(
    std::function<void(const UID& imageUid, const ImageSliceAssemblyRenderingProperties&)> slot
  );

  /// Connect an external slot to the signal that iso-surface mesh assembly has changed
  void connectToIsoMeshAssemblyRenderingPropertiesChangedSignal(
    std::function<void(const MeshAssemblyRenderingProperties&)> slot
  );

  /// Connect an external slot to the signal that label mesh assembly has changed
  void connectToLabelMeshAssemblyRenderingPropertiesChangedSignal(
    std::function<void(const MeshAssemblyRenderingProperties&)> slot
  );

  /// Connect an external slot to the signal that slide assembly has changed
  void connectToSlideStackAssemblyRenderingPropertiesChangedSignal(
    std::function<void(const SlideStackAssemblyRenderingProperties&)> slot
  );

  /// Connect an external slot to the signal that slide transformations have changed
  void connectToSlideTransformationsChangedSignal(
    std::function<void(const std::list<UID>& slideUids)> slot
  );

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;

  /// Default mapping from ViewType to SceneType
  static const std::unordered_map<gui::ViewType, SceneType> smk_defaultViewTypeToSceneTypeMap;
};

#endif // ASSEMBLY_MANAGER_H
