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
  addAppMenuItem(appMenu, @"About Entropy", @selector(orderFrontStandardAboutPanel:), @"");
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

  [NSApp setMainMenu:mainMenu];
  g_installed = true;
}
}  // namespace

void updateMacOSNativeMainMenu(const MainMenuBarCallbacks& callbacks) {
  g_callbacks = callbacks;
  installMacOSNativeMainMenu();
}
