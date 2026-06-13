#include "ui/menus/MacNativeMainMenu.h"

#import <Cocoa/Cocoa.h>

@class EntropyMacMenuTarget;

namespace {
MainMenuBarCallbacks g_callbacks;
EntropyMacMenuTarget* g_target = nil;
NSMenuItem* g_openImageItem = nil;
NSMenuItem* g_openProjectItem = nil;
NSMenuItem* g_openDicomSeriesItem = nil;
NSMenuItem* g_addImageItem = nil;
NSMenuItem* g_saveProjectItem = nil;
NSMenuItem* g_saveProjectAsItem = nil;
NSMenuItem* g_closeProjectItem = nil;
NSMenu* g_activeImagesMenu = nil;
NSMenu* g_layoutsMenu = nil;
bool g_installed = false;
}  // namespace

@interface EntropyMacMenuTarget : NSObject
- (void)openImage:(id)sender;
- (void)openProject:(id)sender;
- (void)openDicomSeries:(id)sender;
- (void)addImage:(id)sender;
- (void)addSegmentation:(id)sender;
- (void)saveProject:(id)sender;
- (void)saveProjectAs:(id)sender;
- (void)closeProject:(id)sender;
- (void)loadLayouts:(id)sender;
- (void)saveLayouts:(id)sender;
- (void)previousLayout:(id)sender;
- (void)nextLayout:(id)sender;
- (void)selectLayout:(id)sender;
- (void)selectActiveImage:(id)sender;
- (void)performMenuAction:(id)sender;
- (void)showAbout:(id)sender;
- (void)quitApp:(id)sender;
- (BOOL)validateMenuItem:(NSMenuItem*)menuItem;
@end

@implementation EntropyMacMenuTarget
- (void)openImage:(id)sender {
  (void)sender;
  main_menu::openImage(g_callbacks);
}

- (void)openProject:(id)sender {
  (void)sender;
  main_menu::openProject(g_callbacks);
}

- (void)openDicomSeries:(id)sender {
  (void)sender;
  main_menu::openDicomSeries(g_callbacks);
}

- (void)addImage:(id)sender {
  (void)sender;
  main_menu::addImage(g_callbacks);
}

- (void)addSegmentation:(id)sender {
  (void)sender;
  main_menu::addSegmentation(g_callbacks);
}

- (void)saveProject:(id)sender {
  (void)sender;
  main_menu::saveProject(g_callbacks);
}

- (void)saveProjectAs:(id)sender {
  (void)sender;
  main_menu::saveProjectAs(g_callbacks);
}

- (void)closeProject:(id)sender {
  (void)sender;
  main_menu::closeProject(g_callbacks);
}

- (void)loadLayouts:(id)sender {
  (void)sender;
  main_menu::loadLayouts(g_callbacks);
}

- (void)saveLayouts:(id)sender {
  (void)sender;
  main_menu::saveLayouts(g_callbacks);
}

- (void)previousLayout:(id)sender {
  (void)sender;
  if (g_callbacks.cycleLayouts) {
    g_callbacks.cycleLayouts(-1);
  }
}

- (void)nextLayout:(id)sender {
  (void)sender;
  if (g_callbacks.cycleLayouts) {
    g_callbacks.cycleLayouts(1);
  }
}

- (void)selectLayout:(id)sender {
  NSMenuItem* item = (NSMenuItem*)sender;
  if (g_callbacks.setCurrentLayoutIndex) {
    g_callbacks.setCurrentLayoutIndex(static_cast<std::size_t>([item tag]));
  }
}

- (void)selectActiveImage:(id)sender {
  NSMenuItem* item = (NSMenuItem*)sender;
  if (g_callbacks.setActiveImageIndex) {
    g_callbacks.setActiveImageIndex(static_cast<std::size_t>([item tag]));
  }
}

- (void)performMenuAction:(id)sender {
  NSMenuItem* item = (NSMenuItem*)sender;
  main_menu::performAction(g_callbacks, static_cast<MainMenuAction>([item tag]));
}

- (void)showAbout:(id)sender {
  (void)sender;
  if (g_callbacks.showAbout) {
    g_callbacks.showAbout();
  }
}

