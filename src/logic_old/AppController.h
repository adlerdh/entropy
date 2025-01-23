#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include "common/UID.h"
#include "logic/serialization/ProjectSerialization.h"

#include <QOffscreenSurface>

#include <memory>
#include <optional>
#include <string>
#include <vector>

class ActionManager;
class AssemblyManager;
class BlankTextures;
class ConnectionManager;
class DataManager;
class ImageDataUiMapper;
class ParcellationDataUiMapper;
class GuiManager;
class InteractionManager;
class LayoutManager;
class QOpenGLContext;
class ShaderProgramContainer;
class SlideStackDataUiMapper;
class TransformationManager;

class AppController
{
public:
  AppController(std::unique_ptr<ActionManager>, std::unique_ptr<AssemblyManager>, std::unique_ptr<ConnectionManager>, std::unique_ptr<DataManager>, std::unique_ptr<GuiManager>, std::unique_ptr<InteractionManager>, std::unique_ptr<LayoutManager>, std::unique_ptr<TransformationManager>, std::unique_ptr<ImageDataUiMapper>, std::unique_ptr<ParcellationDataUiMapper>, std::unique_ptr<SlideStackDataUiMapper>, std::unique_ptr<ShaderProgramContainer>, std::shared_ptr<BlankTextures>);

  ~AppController();

  void showMainWindow();

  void loadProject(serialize::HZeeProject);

  void generateIsoSurfaceMesh(double isoValue);

  void generateLabelMeshes();

  void setupCamerasAndCrosshairsForImage();

  void loadBuiltInImageColorMaps(const std::vector<std::string>& colormapFileNames);

  void testTransformFeedback();

  /// @test
  void testAlignSlideStackToActiveImage();
  void testCreateRefImageLandmark();
  void testCreateSlideLandmark();
  void testCreateSlideAnnotation();

private:
  void initialize();
  void createUiConnections();

  std::unique_ptr<ActionManager> m_actionManager;
  std::unique_ptr<AssemblyManager> m_assemblyManager;
  std::unique_ptr<ConnectionManager> m_connectionManager;
  std::unique_ptr<DataManager> m_dataManager;
  std::unique_ptr<GuiManager> m_guiManager;
  std::unique_ptr<InteractionManager> m_interactionManager;
  std::unique_ptr<LayoutManager> m_layoutManager;
  std::unique_ptr<TransformationManager> m_transformationManager;

  std::unique_ptr<ImageDataUiMapper> m_imageDataUiMapper;
  std::unique_ptr<ParcellationDataUiMapper> m_parcelDataUiMapper;
  std::unique_ptr<SlideStackDataUiMapper> m_slideStackDataUiMapper;

  std::unique_ptr<ShaderProgramContainer> m_shaderPrograms;
  std::shared_ptr<BlankTextures> m_blankTextures;

  QOpenGLContext* m_globalContext;
  QOffscreenSurface m_surface;
};

#endif // APP_CONTROLLER_H
