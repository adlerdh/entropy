#ifndef ACTION_MANAGER_H
#define ACTION_MANAGER_H

#include "common/PublicTypes.h"
#include "common/UID.h"
#include "gui/layout/ViewTypeRange.h"
#include "rendering/common/ShaderProviderType.h"

#include <glm/fwd.hpp>

#include <QOffscreenSurface>

#include <functional>
#include <optional>
#include <string>

class AssemblyManager;
class CoordinateFrame;
class DataManager;
class GuiManager;
class InteractionManager;
class QOpenGLContext;

/**
 * @brief Handles high-level application actions.
 */
class ActionManager
{
public:
  ActionManager(GetterType<view_type_range_t> viewUidAndTypeRangeProvider, ShaderProgramActivatorType, UniformsProviderType, AssemblyManager&, DataManager&, GuiManager&, InteractionManager&);

  ActionManager(const ActionManager&) = delete;
  ActionManager& operator=(const ActionManager&) = delete;

  ActionManager(ActionManager&&) = delete;
  ActionManager& operator=(ActionManager&&) = delete;

  ~ActionManager();

  void setSlideStackFrameProvider(GetterType<CoordinateFrame>);
  void setCrosshairsFrameProvider(GetterType<CoordinateFrame>);

  /// Set the crosshairs position in the application
  void setCrosshairsFrameChangedBroadcaster(SetterType<const CoordinateFrame&>);

  /// Set the crosshairs rotation in the application
  void setCrosshairsFrameChangeDoneBroadcaster(SetterType<const CoordinateFrame&>);

  /// Center the crosshairs on the given image
  void centerCrosshairsOnImage(const UID& imageUid);

  /// Center the crosshairs on the given slide
  void centerCrosshairsOnSlide(const UID& slideUid);

  /// Align crosshairs to the active slide
  void alignCrosshairsToActiveSlide();

  /// Align crosshairs to the slide stack frame
  void alignCrosshairsToSlideStackFrame();

  /// Align crosshairs to the anatomical planes (x, y, z of Subject space) of the active image
  void alignCrosshairsToSubjectXyzPlanes();

  /// Update the application status of a new crosshairs position
  void updateWorldPositionStatus();

  /// Reset all view cameras to their default orienations and projections
  void resetViews();

  void setupCamerasAndCrosshairsForImage();

  /// Load a 3D image from disk and set it as the active image
  std::optional<UID> loadImage(
    const std::string& filename, const std::optional<std::string>& dicomSeriesUid
  );

  /// Load a 3D parcellation from disk and set it as the active parcellation
  std::optional<UID> loadParcellation(
    const std::string& filename, const std::optional<std::string>& dicomSeriesUid
  );

  /// Load a slide image from disk and set it as the active slide
  std::optional<UID> loadSlide(const std::string& filename, bool translateToTopOfStack);

  /// Save project back to disk
  /// @param[in] newFileName Optional new file name. If not provided, then the project is saved
  /// to the same file that it was loaded from.
  void saveProject(const std::optional<std::string>& newFileName);

  /// Generate an iso-surface mesh for the active image
  void generateIsoSurfaceMesh(double isoValue);

  /// Generate label surface meshes for the active parcellation
  void generateLabelMeshes();

  /// These are convenience functions that update Assemblies based on the latest data in DataManager
  void updateImageSliceAssembly();
  void updateIsoMeshAssembly();
  void updateLabelMeshAssembly();
  void updateSlideStackAssembly();
  void updateLandmarkAssemblies();
  void updateAnnotationAssemblies();

  void updateAllAssemblies();

  void transformFeedback();

  void updateAllViews();

private:
  /// @todo If we pass textures to Mesh(), then addMesh() in updateMeshAssembly() shouldn't
  /// need an OpenGL context any more. These blank meshes should live in AssemblyManager.

  QOpenGLContext* m_globalContext;
  QOffscreenSurface m_surface;

  GetterType<view_type_range_t> m_viewUidAndTypeProvider;
  ShaderProgramActivatorType m_shaderProgramActivator;
  UniformsProviderType m_uniformsProvider;

  AssemblyManager& m_assemblyManager;
  DataManager& m_dataManager;
  GuiManager& m_guiManager;
  InteractionManager& m_interactionManager;

  GetterType<CoordinateFrame> m_slideStackFrameProvider;
  GetterType<CoordinateFrame> m_crosshairsFrameProvider;
  SetterType<const CoordinateFrame&> m_crosshairsFrameChangedBroadcaster;
  SetterType<const CoordinateFrame&> m_crosshairsFrameChangedDoneBroadcaster;
};

#endif // ACTION_MANAGER_H