- (void)quitApp:(id)sender {
  (void)sender;
  main_menu::quitApp(g_callbacks);
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  const SEL action = [menuItem action];

  if (action == @selector(openImage:)) {
    return g_callbacks.canOpenProject && g_callbacks.openImageFiles;
  }

  if (action == @selector(openProject:)) {
    return g_callbacks.canOpenProject && g_callbacks.openProjectFile;
  }

  if (action == @selector(openDicomSeries:)) {
    return g_callbacks.canOpenProject && g_callbacks.openDicomFolders;
  }

  if (action == @selector(addImage:)) {
    return g_callbacks.canAddImage && g_callbacks.addImageFiles;
  }

  if (action == @selector(addSegmentation:)) {
    return g_callbacks.canAddSegmentation && g_callbacks.addSegmentationFile;
  }

  if (action == @selector(saveProject:)) {
    return g_callbacks.canSaveProject && g_callbacks.saveProject;
  }

  if (action == @selector(saveProjectAs:)) {
    return g_callbacks.canSaveProject && g_callbacks.saveProjectAs;
  }

  if (action == @selector(closeProject:)) {
    return g_callbacks.canCloseProject && g_callbacks.closeProject;
  }

  if (
    action == @selector(loadLayouts:) || action == @selector(saveLayouts:) || action == @selector(previousLayout:) ||
    action == @selector(nextLayout:) || action == @selector(selectLayout:)) {
    return g_callbacks.canUseLayouts;
  }

  if (action == @selector(selectActiveImage:)) {
    return g_callbacks.canAddImage && g_callbacks.setActiveImageIndex;
  }

  if (action == @selector(performMenuAction:)) {
    const auto menuAction = static_cast<MainMenuAction>([menuItem tag]);
    [menuItem
      setState:main_menu::actionChecked(g_callbacks, menuAction) ? NSControlStateValueOn : NSControlStateValueOff];
    return main_menu::actionEnabled(g_callbacks, menuAction);
  }

  return YES;
}
@end

namespace {
NSMenuItem* addTargetedMenuItem(
  NSMenu* menu,
  NSString* title,
  SEL action,
  NSString* keyEquivalent,
  NSEventModifierFlags modifierMask = NSEventModifierFlagCommand) {
  NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:keyEquivalent];
  [item setTarget:g_target];
  if ([keyEquivalent length] > 0) {
    [item setKeyEquivalentModifierMask:modifierMask];
  }
  [menu addItem:item];
  return item;
}

void setMenuItemSymbol(NSMenuItem* item, NSString* systemSymbolName) {
  NSImage* image = [NSImage imageWithSystemSymbolName:systemSymbolName accessibilityDescription:nil];
  [image setTemplate:YES];
  [item setImage:image];
}

NSMenuItem* addSymbolMenuItem(
  NSMenu* menu,
  NSString* title,
  SEL action,
  NSString* keyEquivalent,
  NSString* systemSymbolName,
  NSEventModifierFlags modifierMask = NSEventModifierFlagCommand) {
  NSMenuItem* item = addTargetedMenuItem(menu, title, action, keyEquivalent, modifierMask);
  setMenuItemSymbol(item, systemSymbolName);
  return item;
}

NSMenuItem* addAppMenuItem(NSMenu* menu, NSString* title, SEL action, NSString* keyEquivalent) {
  NSMenuItem* item = [menu addItemWithTitle:title action:action keyEquivalent:keyEquivalent];
  [item setTarget:NSApp];
  return item;
}

NSMenuItem* addActionMenuItem(
  NSMenu* menu,
  NSString* title,
  MainMenuAction action,
  NSString* keyEquivalent = @"",
  NSEventModifierFlags modifierMask = 0) {
  NSMenuItem* item = addTargetedMenuItem(menu, title, @selector(performMenuAction:), keyEquivalent, modifierMask);
  [item setTag:static_cast<NSInteger>(action)];
  return item;
}

