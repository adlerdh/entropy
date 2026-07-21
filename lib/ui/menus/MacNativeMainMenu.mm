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
NSMenu* g_openRecentMenu = nil;
NSMenu* g_activeImagesMenu = nil;
NSMenu* g_layoutsMenu = nil;
bool g_installed = false;

NSString* menuTitleForRecentPaths(const std::vector<std::filesystem::path>& paths) {
  if (paths.empty()) {
    return @"Untitled";
  }

  const std::string firstName =
    paths.front().filename().empty() ? paths.front().string() : paths.front().filename().string();
  if (paths.size() == 1) {
    return [NSString stringWithUTF8String:firstName.c_str()];
  }

  return [NSString stringWithFormat:@"%s (+%zu more)", firstName.c_str(), paths.size() - 1];
}

NSString* tooltipForRecentPaths(const std::vector<std::filesystem::path>& paths) {
  NSMutableString* tooltip = [NSMutableString string];
  for (const auto& path : paths) {
    if ([tooltip length] > 0) {
      [tooltip appendString:@"\n"];
    }
    [tooltip appendString:[NSString stringWithUTF8String:path.string().c_str()]];
  }
  return tooltip;
}

NSArray* representedObjectForRecentPaths(const std::vector<std::filesystem::path>& paths) {
  NSMutableArray* representedPaths = [NSMutableArray arrayWithCapacity:paths.size()];
  for (const auto& path : paths) {
    [representedPaths addObject:[NSString stringWithUTF8String:path.string().c_str()]];
  }
  return representedPaths;
}

bool representedRecentPathsExist(id representedObject) {
  if ([representedObject isKindOfClass:[NSString class]]) {
    NSString* path = (NSString*)representedObject;
    std::error_code error;
    return std::filesystem::exists(std::filesystem::path{[path UTF8String]}, error);
  }

  if ([representedObject isKindOfClass:[NSArray class]]) {
    NSArray* paths = (NSArray*)representedObject;
    for (NSString* path in paths) {
      std::error_code error;
      if (!std::filesystem::exists(std::filesystem::path{[path UTF8String]}, error)) {
        return false;
      }
    }
    return [paths count] > 0;
  }

  return false;
}
}  // namespace

@interface EntropyMacMenuTarget : NSObject
- (void)openImage:(id)sender;
- (void)openProject:(id)sender;
- (void)openDicomSeries:(id)sender;
- (void)addImage:(id)sender;
- (void)addDicomSeries:(id)sender;
- (void)openRecentProject:(id)sender;
- (void)openRecentImageGroup:(id)sender;
- (void)openRecentDicomGroup:(id)sender;
- (void)clearRecents:(id)sender;
- (void)addSegmentation:(id)sender;
- (void)loadInverseWarp:(id)sender;
- (void)loadForwardWarp:(id)sender;
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
- (void)showKeyboardShortcuts:(id)sender;
- (void)checkForUpdates:(id)sender;
- (void)openDownloadPage:(id)sender;
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

- (void)addDicomSeries:(id)sender {
  (void)sender;
  main_menu::addDicomSeries(g_callbacks);
}

- (void)openRecentProject:(id)sender {
  NSMenuItem* item = (NSMenuItem*)sender;
  NSString* path = (NSString*)[item representedObject];
  if (path && g_callbacks.openProjectFile) {
    g_callbacks.openProjectFile(std::filesystem::path{[path UTF8String]});
  }
}

- (void)openRecentImageGroup:(id)sender {
  NSMenuItem* item = (NSMenuItem*)sender;
  NSArray* paths = (NSArray*)[item representedObject];
  std::vector<std::filesystem::path> fileNames;
  for (NSString* path in paths) {
    fileNames.emplace_back([path UTF8String]);
  }
  if (!fileNames.empty() && g_callbacks.openImageFiles) {
    g_callbacks.openImageFiles(fileNames);
  }
}

- (void)openRecentDicomGroup:(id)sender {
  NSMenuItem* item = (NSMenuItem*)sender;
  NSArray* paths = (NSArray*)[item representedObject];
  std::vector<std::filesystem::path> folderNames;
  for (NSString* path in paths) {
    folderNames.emplace_back([path UTF8String]);
  }
  if (!folderNames.empty() && g_callbacks.openDicomFolders) {
    g_callbacks.openDicomFolders(folderNames);
  }
}

