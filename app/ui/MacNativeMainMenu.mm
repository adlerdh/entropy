#include "ui/MacNativeMainMenu.h"

#import <Cocoa/Cocoa.h>

@class EntropyMacMenuTarget;

namespace {
MainMenuBarCallbacks g_callbacks;
EntropyMacMenuTarget* g_target = nil;
NSMenuItem* g_openImageItem = nil;
NSMenuItem* g_openProjectItem = nil;
NSMenuItem* g_addImageItem = nil;
NSMenuItem* g_addSegmentationItem = nil;
NSMenuItem* g_saveProjectItem = nil;
NSMenuItem* g_saveProjectAsItem = nil;
NSMenuItem* g_closeProjectItem = nil;
NSMenu* g_layoutsMenu = nil;
bool g_installed = false;
}  // namespace

@interface EntropyMacMenuTarget : NSObject
- (void)openImage:(id)sender;
- (void)openProject:(id)sender;
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
- (void)showAbout:(id)sender;
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

- (void)showAbout:(id)sender {
  (void)sender;
  if (g_callbacks.showAbout) {
    g_callbacks.showAbout();
  }
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  const SEL action = [menuItem action];

  if (action == @selector(openImage:)) {
    return g_callbacks.canOpenProject && g_callbacks.openImageFile;
  }

  if (action == @selector(openProject:)) {
    return g_callbacks.canOpenProject && g_callbacks.openProjectFile;
  }

  if (action == @selector(addImage:)) {
    return g_callbacks.canAddImage && g_callbacks.addImageFile;
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

NSMenuItem* addAppMenuItem(NSMenu* menu, NSString* title, SEL action, NSString* keyEquivalent) {
  NSMenuItem* item = [menu addItemWithTitle:title action:action keyEquivalent:keyEquivalent];
  [item setTarget:NSApp];
  return item;
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
  addTargetedMenuItem(appMenu, @"About Entropy", @selector(showAbout:), @"");
  [appMenu addItem:[NSMenuItem separatorItem]];
  addAppMenuItem(appMenu, @"Hide Entropy", @selector(hide:), @"h");
  NSMenuItem* hideOthersItem = addAppMenuItem(appMenu, @"Hide Others", @selector(hideOtherApplications:), @"h");
  [hideOthersItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand | NSEventModifierFlagOption];
  addAppMenuItem(appMenu, @"Show All", @selector(unhideAllApplications:), @"");
  [appMenu addItem:[NSMenuItem separatorItem]];
  addAppMenuItem(appMenu, @"Quit Entropy", @selector(terminate:), @"q");
  [appMenuItem setSubmenu:appMenu];
  [mainMenu addItem:appMenuItem];

  NSMenuItem* fileMenuItem = [[NSMenuItem alloc] initWithTitle:@"File" action:nil keyEquivalent:@""];
  NSMenu* fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
  g_openImageItem = addTargetedMenuItem(fileMenu, @"Open Image...", @selector(openImage:), @"o");
  g_openProjectItem = addTargetedMenuItem(
    fileMenu,
    @"Open Project...",
    @selector(openProject:),
    @"o",
    NSEventModifierFlagCommand | NSEventModifierFlagShift);
  [fileMenu addItem:[NSMenuItem separatorItem]];
  g_addImageItem = addTargetedMenuItem(fileMenu, @"Add Image...", @selector(addImage:), @"");
  g_addSegmentationItem = addTargetedMenuItem(fileMenu, @"Add Segmentation...", @selector(addSegmentation:), @"");
  [fileMenu addItem:[NSMenuItem separatorItem]];
  g_saveProjectItem = addTargetedMenuItem(fileMenu, @"Save Project", @selector(saveProject:), @"s");
  g_saveProjectAsItem = addTargetedMenuItem(
    fileMenu,
    @"Save Project As...",
    @selector(saveProjectAs:),
    @"s",
    NSEventModifierFlagCommand | NSEventModifierFlagShift);
  [fileMenu addItem:[NSMenuItem separatorItem]];
  g_closeProjectItem = addTargetedMenuItem(fileMenu, @"Close Project", @selector(closeProject:), @"w");
  [fileMenuItem setSubmenu:fileMenu];
  [mainMenu addItem:fileMenuItem];

  NSMenuItem* layoutsMenuItem = [[NSMenuItem alloc] initWithTitle:@"Layouts" action:nil keyEquivalent:@""];
  g_layoutsMenu = [[NSMenu alloc] initWithTitle:@"Layouts"];
  [layoutsMenuItem setSubmenu:g_layoutsMenu];
  [mainMenu addItem:layoutsMenuItem];

  [NSApp setMainMenu:mainMenu];
  g_installed = true;
}

void rebuildLayoutsMenu() {
  if (!g_layoutsMenu) {
    return;
  }

  [g_layoutsMenu removeAllItems];
  addTargetedMenuItem(g_layoutsMenu, @"Load...", @selector(loadLayouts:), @"");
  addTargetedMenuItem(g_layoutsMenu, @"Save...", @selector(saveLayouts:), @"");
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
}  // namespace

void updateMacOSNativeMainMenu(const MainMenuBarCallbacks& callbacks) {
  g_callbacks = callbacks;
  installMacOSNativeMainMenu();
  rebuildLayoutsMenu();
}
