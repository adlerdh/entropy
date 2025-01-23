#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include "common/PublicTypes.h"
#include "common/UID.h"
#include "gui/layout/ViewType.h"
#include "gui/layout/ViewTypeRange.h"
#include "rendering/common/SceneType.h"

#include <glm/fwd.hpp>

#include <functional>
#include <list>
#include <map>
#include <memory>

class ActionManager;
class AssemblyManager;
class CoordinateFrame;
class DataManager;
class GuiManager;
class InteractionManager;
class InteractionPack;
class LayoutManager;
class TransformationManager;

class ImageDataUiMapper;
class ParcellationDataUiMapper;
class SlideStackDataUiMapper;

namespace gui
{
class ViewWidget;
}

/// The rule is that "simple" actions are written inline in this class.
/// More complex actions are implemented in ActionManager.
class ConnectionManager
{
private:
  /// Functional returning the widget corresponding to a view keyed by its UID.
  /// If the view UID does not exist, nullptr is returned.
  using ViewWidgetProviderType = std::function<gui::ViewWidget*(const UID& viewUid)>;

  /// Functional returning the scene type corresponding to a view type.
  /// If the view type does not exist, std::nullopt is returned.
  using SceneTypeProviderType = std::function<SceneType(const gui::ViewType&)>;

  /// Functional returning list of UIDs of views with a given view type.
  using ViewsOfTypeProviderType = std::function<std::list<UID>(const gui::ViewType&)>;

  /// Functional returning InteractionPack for a view keyed by its UID.
  /// If the view UID does not exist, nullptr is returned.
  using InteractionPackProviderType = std::function<InteractionPack*(const UID& viewUid)>;

public:
  ConnectionManager(
    ActionManager& actionManager,
    AssemblyManager& assemblyManager,
    DataManager& dataManager,
    GuiManager& guiManager,
    InteractionManager& interactionManager,
    LayoutManager& layoutManager,
    TransformationManager& txManager,
    ImageDataUiMapper& imageUiMappper,
    ParcellationDataUiMapper& parcelUiMappper,
    SlideStackDataUiMapper& slideStackUiMappper,
    ViewWidgetProviderType viewWidgetProvider,
    SceneTypeProviderType sceneTypeProvider,
    GetterType<view_type_range_t> viewUidAndTypeRangeProvider,
    ViewsOfTypeProviderType viewsOfTypeProvider,
    InteractionPackProviderType interactionPackProvider
  );

  ConnectionManager(const ConnectionManager&) = delete;
  ConnectionManager& operator=(const ConnectionManager&) = delete;

  ConnectionManager(ConnectionManager&&) = default;
  ConnectionManager& operator=(ConnectionManager&&) = default;

  ~ConnectionManager();

  void createConnections();

  /// Connect an external slot to the signal that image window/level data has changed
  void connectToImageWindowLevelChangedSignal(std::function<void(const UID& imageUid)>);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

#endif // CONNECTION_MANAGER_H
