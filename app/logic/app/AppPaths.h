#pragma once

#include <filesystem>

namespace app_paths
{

void configureFromCommandLine(int argc, char* argv[]);

bool isRunningFromMacOSAppBundle();
bool usesPlatformUserDirectories();

std::filesystem::path resourceDirectory();
std::filesystem::path logDirectory();
std::filesystem::path userDataDirectory();

} // namespace app_paths
