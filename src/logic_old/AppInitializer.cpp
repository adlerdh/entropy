#include "logic/AppInitializer.h"
#include "logic/AppController.h"

#include "logic/managers/ActionManager.h"
#include "logic/managers/AssemblyManager.h"
#include "logic/managers/ConnectionManager.h"
#include "logic/managers/DataManager.h"
#include "logic/managers/GuiManager.h"
#include "logic/managers/InteractionManager.h"
#include "logic/managers/LayoutManager.h"
#include "logic/managers/TransformationManager.h"

#include "logic/ui/ImageDataUiMapper.h"
#include "logic/ui/ParcellationDataUiMapper.h"
#include "logic/ui/SlideStackDataUiMapper.h"

#include "rendering/utility/containers/BlankTextures.h"
#include "rendering/utility/containers/ShaderProgramContainer.h"

#include <functional>

namespace
{

static const char* sk_viewLayoutsConfig =
#include "gui/layout/config/ViewLayouts.json"
  ;

} // namespace

std::unique_ptr<AppController> createAppController()
{
  using namespace std::placeholders;

  // Stores all 3D image, 3D label, and slide data.
  auto dataManager = std::make_unique<DataManager>();

  // Constructs and stores view layouts.
  auto layoutManager = std::make_unique<LayoutManager>(sk_viewLayoutsConfig);

  // Stores crosshairs and slide stack coordinate frame transformations.
  auto txManager = std::make_unique<TransformationManager>();

  // Stores objects that control view interaction. Also manages the view cameras
  // and alignment of cameras to images, crosshairs, and the slide stack frame.
  auto interactionManager = std::make_unique<InteractionManager>(
    std::bind(&LayoutManager::getViewTypes, layoutManager.get()),
    std::bind(
      &TransformationManager::getCrosshairsFrame, txManager.get(), TransformationState::Committed
    ), /// remove
    std::bind(
      &TransformationManager::getSlideStackCrosshairsFrame,
      txManager.get(),
      TransformationState::Committed
    ), /// remove
    std::bind(
      &TransformationManager::getSlideStackFrame, txManager.get(), TransformationState::Committed
    )
  ); /// remove

  auto shaderPrograms = std::make_unique<ShaderProgramContainer>();

  // This is a shared pointer, since it gets passed down to rendering
  // objects where it is held as a weak pointer
  auto blankTextures = std::make_shared<BlankTextures>();

  // Constructs, manages, and modifies the assemblies of Drawables that are rendered.
  // It passes the shader programs and blank textures down to the Drawables.
  auto assemblyManager = std::make_unique<AssemblyManager>(
    *dataManager,
    std::bind(&ShaderProgramContainer::useProgram, shaderPrograms.get(), _1),
    std::bind(&ShaderProgramContainer::getRegisteredUniforms, shaderPrograms.get(), _1),
    blankTextures
  );

  // Constructs the GLWidgets and renderers.
  auto guiManager = std::make_unique<GuiManager>(
    std::bind(&LayoutManager::getViewTypes, layoutManager.get()),
    std::bind(&LayoutManager::setViewWidget, layoutManager.get(), _1),
    std::bind(&InteractionManager::getInteractionPack, interactionManager.get(), _1),
    std::bind(&AssemblyManager::getRootDrawable, assemblyManager.get(), _1),
    std::bind(&AssemblyManager::getOverlayRootDrawable, assemblyManager.get(), _1),
    std::bind(&AssemblyManager::getSceneType, assemblyManager.get(), _1),
    std::bind(&ShaderProgramContainer::useProgram, shaderPrograms.get(), _1),
    std::bind(&ShaderProgramContainer::getRegisteredUniforms, shaderPrograms.get(), _1)
  );

  // Performs actions that are usually triggered by the GUI and that affect the GUI
  auto actionManager = std::make_unique<ActionManager>(
    std::bind(&LayoutManager::getViewTypes, layoutManager.get()),
    std::bind(&ShaderProgramContainer::useProgram, shaderPrograms.get(), _1),
    std::bind(&ShaderProgramContainer::getRegisteredUniforms, shaderPrograms.get(), _1),
    *assemblyManager,
    *dataManager,
    *guiManager,
    *interactionManager
  );

  // Object that maps image data between UI and app
  auto imageDataUiMapper = std::make_unique<ImageDataUiMapper>(
    *actionManager,
    *assemblyManager,
    *dataManager,
    std::bind(&GuiManager::updateAllViewWidgets, guiManager.get())
  );

  // Object that maps parcellation data between UI and app
  auto parcelDataUiMapper = std::make_unique<ParcellationDataUiMapper>(
    *actionManager,
    *assemblyManager,
    *dataManager,
    std::bind(&GuiManager::updateAllViewWidgets, guiManager.get())
  );

  // Object that maps parcellation data between UI and app
  auto slideStackDataUiMapper = std::make_unique<SlideStackDataUiMapper>(
    *actionManager,
    *assemblyManager,
    *dataManager,
    *interactionManager,
    std::bind(&GuiManager::updateAllViewWidgets, guiManager.get())
  );

  auto connectionManager = std::make_unique<ConnectionManager>(
    *actionManager,
    *assemblyManager,
    *dataManager,
    *guiManager,
    *interactionManager,
    *layoutManager,
    *txManager,
    *imageDataUiMapper,
    *parcelDataUiMapper,
    *slideStackDataUiMapper,

    /// @todo remove these:
    std::bind(&GuiManager::getViewWidget, guiManager.get(), _1),
    std::bind(&AssemblyManager::getSceneType, assemblyManager.get(), _1),
    std::bind(&LayoutManager::getViewTypes, layoutManager.get()),
    std::bind(&LayoutManager::getViewUidsOfType, layoutManager.get(), _1),
    std::bind(&InteractionManager::getInteractionPack, interactionManager.get(), _1)
  );

  return std::make_unique<AppController>(
    std::move(actionManager),
    std::move(assemblyManager),
    std::move(connectionManager),
    std::move(dataManager),
    std::move(guiManager),
    std::move(interactionManager),
    std::move(layoutManager),
    std::move(txManager),
    std::move(imageDataUiMapper),
    std::move(parcelDataUiMapper),
    std::move(slideStackDataUiMapper),
    std::move(shaderPrograms),
    std::move(blankTextures)
  );
}