NSMenuItem* addSymbolActionMenuItem(
  NSMenu* menu,
  NSString* title,
  MainMenuAction action,
  NSString* systemSymbolName,
  NSString* keyEquivalent = @"",
  NSEventModifierFlags modifierMask = 0) {
  NSMenuItem* item = addActionMenuItem(menu, title, action, keyEquivalent, modifierMask);
  setMenuItemSymbol(item, systemSymbolName);
  return item;
}

void addModeMenu(NSMenu* mainMenu) {
  NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"Mode" action:nil keyEquivalent:@""];
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Mode"];
  addSymbolActionMenuItem(menu, @"Pointer", MainMenuAction::SetModePointer, @"cursorarrow");
  addSymbolActionMenuItem(menu, @"Window / Level", MainMenuAction::SetModeWindowLevel, @"sun.max");
  addSymbolActionMenuItem(menu, @"Zoom", MainMenuAction::SetModeZoom, @"magnifyingglass");
  addSymbolActionMenuItem(menu, @"Pan / Dolly", MainMenuAction::SetModePan, @"hand.draw");
  addSymbolActionMenuItem(menu, @"Rotate View", MainMenuAction::SetModeRotateView, @"rotate.3d");
  addSymbolActionMenuItem(menu, @"Rotate Crosshairs", MainMenuAction::SetModeRotateCrosshairs, @"scope");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(menu, @"Brush", MainMenuAction::SetModeSegment, @"paintbrush");
  addSymbolActionMenuItem(menu, @"Annotate", MainMenuAction::SetModeAnnotate, @"pencil");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(
    menu,
    @"Translate Image",
    MainMenuAction::SetModeTranslateImage,
    @"arrow.up.and.down.and.arrow.left.and.right");
  addSymbolActionMenuItem(menu, @"Rotate Image", MainMenuAction::SetModeRotateImage, @"rotate.right");
  [menuItem setSubmenu:menu];
  [mainMenu addItem:menuItem];
}

void addImageMenu(NSMenu* mainMenu) {
  NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"Image" action:nil keyEquivalent:@""];
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Image"];
  addActionMenuItem(menu, @"Show Images Panel", MainMenuAction::ToggleImagesWindow);
  [menu addItem:[NSMenuItem separatorItem]];
  NSMenuItem* activeImageItem = [[NSMenuItem alloc] initWithTitle:@"Active Image" action:nil keyEquivalent:@""];
  g_activeImagesMenu = [[NSMenu alloc] initWithTitle:@"Active Image"];
  [activeImageItem setSubmenu:g_activeImagesMenu];
  [menu addItem:activeImageItem];
  addTargetedMenuItem(menu, @"Add Image(s)...", @selector(addImage:), @"");
  addTargetedMenuItem(menu, @"Add DICOM Series...", @selector(openDicomSeries:), @"");
  addActionMenuItem(menu, @"Export DICOM Series as Image...", MainMenuAction::ExportActiveImage);
  addActionMenuItem(menu, @"Remove Active Image", MainMenuAction::RemoveActiveImage);
  addActionMenuItem(menu, @"Set Image as Reference", MainMenuAction::SetActiveImageAsReference);
  [menu addItem:[NSMenuItem separatorItem]];
  addActionMenuItem(menu, @"Move Image Backward", MainMenuAction::MoveActiveImageBackward);
  addActionMenuItem(menu, @"Move Image Forward", MainMenuAction::MoveActiveImageForward);
  addActionMenuItem(menu, @"Move Image to Back", MainMenuAction::MoveActiveImageToBack);
  addActionMenuItem(menu, @"Move Image to Front", MainMenuAction::MoveActiveImageToFront);
  [menu addItem:[NSMenuItem separatorItem]];
  addActionMenuItem(menu, @"Lock Transformation", MainMenuAction::ToggleActiveImageTransformationLock);
  addActionMenuItem(menu, @"Reset Manual Transformation", MainMenuAction::ResetActiveImageManualTransformation);
  addActionMenuItem(menu, @"Save Manual Transformation...", MainMenuAction::SaveActiveImageManualTransformation);
  addActionMenuItem(
    menu,
    @"Save Initial + Manual Transformation...",
    MainMenuAction::SaveActiveImageInitialAndManualTransformation);
  [menuItem setSubmenu:menu];
  [mainMenu addItem:menuItem];
}

