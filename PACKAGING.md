# Packaging the Entropy Medical Image Viewer
This guide gives the release packaging commands for Entropy. For general build setup, supported platforms, and CMake option details, see [BUILDING.md](BUILDING.md).

Entropy packages are created with CPack from the release app build. By default, generated packages and CPack scratch files are written under the active build tree:
```text
build-release/packages/
```

The output directory is controlled by `Entropy_PACKAGE_OUTPUT_DIR` and can be changed when configuring the app stage:
```sh
cmake --preset app-release -DEntropy_PACKAGE_OUTPUT_DIR=/tmp/entropy-packages
```

## Release Flow
Use this sequence for normal release packages on Linux, macOS, and Windows:

```sh
cmake --preset superbuild-release
cmake --build --preset superbuild-release --parallel

cmake --preset app-release
cmake --build --preset app-release --parallel

cmake --build --preset package-release --parallel
```

The `package-release` preset builds CMake's `package` target, which runs CPack using `build-release/CPackConfig.cmake`. The final package appears under `build-release/packages/` unless `Entropy_PACKAGE_OUTPUT_DIR` was overridden.

If the release app is already configured and built, you can run CPack directly:
```sh
cpack --config build-release/CPackConfig.cmake
```

## GitHub Tag Releases

Public GitHub Releases are created by `.github/workflows/release.yml`. The release workflow runs only when a tag matching `v*.*.*.*` is pushed. It verifies that the tag exactly matches the version in `CMakeLists.txt`, then builds the Windows release package, runs release tests, smoke-tests a staged install, creates a GitHub Release, and uploads the MSI and portable ZIP as release assets.

Before creating a release tag, update these values in `CMakeLists.txt`:

```cmake
set(VERSION_MAJOR 0)
set(VERSION_MINOR 9)
set(VERSION_FEATURE 5)
set(VERSION_PATCH 0)
```

After the version bump has been reviewed and merged, create a signed tag if you have GPG signing configured:

```sh
git switch main
git pull --ff-only
git tag -s v0.9.5.0 -m "Entropy 0.9.5.0"
git push origin v0.9.5.0
```

If signed tags are not configured, use an annotated tag:

```sh
git tag -a v0.9.5.0 -m "Entropy 0.9.5.0"
git push origin v0.9.5.0
```

The tag name must match `VERSION_FULL` exactly. For example, `VERSION_MAJOR=0`, `VERSION_MINOR=9`, `VERSION_FEATURE=5`, and `VERSION_PATCH=0` require the tag `v0.9.5.0`. If the tag and CMake version disagree, the release workflow fails before building packages.

The generated GitHub Release assets are named like:

```text
Entropy-0.9.5.0-Windows-x86_64.msi
Entropy-0.9.5.0-Windows-x86_64-portable.zip
```

The same files are also uploaded as workflow artifacts for debugging. The GitHub Release assets are the public download files; avoid storing large binaries directly in GitHub Pages. If a project website needs download links, point the website to the GitHub Release or its latest-release page.

## Linux Packages
On Linux, CPack creates both a Debian package and a portable tarball named like:
```text
build-release/packages/Entropy-x.y.z.w-Linux-x86_64.deb
build-release/packages/Entropy-x.y.z.w-Linux-x86_64-portable.tar.gz
```

To force a single generator when running CPack directly:
```sh
cpack -G DEB --config build-release/CPackConfig.cmake
cpack -G TGZ --config build-release/CPackConfig.cmake
```

### Linux Package Layout
The DEB installs the runtime payload under `/usr`, while the portable tarball contains the same payload under a top-level archive directory:
```text
bin/entropy
lib/entropy/libQt6Core.so*
share/applications/io.github.adlerdh.entropy.desktop
share/icons/hicolor/<size>x<size>/apps/io.github.adlerdh.entropy.png
```

ITK, GLFW, Native File Dialog Extended, spdlog, and small helper libraries are linked statically into the executable by default on Linux. Qt Core remains dynamic and is bundled under `lib/entropy`. The About dialog icon is embedded into the executable with CMakeRC instead of installed as a loose file. The installed executable uses this RPATH:
```text
$ORIGIN/../lib/entropy
```

The app resolves `share/entropy` relative to the executable first, so both `/usr/bin/entropy` from the DEB and `bin/entropy` from the portable tarball can find packaged resources. The install step checks bundled private libraries and fails if any copied library still contains the build directory in its RPATH or RUNPATH.

