# Packaging Entropy

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

Use this sequence for normal release packages on Linux and macOS:

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

## Linux DEB Package

On Linux, CPack creates a Debian package named like:

```text
build-release/packages/Entropy-x.y.z.w-Linux-x86_64.deb
```

To force the DEB generator when running CPack directly:

```sh
cpack -G DEB --config build-release/CPackConfig.cmake
```

### Linux Package Layout

The DEB installs:

```text
/usr/bin/Entropy
/usr/lib/entropy/*.so*
/usr/share/applications/io.github.adlerdh.entropy.desktop
/usr/share/entropy/AboutEntropyIcon.png
/usr/share/icons/hicolor/<size>x<size>/apps/io.github.adlerdh.entropy.png
```

Entropy's private CMake-built shared libraries are bundled under `/usr/lib/entropy`. The installed executable uses this RPATH:

```text
$ORIGIN/../lib/entropy
```

The install step checks bundled private libraries and fails if any copied library still contains the build directory in its RPATH or RUNPATH.

### Linux Runtime Dependencies

The DEB uses `dpkg-shlibdeps` to detect non-bundled system library dependencies. It also recommends `xdg-desktop-portal` plus a common portal backend:

```text
xdg-desktop-portal, xdg-desktop-portal-gtk | xdg-desktop-portal-kde | xdg-desktop-portal-gnome
```

Native File Dialog Extended uses the portal backend on Linux, so a working desktop portal is needed for native file dialogs.

### Ubuntu Compatibility

Linux binary compatibility is determined by the system used to build the package. A package built on a newer host may require newer `libc6`, `libstdc++6`, or compiler runtime packages than older Ubuntu releases provide.

If Ubuntu 22.04 compatibility is required, build the release package on Ubuntu 22.04 or inside a real 22.04 container, VM, or sysroot with matching compilers and runtime libraries. A CMake toolchain file alone cannot make a binary built against a newer host glibc compatible with Ubuntu 22.04.

### Test A Linux Package

Inspect the DEB:

```sh
dpkg-deb --info build-release/packages/Entropy-x.y.z.w-Linux-x86_64.deb
dpkg-deb --contents build-release/packages/Entropy-x.y.z.w-Linux-x86_64.deb
```

Install locally and launch:

```sh
sudo apt install ./build-release/packages/Entropy-x.y.z.w-Linux-x86_64.deb
Entropy --help
Entropy
```

If you are testing a rebuilt package with the same version number as the package already installed, force apt to replace the installed files:

```sh
sudo apt install --reinstall ./build-release/packages/Entropy-x.y.z.w-Linux-x86_64.deb
```

Also test the desktop launcher and at least one open/save file dialog on a clean graphical Ubuntu system.

For a staged install check without installing system-wide:

```sh
cmake --install build-release --prefix build-release/linux-package-install
readelf -d build-release/linux-package-install/bin/Entropy
ldd build-release/linux-package-install/bin/Entropy
build-release/linux-package-install/bin/Entropy --help
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

By default, `entropy_MACOS_CODESIGN_IDENTITY` is `-`, which creates an ad-hoc signature. This is useful for local testing after bundle fixup. For a public release, configure the release app with a Developer ID Application identity:

```sh
cmake --preset app-release -Dentropy_MACOS_CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
cmake --build --preset package-release --parallel
```

The package step does not notarize the app. Public macOS distribution still needs Developer ID signing and Apple notarization outside this CMake packaging step.

### Test A macOS Package

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

The Linux package bundles private shared libraries from GLFW, Native File Dialog Extended, QtBase, spdlog, and ITK so the installed app does not depend on build-tree paths or distro-provided versions of those libraries.

Release packages should be built from `build-release`, not `build-default`. A `RelWithDebInfo` dependency tree can include debug information in copied private libraries and produce much larger packages. The release superbuild avoids that by building dependencies with `CMAKE_BUILD_TYPE=Release`.