void addSegmentationMenu(NSMenu* mainMenu) {
  NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"Segmentation" action:nil keyEquivalent:@""];
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Segmentation"];
  addActionMenuItem(menu, @"Show Segmentations Panel", MainMenuAction::ToggleSegmentationsWindow);
  [menu addItem:[NSMenuItem separatorItem]];
  addTargetedMenuItem(menu, @"Add Segmentation...", @selector(addSegmentation:), @"");
  addActionMenuItem(menu, @"Create Blank Segmentation", MainMenuAction::CreateSegmentation);
  addActionMenuItem(menu, @"Save Active Segmentation...", MainMenuAction::SaveSegmentation);
  addActionMenuItem(menu, @"Clear Active Segmentation", MainMenuAction::ClearSegmentation);
  addActionMenuItem(menu, @"Remove Active Segmentation", MainMenuAction::RemoveSegmentation);
  [menu addItem:[NSMenuItem separatorItem]];
  addActionMenuItem(menu, @"Previous Foreground Label", MainMenuAction::PreviousForegroundLabel, @",");
  addActionMenuItem(menu, @"Next Foreground Label", MainMenuAction::NextForegroundLabel, @".");
  addActionMenuItem(menu, @"Previous Background Label", MainMenuAction::PreviousBackgroundLabel, @"<");
  addActionMenuItem(menu, @"Next Background Label", MainMenuAction::NextBackgroundLabel, @">");
  [menu addItem:[NSMenuItem separatorItem]];
  addActionMenuItem(menu, @"Decrease Brush Size", MainMenuAction::DecreaseBrushSize, @"-");
  addActionMenuItem(menu, @"Increase Brush Size", MainMenuAction::IncreaseBrushSize, @"+");
  [menuItem setSubmenu:menu];
  [mainMenu addItem:menuItem];
}

void addAnnotationMenu(NSMenu* mainMenu) {
  NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"Annotation" action:nil keyEquivalent:@""];
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Annotation"];
  addActionMenuItem(menu, @"Show Annotations Panel", MainMenuAction::ToggleAnnotationsWindow);
  [menu addItem:[NSMenuItem separatorItem]];
  addActionMenuItem(menu, @"Save All Annotations...", MainMenuAction::SaveAnnotations);
  addActionMenuItem(menu, @"Remove Active Annotation", MainMenuAction::RemoveAnnotation);
  [menu addItem:[NSMenuItem separatorItem]];
  addActionMenuItem(menu, @"Move Annotation Backward", MainMenuAction::MoveAnnotationBackward);
  addActionMenuItem(menu, @"Move Annotation Forward", MainMenuAction::MoveAnnotationForward);
  addActionMenuItem(menu, @"Move Annotation to Back", MainMenuAction::MoveAnnotationToBack);
  addActionMenuItem(menu, @"Move Annotation to Front", MainMenuAction::MoveAnnotationToFront);
  [menu addItem:[NSMenuItem separatorItem]];
  addActionMenuItem(menu, @"Paint Segmentation from Annotation", MainMenuAction::PaintSegmentationFromAnnotation);
  [menuItem setSubmenu:menu];
  [mainMenu addItem:menuItem];
}

void addLandmarkMenu(NSMenu* mainMenu) {
  NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"Landmarks" action:nil keyEquivalent:@""];
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Landmarks"];
  addActionMenuItem(menu, @"Show Landmarks Panel", MainMenuAction::ToggleLandmarksWindow);
  [menu addItem:[NSMenuItem separatorItem]];
  addActionMenuItem(menu, @"Create Landmark Group", MainMenuAction::CreateLandmarkGroup);
  addActionMenuItem(menu, @"Save Active Landmark Group...", MainMenuAction::SaveLandmarkGroup);
  addActionMenuItem(menu, @"Add Landmark at Crosshairs", MainMenuAction::AddLandmark);
  [menuItem setSubmenu:menu];
  [mainMenu addItem:menuItem];
}

