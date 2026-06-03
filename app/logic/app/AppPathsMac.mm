#include "logic/app/AppPaths.h"

#import <Foundation/Foundation.h>

#include <atomic>
#include <filesystem>
#include <string_view>

namespace {

namespace fs = std::filesystem;

std::atomic_bool g_hasLaunchServicesProcessSerialNumber{false};

NSString* appName() {
  return @"Entropy";
}

fs::path pathFromURL(NSURL* url) {
  if (url == nil) {
    return {};
  }

  NSString* path = [url path];
  return path == nil ? fs::path{} : fs::path{[path fileSystemRepresentation]};
}

fs::path userDirectory(NSSearchPathDirectory directory) {
  NSError* error = nil;
  NSURL* url = [[NSFileManager defaultManager] URLForDirectory:directory
                                                      inDomain:NSUserDomainMask
                                             appropriateForURL:nil
                                                        create:NO
                                                         error:&error];
  return pathFromURL(url);
}

bool hasLaunchServicesWorkingDirectory() {
  std::error_code error;
  const fs::path cwd = fs::current_path(error);
  return error || cwd == fs::path{"/"};
}

}  // namespace

namespace app_paths {

void configureFromCommandLine(int argc, char* argv[]) {
  g_hasLaunchServicesProcessSerialNumber = false;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg{argv[i]};
    if (arg.starts_with("-psn_")) {
      g_hasLaunchServicesProcessSerialNumber = true;
      return;
    }
  }
}

bool isRunningFromMacOSAppBundle() {
  NSURL* bundleURL = [[NSBundle mainBundle] bundleURL];
  if (bundleURL == nil) {
    return false;
  }

  NSString* extension = [bundleURL pathExtension];
  return extension != nil && [extension caseInsensitiveCompare:@"app"] == NSOrderedSame;
}

bool usesPlatformUserDirectories() {
  return isRunningFromMacOSAppBundle() &&
         (g_hasLaunchServicesProcessSerialNumber || hasLaunchServicesWorkingDirectory());
}

std::filesystem::path logDirectory() {
  if (usesPlatformUserDirectories()) {
    if (const fs::path libraryDir = userDirectory(NSLibraryDirectory); !libraryDir.empty()) {
      return libraryDir / "Logs" / [appName() fileSystemRepresentation];
    }
  }

  return "log";
}

std::filesystem::path userDataDirectory() {
  if (usesPlatformUserDirectories()) {
    if (const fs::path appSupportDir = userDirectory(NSApplicationSupportDirectory); !appSupportDir.empty()) {
      return appSupportDir / [appName() fileSystemRepresentation];
    }
  }

  return ".";
}

}  // namespace app_paths