### Linux Runtime Dependencies
The DEB uses `dpkg-shlibdeps` to detect non-bundled system library dependencies. It also recommends `xdg-desktop-portal` plus a common portal backend:
```text
xdg-desktop-portal, xdg-desktop-portal-gtk | xdg-desktop-portal-kde | xdg-desktop-portal-gnome
```

Native File Dialog Extended uses the portal backend on Linux, so a working desktop portal is needed for native file dialogs.

### Ubuntu Compatibility
Linux binary compatibility is determined by the system used to build the package. A package built on a newer host may require newer `libc6`, `libstdc++6`, or compiler runtime packages than older Ubuntu releases provide.

If Ubuntu 22.04 compatibility is required, build the release package on Ubuntu 22.04 or inside a real 22.04 container, VM, or sysroot with matching compilers and runtime libraries. A CMake toolchain file alone cannot make a binary built against a newer host glibc compatible with Ubuntu 22.04.

### Test a Linux Package
Inspect the DEB:
```sh
dpkg-deb --info build-release/packages/Entropy-x.y.z.w-Linux-x86_64.deb
dpkg-deb --contents build-release/packages/Entropy-x.y.z.w-Linux-x86_64.deb
```

Install locally and launch:
```sh
sudo apt install ./build-release/packages/Entropy-x.y.z.w-Linux-x86_64.deb
entropy --help
entropy
```

If you are testing a rebuilt package with the same version number as the package already installed, force apt to replace the installed files:
```sh
sudo apt install --reinstall ./build-release/packages/Entropy-x.y.z.w-Linux-x86_64.deb
```

Also test the desktop launcher and at least one open/save file dialog on a clean graphical Ubuntu system.

For a staged install check without installing system-wide:
```sh
cmake --install build-release --prefix build-release/linux-package-install
readelf -d build-release/linux-package-install/bin/entropy
ldd build-release/linux-package-install/bin/entropy
build-release/linux-package-install/bin/entropy --help
```

## macOS DMG Package
On macOS, Entropy is built as an `.app` bundle. CPack creates a drag-and-drop DMG named like:
```text
build-release/packages/Entropy-x.y.z.w-macOS.dmg
```

To force the DragNDrop generator when running CPack directly:
```sh
cpack -G DragNDrop --config build-release/CPackConfig.cmake
```

### macOS Bundle Fixup And Signing
The install and CPack steps copy required third-party dynamic libraries into:
```text
Entropy.app/Contents/Frameworks
```

CMake's bundle fixup rewrites library paths so the app does not depend on the build tree.

By default, `Entropy_MACOS_CODESIGN_IDENTITY` is `-`, which creates an ad-hoc signature. This is useful for local testing after bundle fixup. For a public release, configure the release app with a Developer ID Application identity:
```sh
cmake --preset app-release -DEntropy_MACOS_CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
cmake --build --preset package-release --parallel
```

The package step does not notarize the app. Public macOS distribution still needs Developer ID signing and Apple notarization outside this CMake packaging step.

### Test a macOS Package
Test the build-tree app:
```sh
open build-release/bin/Entropy.app
```

Test a staged install:
```sh
cmake --install build-release --prefix build-release/install
open build-release/install/Entropy.app
```

Test the DMG by opening it, dragging `Entropy.app` to `/Applications`, and launching from Finder or Launchpad.

Finder-launched macOS builds write logs under:
```text
~/Library/Logs/Entropy
```

and UI state under:
```text
~/Library/Application Support/Entropy
```

Terminal launches keep the development defaults used by the command-line app.

## Package Size Notes
Release packages default to `Entropy_STATIC_BUNDLED_DEPENDENCIES=ON`, which links ITK, GLFW, Native File Dialog Extended, spdlog, and small helper libraries statically into the executable. Qt Core stays dynamic and is bundled privately so the app does not depend on a system-provided Qt version.

Release packages should be built from `build-release`, not `build-default`. A `RelWithDebInfo` dependency tree can include debug information in copied private libraries and produce much larger packages. The release superbuild avoids that by building dependencies with `CMAKE_BUILD_TYPE=Release`.

Changing `Entropy_STATIC_BUNDLED_DEPENDENCIES` changes how third-party dependencies are built. Reconfigure and rebuild the superbuild after changing it; do not reuse a dependency tree that was previously built with the opposite static/shared setting.