void addViewsMenu(NSMenu* mainMenu) {
  NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"View" action:nil keyEquivalent:@""];
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"View"];
  addActionMenuItem(menu, @"Recenter", MainMenuAction::Recenter, @"c");
  addActionMenuItem(menu, @"Reset Views and Crosshairs", MainMenuAction::ResetView, @"c", NSEventModifierFlagShift);
  [menu addItem:[NSMenuItem separatorItem]];
  addActionMenuItem(menu, @"Show Image", MainMenuAction::ToggleImageVisibility, @"w");
  addActionMenuItem(menu, @"Show Segmentation", MainMenuAction::ToggleSegmentationVisibility, @"s");
  addActionMenuItem(menu, @"Show Image Edges", MainMenuAction::ToggleImageEdges, @"e", NSEventModifierFlagShift);
  addActionMenuItem(menu, @"Show Segmentation Outline", MainMenuAction::ToggleSegmentationOutline, @" ");
  [menu addItem:[NSMenuItem separatorItem]];
  addActionMenuItem(menu, @"Decrease Active Image Opacity", MainMenuAction::DecreaseActiveImageOpacity, @"q");
  addActionMenuItem(menu, @"Increase Active Image Opacity", MainMenuAction::IncreaseActiveImageOpacity, @"e");
  addActionMenuItem(menu, @"Decrease Segmentation Opacity", MainMenuAction::DecreaseSegmentationOpacity, @"a");
  addActionMenuItem(menu, @"Increase Segmentation Opacity", MainMenuAction::IncreaseSegmentationOpacity, @"d");
  [menu addItem:[NSMenuItem separatorItem]];
  addActionMenuItem(menu, @"Show Scale Bars", MainMenuAction::ToggleScaleBars);
  addActionMenuItem(menu, @"Cycle Overlays", MainMenuAction::ToggleOverlays, @"o");
  [menu addItem:[NSMenuItem separatorItem]];
  NSMenuItem* syncItem = [[NSMenuItem alloc] initWithTitle:@"Synchronize with ITK-SNAP" action:nil keyEquivalent:@""];
  [syncItem setIndentationLevel:0];
  NSMenu* syncMenu = [[NSMenu alloc] initWithTitle:@"Synchronize with ITK-SNAP"];
  addActionMenuItem(syncMenu, @"Enable Synchronization", MainMenuAction::ToggleSync);
  [syncMenu addItem:[NSMenuItem separatorItem]];
  addActionMenuItem(syncMenu, @"Cursor: Send", MainMenuAction::ToggleSyncSendCursor);
  addActionMenuItem(syncMenu, @"Cursor: Receive", MainMenuAction::ToggleSyncReceiveCursor);
  addActionMenuItem(syncMenu, @"Zoom: Send", MainMenuAction::ToggleSyncSendZoom);
  addActionMenuItem(syncMenu, @"Zoom: Receive", MainMenuAction::ToggleSyncReceiveZoom);
  addActionMenuItem(syncMenu, @"Pan: Send", MainMenuAction::ToggleSyncSendPan);
  addActionMenuItem(syncMenu, @"Pan: Receive", MainMenuAction::ToggleSyncReceivePan);
  [syncMenu addItem:[NSMenuItem separatorItem]];
  addActionMenuItem(syncMenu, @"Settings...", MainMenuAction::ShowSynchronizeSettingsWindow);
  [syncItem setSubmenu:syncMenu];
  [menu addItem:syncItem];
  [menuItem setSubmenu:menu];
  [mainMenu addItem:menuItem];
}