- (void)clearRecents:(id)sender {
  (void)sender;
  if (g_callbacks.clearRecents) {
    g_callbacks.clearRecents();
  }
}

- (void)addSegmentation:(id)sender {
  (void)sender;
  main_menu::addSegmentation(g_callbacks);
}

- (void)loadInverseWarp:(id)sender {
  (void)sender;
  main_menu::loadInverseWarpForActiveImage(g_callbacks);
}

- (void)loadForwardWarp:(id)sender {
  (void)sender;
  main_menu::loadForwardWarpForActiveImage(g_callbacks);
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

- (void)showKeyboardShortcuts:(id)sender {
  (void)sender;
  if (g_callbacks.showKeyboardShortcuts) {
    g_callbacks.showKeyboardShortcuts();
  }
}

- (void)checkForUpdates:(id)sender {
  (void)sender;
  if (g_callbacks.checkForUpdates) {
    g_callbacks.checkForUpdates();
  }
}

- (void)openDownloadPage:(id)sender {
  (void)sender;
  if (g_callbacks.openDownloadPage) {
    g_callbacks.openDownloadPage();
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

  if (action == @selector(addDicomSeries:)) {
    return g_callbacks.canAddImage && g_callbacks.openDicomFolders;
  }

  if (action == @selector(openRecentProject:)) {
    return g_callbacks.canOpenProject && g_callbacks.openProjectFile &&
           representedRecentPathsExist([menuItem representedObject]);
  }

  if (action == @selector(openRecentImageGroup:)) {
    return g_callbacks.canOpenProject && g_callbacks.openImageFiles &&
           representedRecentPathsExist([menuItem representedObject]);
  }

  if (action == @selector(openRecentDicomGroup:)) {
    return g_callbacks.canOpenProject && g_callbacks.openDicomFolders &&
           representedRecentPathsExist([menuItem representedObject]);
  }

  if (action == @selector(clearRecents:)) {
    return g_callbacks.clearRecents != nullptr;
  }

  if (action == @selector(addSegmentation:)) {
    return g_callbacks.canAddSegmentation && g_callbacks.addSegmentationFile;
  }

  if (action == @selector(loadInverseWarp:)) {
    return g_callbacks.canLoadDeformationFieldForActiveImage && g_callbacks.loadInverseWarpForActiveImage;
  }

  if (action == @selector(loadForwardWarp:)) {
    return g_callbacks.canLoadDeformationFieldForActiveImage && g_callbacks.loadForwardWarpForActiveImage;
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

void addDisabledMenuItem(NSMenu* menu, NSString* title) {
  NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];
  [item setEnabled:NO];
  [menu addItem:item];
}

void addRecentProjectItems(NSMenu* menu, const std::vector<std::filesystem::path>& paths) {
  if (paths.empty()) {
    addDisabledMenuItem(menu, @"None");
    return;
  }

  for (const auto& path : paths) {
    const std::vector<std::filesystem::path> pathGroup{path};
    NSMenuItem* item =
      addTargetedMenuItem(menu, menuTitleForRecentPaths(pathGroup), @selector(openRecentProject:), @"", 0);
    [item setRepresentedObject:[NSString stringWithUTF8String:path.string().c_str()]];
    [item setToolTip:tooltipForRecentPaths(pathGroup)];
  }
}

void addRecentGroupItems(NSMenu* menu, const std::vector<std::vector<std::filesystem::path>>& groups, SEL action) {
  if (groups.empty()) {
    addDisabledMenuItem(menu, @"None");
    return;
  }

  for (const auto& group : groups) {
    NSMenuItem* item = addTargetedMenuItem(menu, menuTitleForRecentPaths(group), action, @"", 0);
    [item setRepresentedObject:representedObjectForRecentPaths(group)];
    [item setToolTip:tooltipForRecentPaths(group)];
  }
}

void rebuildOpenRecentMenu() {
  if (!g_openRecentMenu) {
    return;
  }

  [g_openRecentMenu removeAllItems];

  const auto projects =
    g_callbacks.recentProjectFiles ? g_callbacks.recentProjectFiles() : std::vector<std::filesystem::path>{};
  const auto imageGroups =
    g_callbacks.recentImageGroups ? g_callbacks.recentImageGroups() : std::vector<std::vector<std::filesystem::path>>{};
  const auto dicomGroups =
    g_callbacks.recentDicomGroups ? g_callbacks.recentDicomGroups() : std::vector<std::vector<std::filesystem::path>>{};

  std::vector<std::vector<std::filesystem::path>> singleImages;
  std::vector<std::vector<std::filesystem::path>> multiImageGroups;
  for (const auto& group : imageGroups) {
    if (group.empty()) {
      continue;
    }
    if (group.size() == 1) {
      singleImages.push_back(group);
    } else {
      multiImageGroups.push_back(group);
    }
  }

  NSMenuItem* projectsItem = [[NSMenuItem alloc] initWithTitle:@"Projects" action:nil keyEquivalent:@""];
  NSMenu* projectsMenu = [[NSMenu alloc] initWithTitle:@"Projects"];
  addRecentProjectItems(projectsMenu, projects);
  [projectsItem setSubmenu:projectsMenu];
  [g_openRecentMenu addItem:projectsItem];

  NSMenuItem* imagesItem = [[NSMenuItem alloc] initWithTitle:@"Images" action:nil keyEquivalent:@""];
  NSMenu* imagesMenu = [[NSMenu alloc] initWithTitle:@"Images"];
  addRecentGroupItems(imagesMenu, singleImages, @selector(openRecentImageGroup:));
  [imagesItem setSubmenu:imagesMenu];
  [g_openRecentMenu addItem:imagesItem];

  NSMenuItem* imageGroupsItem = [[NSMenuItem alloc] initWithTitle:@"Image Groups" action:nil keyEquivalent:@""];
  NSMenu* imageGroupsMenu = [[NSMenu alloc] initWithTitle:@"Image Groups"];
  addRecentGroupItems(imageGroupsMenu, multiImageGroups, @selector(openRecentImageGroup:));
  [imageGroupsItem setSubmenu:imageGroupsMenu];
  [g_openRecentMenu addItem:imageGroupsItem];

  NSMenuItem* dicomItem = [[NSMenuItem alloc] initWithTitle:@"DICOM Series" action:nil keyEquivalent:@""];
  NSMenu* dicomMenu = [[NSMenu alloc] initWithTitle:@"DICOM Series"];
  addRecentGroupItems(dicomMenu, dicomGroups, @selector(openRecentDicomGroup:));
  [dicomItem setSubmenu:dicomMenu];
  [g_openRecentMenu addItem:dicomItem];

  [g_openRecentMenu addItem:[NSMenuItem separatorItem]];
  addTargetedMenuItem(g_openRecentMenu, @"Clear Recents", @selector(clearRecents:), @"", 0);
}

void addModeMenu(NSMenu* mainMenu) {
  NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"Mode" action:nil keyEquivalent:@""];
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Mode"];
  addSymbolActionMenuItem(menu, @"Pointer", MainMenuAction::SetModePointer, @"cursorarrow", @"v");
  addSymbolActionMenuItem(menu, @"Window / Level", MainMenuAction::SetModeWindowLevel, @"sun.max", @"l");
  addSymbolActionMenuItem(menu, @"Zoom", MainMenuAction::SetModeZoom, @"magnifyingglass", @"z");
  addSymbolActionMenuItem(menu, @"Pan / Dolly", MainMenuAction::SetModePan, @"hand.draw", @"p");
  addSymbolActionMenuItem(menu, @"Rotate View", MainMenuAction::SetModeRotateView, @"rotate.3d");
  addSymbolActionMenuItem(menu, @"Rotate Crosshairs", MainMenuAction::SetModeRotateCrosshairs, @"scope");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(menu, @"Segmentation Brush", MainMenuAction::SetModeSegment, @"paintbrush", @"b");
  addSymbolActionMenuItem(menu, @"Vector Annotate", MainMenuAction::SetModeAnnotate, @"pencil");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(
    menu,
    @"Translate Image",
    MainMenuAction::SetModeTranslateImage,
    @"arrow.up.and.down.and.arrow.left.and.right",
    @"t");
  addSymbolActionMenuItem(menu, @"Rotate Image", MainMenuAction::SetModeRotateImage, @"rotate.right", @"r");
  addSymbolActionMenuItem(
    menu,
    @"Scale Image",
    MainMenuAction::SetModeScaleImage,
    @"arrow.up.right.and.arrow.down.left",
    @"y");
  [menuItem setSubmenu:menu];
  [mainMenu addItem:menuItem];
}

void addImageMenu(NSMenu* mainMenu) {
  NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"Image" action:nil keyEquivalent:@""];
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Image"];
  addSymbolActionMenuItem(menu, @"Images Panel", MainMenuAction::ToggleImagesWindow, @"slider.horizontal.3");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolMenuItem(menu, @"Add Image(s)...", @selector(addImage:), @"", @"plus.rectangle.on.rectangle");
  addSymbolMenuItem(menu, @"Add DICOM Series...", @selector(addDicomSeries:), @"", @"externaldrive.badge.plus");
  addSymbolActionMenuItem(
    menu,
    @"Export DICOM Series as Image...",
    MainMenuAction::ExportActiveImage,
    @"square.and.arrow.up");
  [menu addItem:[NSMenuItem separatorItem]];
  NSMenuItem* activeImageItem = [[NSMenuItem alloc] initWithTitle:@"Select Active Image" action:nil keyEquivalent:@""];
  setMenuItemSymbol(activeImageItem, @"photo.stack");
  g_activeImagesMenu = [[NSMenu alloc] initWithTitle:@"Select Active Image"];
  [activeImageItem setSubmenu:g_activeImagesMenu];
  [menu addItem:activeImageItem];
  addSymbolActionMenuItem(menu, @"Remove Active Image", MainMenuAction::RemoveActiveImage, @"xmark");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(menu, @"Set Image as Reference", MainMenuAction::SetActiveImageAsReference, @"scope");
  addSymbolActionMenuItem(
    menu,
    @"Lock Manual Affine Transformation",
    MainMenuAction::ToggleActiveImageTransformationLock,
    @"lock");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(menu, @"Move Image Backward", MainMenuAction::MoveActiveImageBackward, @"backward");
  addSymbolActionMenuItem(menu, @"Move Image Forward", MainMenuAction::MoveActiveImageForward, @"forward");
  addSymbolActionMenuItem(menu, @"Move Image to Back", MainMenuAction::MoveActiveImageToBack, @"backward.end");
  addSymbolActionMenuItem(menu, @"Move Image to Front", MainMenuAction::MoveActiveImageToFront, @"forward.end");
  [menu addItem:[NSMenuItem separatorItem]];
  NSMenuItem* timeSeriesItem = [[NSMenuItem alloc] initWithTitle:@"Time Series" action:nil keyEquivalent:@""];
  setMenuItemSymbol(timeSeriesItem, @"clock");
  NSMenu* timeSeriesMenu = [[NSMenu alloc] initWithTitle:@"Time Series"];
  addSymbolActionMenuItem(timeSeriesMenu, @"Show Time Controls", MainMenuAction::ToggleGlobalTimeControls, @"clock");
  [timeSeriesMenu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(timeSeriesMenu, @"Play / Pause", MainMenuAction::ToggleTimePlayback, @"playpause");
  [timeSeriesMenu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(timeSeriesMenu, @"First Frame", MainMenuAction::FirstTimePoint, @"backward.end");
  addSymbolActionMenuItem(
    timeSeriesMenu,
    @"Previous Frame",
    MainMenuAction::PreviousTimePoint,
    @"backward",
    @",",
    NSEventModifierFlagOption);
  addSymbolActionMenuItem(
    timeSeriesMenu,
    @"Next Frame",
    MainMenuAction::NextTimePoint,
    @"forward",
    @".",
    NSEventModifierFlagOption);
  addSymbolActionMenuItem(timeSeriesMenu, @"Last Frame", MainMenuAction::LastTimePoint, @"forward.end");
  [timeSeriesItem setSubmenu:timeSeriesMenu];
  [menu addItem:timeSeriesItem];
  NSMenuItem* isosurfacesItem = [[NSMenuItem alloc] initWithTitle:@"Isosurfaces" action:nil keyEquivalent:@""];
  setMenuItemSymbol(isosurfacesItem, @"cube.transparent");
  NSMenu* isosurfacesMenu = [[NSMenu alloc] initWithTitle:@"Isosurfaces"];
  addSymbolActionMenuItem(
    isosurfacesMenu,
    @"Isosurfaces Panel",
    MainMenuAction::ToggleIsosurfacesWindow,
    @"cube.transparent");
  [isosurfacesMenu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(isosurfacesMenu, @"Add Isosurface...", MainMenuAction::AddIsosurface, @"plus");
  addSymbolActionMenuItem(
    isosurfacesMenu,
    @"Add Isosurface Range...",
    MainMenuAction::AddIsosurfaceRange,
    @"plus.rectangle.on.rectangle");
  addSymbolActionMenuItem(isosurfacesMenu, @"Show Isosurfaces", MainMenuAction::ToggleActiveImageIsosurfaces, @"eye");
  [isosurfacesItem setSubmenu:isosurfacesMenu];
  [menu addItem:isosurfacesItem];
  [menu addItem:[NSMenuItem separatorItem]];
  NSMenuItem* affineItem = [[NSMenuItem alloc] initWithTitle:@"Affine Transformations" action:nil keyEquivalent:@""];
  setMenuItemSymbol(affineItem, @"arrow.up.left.and.arrow.down.right");
  NSMenu* affineMenu = [[NSMenu alloc] initWithTitle:@"Affine Transformations"];
  addSymbolActionMenuItem(
    affineMenu,
    @"Load Initial Affine...",
    MainMenuAction::LoadActiveImageInitialTransformation,
    @"square.and.arrow.down");
  addSymbolActionMenuItem(
    affineMenu,
    @"Save Initial Affine...",
    MainMenuAction::SaveActiveImageInitialTransformation,
    @"square.and.arrow.up");
  addSymbolActionMenuItem(
    affineMenu,
    @"Reset Initial Affine",
    MainMenuAction::ResetActiveImageInitialTransformation,
    @"arrow.counterclockwise");
  [affineMenu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(
    affineMenu,
    @"Save Manual Affine...",
    MainMenuAction::SaveActiveImageManualTransformation,
    @"square.and.arrow.up");
  addSymbolActionMenuItem(
    affineMenu,
    @"Reset Manual Affine",
    MainMenuAction::ResetActiveImageManualTransformation,
    @"arrow.counterclockwise");
  [affineMenu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(
    affineMenu,
    @"Save Effective (Manual * Initial) Affine...",
    MainMenuAction::SaveActiveImageEffectiveTransformation,
    @"square.and.arrow.up.on.square");
  [affineItem setSubmenu:affineMenu];
  [menu addItem:affineItem];
  NSMenuItem* deformationItem = [[NSMenuItem alloc] initWithTitle:@"Deformable Transformations"
                                                           action:nil
                                                    keyEquivalent:@""];
  setMenuItemSymbol(deformationItem, @"waveform.path.ecg");
  NSMenu* deformationMenu = [[NSMenu alloc] initWithTitle:@"Deformable Transformations"];
  addSymbolMenuItem(
    deformationMenu,
    @"Load Inverse Warp Field...",
    @selector(loadInverseWarp:),
    @"",
    @"arrow.down.doc");
  addSymbolMenuItem(
    deformationMenu,
    @"Load Forward Warp Field...",
    @selector(loadForwardWarp:),
    @"",
    @"arrow.down.doc");
  addSymbolActionMenuItem(
    deformationMenu,
    @"Apply Deformable Warp",
    MainMenuAction::ToggleApplyActiveImageWarp,
    @"waveform.path.ecg");
  [deformationItem setSubmenu:deformationMenu];
  [menu addItem:deformationItem];
  NSMenuItem* registrationItem = [[NSMenuItem alloc] initWithTitle:@"Image Registration" action:nil keyEquivalent:@""];
  setMenuItemSymbol(registrationItem, @"arrow.triangle.2.circlepath");
  NSMenu* registrationMenu = [[NSMenu alloc] initWithTitle:@"Image Registration"];
  addSymbolActionMenuItem(
    registrationMenu,
    @"Registration...",
    MainMenuAction::ShowRegistrationSetupWindow,
    @"arrow.triangle.2.circlepath");
  addSymbolActionMenuItem(
    registrationMenu,
    @"Image Registration Jobs...",
    MainMenuAction::ToggleRegistrationJobsWindow,
    @"list.bullet.rectangle");
  [registrationItem setSubmenu:registrationMenu];
  [menu addItem:registrationItem];
  [menuItem setSubmenu:menu];
  [mainMenu addItem:menuItem];
}

void addSegmentationMenu(NSMenu* mainMenu) {
  NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"Segmentation" action:nil keyEquivalent:@""];
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Segmentation"];
  addSymbolActionMenuItem(menu, @"Segmentations Panel", MainMenuAction::ToggleSegmentationsWindow, @"star");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolMenuItem(menu, @"Add Segmentation...", @selector(addSegmentation:), @"", @"folder.badge.plus");
  addSymbolActionMenuItem(menu, @"Create Blank Segmentation", MainMenuAction::CreateSegmentation, @"doc");
  addSymbolActionMenuItem(
    menu,
    @"Save Active Segmentation...",
    MainMenuAction::SaveSegmentation,
    @"square.and.arrow.down");
  addSymbolActionMenuItem(menu, @"Clear Active Segmentation", MainMenuAction::ClearSegmentation, @"eraser");
  addSymbolActionMenuItem(menu, @"Remove Active Segmentation", MainMenuAction::RemoveSegmentation, @"trash");
  [menu addItem:[NSMenuItem separatorItem]];
  addActionMenuItem(menu, @"Previous Foreground Label", MainMenuAction::PreviousForegroundLabel, @",");
  addActionMenuItem(menu, @"Next Foreground Label", MainMenuAction::NextForegroundLabel, @".");
  addActionMenuItem(menu, @"Previous Background Label", MainMenuAction::PreviousBackgroundLabel, @"<");
  addActionMenuItem(menu, @"Next Background Label", MainMenuAction::NextBackgroundLabel, @">");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(
    menu,
    @"Decrease Brush Size",
    MainMenuAction::DecreaseBrushSize,
    @"smallcircle.filled.circle",
    @"-");
  addSymbolActionMenuItem(
    menu,
    @"Increase Brush Size",
    MainMenuAction::IncreaseBrushSize,
    @"largecircle.fill.circle",
    @"+");
  [menuItem setSubmenu:menu];
  [mainMenu addItem:menuItem];
}

void addAnnotationMenu(NSMenu* mainMenu) {
  NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"Annotation" action:nil keyEquivalent:@""];
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Annotation"];
  addSymbolActionMenuItem(menu, @"Annotations Panel", MainMenuAction::ToggleAnnotationsWindow, @"pencil.and.outline");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(menu, @"Import Annotations...", MainMenuAction::ImportAnnotations, @"square.and.arrow.down");
  addSymbolActionMenuItem(menu, @"Export Annotations...", MainMenuAction::ExportAnnotations, @"square.and.arrow.up");
  addSymbolActionMenuItem(menu, @"Remove Active Annotation", MainMenuAction::RemoveAnnotation, @"trash");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(menu, @"Move Annotation Backward", MainMenuAction::MoveAnnotationBackward, @"backward");
  addSymbolActionMenuItem(menu, @"Move Annotation Forward", MainMenuAction::MoveAnnotationForward, @"forward");
  addSymbolActionMenuItem(menu, @"Move Annotation to Back", MainMenuAction::MoveAnnotationToBack, @"backward.end");
  addSymbolActionMenuItem(menu, @"Move Annotation to Front", MainMenuAction::MoveAnnotationToFront, @"forward.end");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(
    menu,
    @"Paint Segmentation from Annotation",
    MainMenuAction::PaintSegmentationFromAnnotation,
    @"paintbucket");
  [menuItem setSubmenu:menu];
  [mainMenu addItem:menuItem];
}

void addLandmarkMenu(NSMenu* mainMenu) {
  NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"Landmarks" action:nil keyEquivalent:@""];
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Landmarks"];
  addSymbolActionMenuItem(menu, @"Landmarks Panel", MainMenuAction::ToggleLandmarksWindow, @"mappin.and.ellipse");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(menu, @"Create Landmark Group", MainMenuAction::CreateLandmarkGroup, @"plus");
  addSymbolActionMenuItem(menu, @"Remove Active Group", MainMenuAction::RemoveLandmarkGroup, @"trash");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(menu, @"Import Landmarks...", MainMenuAction::ImportLandmarkGroup, @"square.and.arrow.down");
  addSymbolActionMenuItem(menu, @"Export Landmarks...", MainMenuAction::SaveLandmarkGroup, @"square.and.arrow.up");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(menu, @"Add Landmark at Crosshairs", MainMenuAction::AddLandmark, @"mappin.and.ellipse");
  [menuItem setSubmenu:menu];
  [mainMenu addItem:menuItem];
}

void addViewsMenu(NSMenu* mainMenu) {
  NSMenuItem* menuItem = [[NSMenuItem alloc] initWithTitle:@"View" action:nil keyEquivalent:@""];
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"View"];
  addSymbolActionMenuItem(menu, @"Recenter Views", MainMenuAction::Recenter, @"scope", @"c");
  addSymbolActionMenuItem(
    menu,
    @"Reset Views and Crosshairs",
    MainMenuAction::ResetView,
    @"arrow.counterclockwise",
    @"c",
    NSEventModifierFlagShift);
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(menu, @"Show Image", MainMenuAction::ToggleImageVisibility, @"eye", @"w");
  addSymbolActionMenuItem(
    menu,
    @"Show Image Edges",
    MainMenuAction::ToggleImageEdges,
    @"waveform.path.ecg",
    @"e",
    NSEventModifierFlagShift);
  addSymbolActionMenuItem(menu, @"Show Segmentation", MainMenuAction::ToggleSegmentationVisibility, @"star.fill", @"s");
  addSymbolActionMenuItem(menu, @"Show Segmentation Outline", MainMenuAction::ToggleSegmentationOutline, @"star", @" ");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(
    menu,
    @"Decrease Active Image Opacity",
    MainMenuAction::DecreaseActiveImageOpacity,
    @"circle.lefthalf.filled",
    @"q");
  addSymbolActionMenuItem(
    menu,
    @"Increase Active Image Opacity",
    MainMenuAction::IncreaseActiveImageOpacity,
    @"circle.fill",
    @"e");
  addSymbolActionMenuItem(
    menu,
    @"Decrease Segmentation Opacity",
    MainMenuAction::DecreaseSegmentationOpacity,
    @"circle.lefthalf.filled",
    @"a");
  addSymbolActionMenuItem(
    menu,
    @"Increase Segmentation Opacity",
    MainMenuAction::IncreaseSegmentationOpacity,
    @"circle.fill",
    @"d");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(menu, @"Show Crosshairs", MainMenuAction::ToggleCrosshairs, @"plus.viewfinder", @"x");
  addSymbolActionMenuItem(menu, @"Snap Crosshairs to Voxels", MainMenuAction::ToggleCrosshairsVoxelSnapping, @"scope");
  addSymbolActionMenuItem(menu, @"Show Scale Bars", MainMenuAction::ToggleScaleBars, @"ruler");
  addSymbolActionMenuItem(menu, @"Show User Interface", MainMenuAction::ToggleUserInterface, @"sidebar.left", @"u");
  addSymbolActionMenuItem(menu, @"Cycle View Overlays", MainMenuAction::CycleViewOverlays, @"square.2.layers.3d", @"o");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(menu, @"ASCII Rendering", MainMenuAction::ToggleAsciiRendering, @"textformat");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(
    menu,
    @"Synchronize Entropy Instances",
    MainMenuAction::ToggleEntropyInstanceSync,
    @"dot.radiowaves.left.and.right");
  NSMenuItem* syncItem = [[NSMenuItem alloc] initWithTitle:@"Synchronize with ITK-SNAP" action:nil keyEquivalent:@""];
  [syncItem setIndentationLevel:0];
  setMenuItemSymbol(syncItem, @"dot.radiowaves.left.and.right");
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
  [menu addItem:[NSMenuItem separatorItem]];
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
  addSymbolActionMenuItem(
    menu,
    @"Registration Panel",
    MainMenuAction::ShowRegistrationSetupWindow,
    @"arrow.up.left.and.arrow.down.right");
  addSymbolActionMenuItem(menu, @"Annotations Panel", MainMenuAction::ToggleAnnotationsWindow, @"pencil.and.outline");
  addSymbolActionMenuItem(menu, @"Landmarks Panel", MainMenuAction::ToggleLandmarksWindow, @"mappin.and.ellipse");
  addSymbolActionMenuItem(menu, @"Isosurfaces Panel", MainMenuAction::ToggleIsosurfacesWindow, @"cube.transparent");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(menu, @"Voxel Inspector", MainMenuAction::ToggleInspectorWindow, @"info.circle", @"i");
  addSymbolActionMenuItem(menu, @"Opacity Mixer", MainMenuAction::ToggleOpacityMixerWindow, @"slider.horizontal.3");
  addSymbolActionMenuItem(
    menu,
    @"Registration Jobs",
    MainMenuAction::ToggleRegistrationJobsWindow,
    @"list.bullet.rectangle");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(
    menu,
    @"Application Settings",
    MainMenuAction::ToggleSettingsWindow,
    @"gearshape",
    @",",
    NSEventModifierFlagCommand);
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(menu, @"Reset Panel Layout", MainMenuAction::ResetPanelLayout, @"rectangle.3.group");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(menu, @"Toolbar", MainMenuAction::ToggleToolbar, @"wrench.and.screwdriver");
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
  addSymbolMenuItem(menu, @"Check for Updates...", @selector(checkForUpdates:), @"", @"arrow.triangle.2.circlepath");
  addSymbolMenuItem(menu, @"Download Entropy...", @selector(openDownloadPage:), @"", @"square.and.arrow.down");
  [menu addItem:[NSMenuItem separatorItem]];
  addSymbolMenuItem(menu, @"Keyboard Shortcuts", @selector(showKeyboardShortcuts:), @"", @"keyboard");
  [menu addItem:[NSMenuItem separatorItem]];
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
    @"Application Settings...",
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
  g_openProjectItem = addSymbolMenuItem(
    fileMenu,
    @"Open Project...",
    @selector(openProject:),
    @"o",
    @"folder",
    NSEventModifierFlagCommand | NSEventModifierFlagShift);
  g_openDicomSeriesItem =
    addSymbolMenuItem(fileMenu, @"Open DICOM Series...", @selector(openDicomSeries:), @"", @"externaldrive");
  g_openRecentMenu = [[NSMenu alloc] initWithTitle:@"Open Recent"];
  NSMenuItem* openRecentItem = [[NSMenuItem alloc] initWithTitle:@"Open Recent" action:nil keyEquivalent:@""];
  setMenuItemSymbol(openRecentItem, @"clock.arrow.circlepath");
  [openRecentItem setSubmenu:g_openRecentMenu];
  [fileMenu addItem:openRecentItem];
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
  addSymbolActionMenuItem(
    fileMenu,
    @"Reset Project Settings...",
    MainMenuAction::ResetProjectSettings,
    @"arrow.counterclockwise");
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
  addSymbolMenuItem(g_layoutsMenu, @"Load Layouts...", @selector(loadLayouts:), @"", @"folder");
  addSymbolMenuItem(g_layoutsMenu, @"Save Layouts...", @selector(saveLayouts:), @"", @"square.and.arrow.down");
  [g_layoutsMenu addItem:[NSMenuItem separatorItem]];
  addSymbolActionMenuItem(g_layoutsMenu, @"Add Layout...", MainMenuAction::AddLayout, @"plus");
  addSymbolActionMenuItem(g_layoutsMenu, @"Remove Current Layout", MainMenuAction::RemoveLayout, @"trash");
  addSymbolActionMenuItem(
    g_layoutsMenu,
    @"Show Layout Tabs",
    MainMenuAction::ToggleLayoutTabs,
    @"rectangle.topthird.inset.filled");
  [g_layoutsMenu addItem:[NSMenuItem separatorItem]];
  addSymbolMenuItem(g_layoutsMenu, @"Previous Layout", @selector(previousLayout:), @"[", @"arrow.left.square", 0);
  addSymbolMenuItem(g_layoutsMenu, @"Next Layout", @selector(nextLayout:), @"]", @"arrow.right.square", 0);
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
  if (!names.empty()) {
    [g_activeImagesMenu addItem:[NSMenuItem separatorItem]];
  }
  addSymbolActionMenuItem(
    g_activeImagesMenu,
    @"Activate Previous Image",
    MainMenuAction::ActivatePreviousImage,
    @"arrow.left.square",
    @"[",
    NSEventModifierFlagShift);
  addSymbolActionMenuItem(
    g_activeImagesMenu,
    @"Activate Next Image",
    MainMenuAction::ActivateNextImage,
    @"arrow.right.square",
    @"]",
    NSEventModifierFlagShift);
}
}  // namespace

void updateMacOSNativeMainMenu(const MainMenuBarCallbacks& callbacks) {
  g_callbacks = callbacks;
  installMacOSNativeMainMenu();
  rebuildOpenRecentMenu();
  rebuildLayoutsMenu();
  rebuildActiveImagesMenu();
}