## Windows Packages
On Windows, CPack creates an MSI installer and a portable ZIP archive named like:
```text
build-release/packages/Entropy-x.y.z.w-Windows-x86_64.msi
build-release/packages/Entropy-x.y.z.w-Windows-x86_64-portable.zip
```

The Windows MSI package uses CPack's WiX generator. With CMake 3.28, use WiX Toolset v3 (`candle.exe` and `light.exe`). The portable ZIP uses CPack's archive generator and does not need WiX when generated by itself.

### Windows Packaging Environment
Required tools:
```text
CMake 3.28 or newer
Visual Studio 2022 C++ build tools
PowerShell
WiX Toolset v3.14.1
```

Build the release dependency tree and app first:
```powershell
cmake --preset superbuild-release
cmake --build --preset superbuild-release --parallel

cmake --preset app-release
cmake --build --preset app-release --parallel
```

CPack needs WiX v3 tools when generating the MSI. Use one of these setup options.

**Option 1:** install WiX globally with winget. This may require administrator privileges and may also require the Windows `.NET Framework 3.5` feature:

```powershell
winget install --id WiXToolset.WiXToolset --version 3.14.1.8722 `
  --accept-package-agreements `
  --accept-source-agreements
```

After installation, open a new PowerShell session and verify:

```powershell
Get-Command candle.exe
Get-Command light.exe
```

**Option 2:** keep WiX local to the build tree. This does not require installing WiX globally. CMake will automatically use `build-release\tools\wix\tools` when that directory contains `candle.exe` and `light.exe`:

```powershell
New-Item -ItemType Directory -Force build-release\tools | Out-Null
curl.exe -L --retry 3 --retry-delay 2 --fail `
  -o build-release\tools\wix.3.14.1.nupkg `
  https://www.nuget.org/api/v2/package/wix/3.14.1
Copy-Item build-release\tools\wix.3.14.1.nupkg build-release\tools\wix.3.14.1.zip -Force
Expand-Archive build-release\tools\wix.3.14.1.zip build-release\tools\wix -Force
cmake --preset app-release
```

Verify the local WiX tools:
```powershell
Get-Command build-release\tools\wix\tools\candle.exe
Get-Command build-release\tools\wix\tools\light.exe
```

### Create a Windows Package
After WiX is available and the app stage has been configured, run CPack directly to create both Windows artifacts:
```powershell
cpack -C Release --config build-release\CPackConfig.cmake
```

To create only the portable ZIP archive, WiX is not required:
```powershell
cpack -G ZIP -C Release --config build-release\CPackConfig.cmake
```

The Windows release presets set `Entropy_STATIC_BUNDLED_DEPENDENCIES=ON`, so ITK, GLFW, Native File Dialog Extended, spdlog, and small helper libraries are linked statically into `entropy.exe`. Qt Core and the Visual C++ runtime keep the normal Windows dynamic linkage and are bundled as required private runtime DLLs. `Entropy_WINDOWS_PACKAGE_STATIC_SMALL_DEPS=ON` remains enabled for Windows release builds, but `Entropy_STATIC_BUNDLED_DEPENDENCIES=ON` is the setting that makes ITK static.

The Windows MSI includes `entropy.exe`, required private runtime DLLs, the Visual C++ runtime DLLs, the app icon metadata, Start Menu and desktop shortcuts, package documents, and the FireANTs Python bridge. The About dialog icon is embedded into the executable with CMakeRC instead of installed under `share\entropy`.

The portable ZIP contains the same runtime payload without installer metadata or shortcuts. Extract it to a writable folder and run `entropy.exe` from that folder.

Windows logs and UI state are written under:
```text
%LOCALAPPDATA%\Entropy
%LOCALAPPDATA%\Entropy\Logs
```

### Test a Windows Package
Test a staged install without installing the MSI system-wide:
```powershell
cmake --install build-release --config Release --prefix build-release\windows-package-install
build-release\windows-package-install\entropy.exe --help
```

Then install the MSI on a clean Windows system, launch Entropy from the Start Menu shortcut, verify open/save dialogs, and uninstall it from Apps > Installed apps or Programs and Features. Also extract the portable ZIP on a clean Windows system and run `entropy.exe --help` and `entropy.exe` from the extracted folder.

The current MSI is unsigned. For public distribution, sign the MSI with a trusted code-signing certificate after CPack creates it. MSIX is a good future path when package identity, signing, and update requirements are ready, but MSI is the current CPack-native installer path for this project.