void addWindowsMenu(NSMenu* mainMenu) {
  NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Window"];
  addSymbolActionMenuItem(menu, @"Images Panel", MainMenuAction::ToggleImagesWindow, @"photo.stack");
  addSymbolActionMenuItem(
    menu,
    @"Segmentations Panel",
    MainMenuAction::ToggleSegmentationsWindow,
    @"square.3.layers.3d");
  addSymbolActionMenuItem(menu, @"Annotations Panel", MainMenuAction::ToggleAnnotationsWindow, @"pencil.and.outline");
  addSymbolActionMenuItem(menu, @"Landmarks Panel", MainMenuAction::ToggleLandmarksWindow, @"mappin.and.ellipse");
  addSymbolActionMenuItem(menu, @"Isosurfaces Panel", MainMenuAction::ToggleIsosurfacesWindow, @"cube.transparent");
  addSymbolActionMenuItem(menu, @"Inspector", MainMenuAction::ToggleInspectorWindow, @"info.circle");
  addSymbolActionMenuItem(menu, @"Opacity Mixer", MainMenuAction::ToggleOpacityMixerWindow, @"slider.horizontal.3");
  addSymbolActionMenuItem(menu, @"Settings", MainMenuAction::ToggleSettingsWindow, @"gearshape");
  [menu addItem:[NSMenuItem separatorItem]];
  addActionMenuItem(menu, @"Toolbar", MainMenuAction::ToggleToolbar);
#ifndef NDEBUG
  addActionMenuItem(menu, @"ImGui Demo", MainMenuAction::ToggleImGuiDemoWindow);
  addActionMenuItem(menu, @"ImPlot Demo", MainMenuAction::ToggleImPlotDemoWindow);
#endif
  [menuItem setSubmenu:menu];
  [mainMenu addItem:menuItem];
}

void addHelpMenu(NSMenu* mainMenu) {
  NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"Help" action:nil keyEquivalent:@""];
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Help"];
  addSymbolMenuItem(menu, @"About Entropy", @selector(showAbout:), @"", @"info.circle");
  [menuItem setSubmenu:menu];
  [mainMenu addItem:menuItem];
}

void installMacOSNativeMainMenu() {
  if (g_installed) {
    return;
  }

  [NSApplication sharedApplication];
  g_target = [[EntropyMacMenuTarget alloc] init];

  NSMenu* mainMenu = [[NSMenu alloc] initWithTitle:@"Entropy"];

  NSMenuItem* appMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
  NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@"Entropy"];
  addSymbolMenuItem(appMenu, @"About Entropy", @selector(showAbout:), @"", @"info.circle");
  addSymbolActionMenuItem(
    appMenu,
    @"Settings...",
    MainMenuAction::ToggleSettingsWindow,
    @"gearshape",
    @",",
    NSEventModifierFlagCommand);
  [appMenu addItem:[NSMenuItem separatorItem]];
  addAppMenuItem(appMenu, @"Hide Entropy", @selector(hide:), @"h");
  NSMenuItem* hideOthersItem = addAppMenuItem(appMenu, @"Hide Others", @selector(hideOtherApplications:), @"h");
  [hideOthersItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagOption];
  addAppMenuItem(appMenu, @"Show All", @selector(unhideAllApplications:), @"");
  [appMenu addItem:[NSMenuItem separatorItem]];
  addTargetedMenuItem(appMenu, @"Quit Entropy", @selector(quitApp:), @"q");
  [appMenuItem setSubmenu:appMenu];
  [mainMenu addItem:appMenuItem];

  NSMenuItem* fileMenuItem = [[NSMenuItem alloc] initWithTitle:@"File" action:nil keyEquivalent:@""];
  NSMenu* fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
  g_openImageItem =
    addSymbolMenuItem(fileMenu, @"Open Image(s)...", @selector(openImage:), @"o", @"photo.on.rectangle");
  g_openDicomSeriesItem =
    addSymbolMenuItem(fileMenu, @"Open DICOM Series...", @selector(openDicomSeries:), @"", @"externaldrive");
  g_openProjectItem = addSymbolMenuItem(
    fileMenu,
    @"Open Project...",
    @selector(openProject:),
    @"o",
    @"folder",
    NSEventModifierFlagCommand | NSEventModifierFlagShift);
  [fileMenu addItem:[NSMenuItem separatorItem]];
  g_addImageItem =
    addSymbolMenuItem(fileMenu, @"Add Image(s)...", @selector(addImage:), @"", @"plus.rectangle.on.rectangle");
  [fileMenu addItem:[NSMenuItem separatorItem]];
  g_saveProjectItem =
    addSymbolMenuItem(fileMenu, @"Save Project", @selector(saveProject:), @"s", @"square.and.arrow.down");
  g_saveProjectAsItem = addSymbolMenuItem(
    fileMenu,
    @"Save Project As...",
    @selector(saveProjectAs:),
    @"s",
    @"square.and.arrow.down.on.square",
    NSEventModifierFlagCommand | NSEventModifierFlagShift);
  [fileMenu addItem:[NSMenuItem separatorItem]];
  g_closeProjectItem = addSymbolMenuItem(fileMenu, @"Close Project", @selector(closeProject:), @"w", @"xmark.circle");
  [fileMenuItem setSubmenu:fileMenu];
  [mainMenu addItem:fileMenuItem];

  addModeMenu(mainMenu);
  addImageMenu(mainMenu);
  addSegmentationMenu(mainMenu);
  addAnnotationMenu(mainMenu);
  addLandmarkMenu(mainMenu);

  NSMenuItem* layoutsMenuItem = [[NSMenuItem alloc] initWithTitle:@"Layout" action:nil keyEquivalent:@""];
  g_layoutsMenu = [[NSMenu alloc] initWithTitle:@"Layout"];
  [layoutsMenuItem setSubmenu:g_layoutsMenu];
  [mainMenu addItem:layoutsMenuItem];

  addViewsMenu(mainMenu);
  addWindowsMenu(mainMenu);
  addHelpMenu(mainMenu);

  [NSApp setMainMenu:mainMenu];
  g_installed = true;
}

void rebuildLayoutsMenu() {
  if (!g_layoutsMenu) {
    return;
  }

  [g_layoutsMenu removeAllItems];
  addTargetedMenuItem(g_layoutsMenu, @"Load Layout...", @selector(loadLayouts:), @"");
  addTargetedMenuItem(g_layoutsMenu, @"Save Layout...", @selector(saveLayouts:), @"");
  [g_layoutsMenu addItem:[NSMenuItem separatorItem]];
  addActionMenuItem(g_layoutsMenu, @"Add Layout...", MainMenuAction::AddLayout);
  addActionMenuItem(g_layoutsMenu, @"Remove Current Layout", MainMenuAction::RemoveLayout);
  [g_layoutsMenu addItem:[NSMenuItem separatorItem]];
  addTargetedMenuItem(g_layoutsMenu, @"Previous", @selector(previousLayout:), @"[", 0);
  addTargetedMenuItem(g_layoutsMenu, @"Next", @selector(nextLayout:), @"]", 0);
  [g_layoutsMenu addItem:[NSMenuItem separatorItem]];

  const auto names = g_callbacks.layoutNames ? g_callbacks.layoutNames() : std::vector<std::string>{};
  const std::size_t currentIndex = g_callbacks.currentLayoutIndex ? g_callbacks.currentLayoutIndex() : 0;
  for (std::size_t i = 0; i < names.size(); ++i) {
    NSString* title = [NSString stringWithUTF8String:names.at(i).c_str()];
    NSMenuItem* item = addTargetedMenuItem(g_layoutsMenu, title, @selector(selectLayout:), @"", 0);
    [item setTag:static_cast<NSInteger>(i)];
    [item setState:(i == currentIndex) ? NSControlStateValueOn : NSControlStateValueOff];
  }
}

void rebuildActiveImagesMenu() {
  if (!g_activeImagesMenu) {
    return;
  }

  [g_activeImagesMenu removeAllItems];
  const auto names = g_callbacks.imageNames ? g_callbacks.imageNames() : std::vector<std::string>{};
  const std::size_t activeIndex = g_callbacks.activeImageIndex ? g_callbacks.activeImageIndex() : 0;
  for (std::size_t i = 0; i < names.size(); ++i) {
    NSString* title = [NSString stringWithUTF8String:names.at(i).c_str()];
    NSMenuItem* item = addTargetedMenuItem(g_activeImagesMenu, title, @selector(selectActiveImage:), @"", 0);
    [item setTag:static_cast<NSInteger>(i)];
    [item setState:(i == activeIndex) ? NSControlStateValueOn : NSControlStateValueOff];
  }
}
}  // namespace

void updateMacOSNativeMainMenu(const MainMenuBarCallbacks& callbacks) {
  g_callbacks = callbacks;
  installMacOSNativeMainMenu();
  rebuildLayoutsMenu();
  rebuildActiveImagesMenu();
}
