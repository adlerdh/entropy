# Packaging Entropy

This guide covers local release packaging and GitHub release behavior. For compiler requirements, dependency builds,
tests, and general source build instructions, see [BUILDING.md](BUILDING.md).

Entropy packages are created with [CPack](https://cmake.org/cmake/help/latest/module/CPack.html) from the release app build and written to `build-release/packages/`.

All packages include the runtime application plus `README.md`, `LICENSE.txt`, `NOTICE.txt`, and
`THIRD_PARTY_NOTICES.md`.

Release package builds link bundled third-party libraries statically where practical. Qt and platform system libraries
remain dynamic and are bundled or referenced according to the platform package format.

Portable ZIP/TAR.GZ archives contain the same runtime application as installable packages, without installer metadata or
shortcuts.

## Release Package Flow

Use this sequence for local release packages on Linux, macOS, and Windows:

```sh
cmake --preset deps-release
cmake --build --preset deps-release --parallel

cmake --preset app-release
cmake --build --preset app-release --parallel

cmake --build --preset package-release --parallel
```

The `package-release` preset runs CMake's `package` target, which calls CPack with `build-release/CPackConfig.cmake`.

If the release app is already configured and built, CPack can be run directly:

```sh
# Single-config builds, such as Make or Ninja
cpack --config build-release/CPackConfig.cmake

# Multi-config builds, such as Visual Studio
cpack -C Release --config build-release/CPackConfig.cmake
```

## Package Options

Pass these when configuring the app stage with `cmake --preset app-release -DNAME=value`.

| Option | Default | Purpose |
| --- | --- | --- |
| `Entropy_PACKAGE_OUTPUT_DIR` | `build-release/packages` | Directory where CPack writes packages |
| `Entropy_PACKAGE_ARCHITECTURE` | detected from host | Architecture label used in package filenames |
| `Entropy_STATIC_BUNDLED_DEPENDENCIES` | `ON` in release presets | Links bundled dependencies statically where practical; Qt and system libraries stay dynamic |
| `Entropy_LINUX_PACKAGE_PLATFORM_LABEL` | detected from `/etc/os-release` | Linux platform label in package filenames |
| `Entropy_LINUX_CPACK_GENERATORS` | `DEB;TGZ` | Linux package generators; use `RPM;TGZ` for Fedora |
| `Entropy_MACOS_CODESIGN_IDENTITY` | `-` | macOS signing identity; `-` means ad-hoc signing, empty skips signing |
| `Entropy_STRIP_PACKAGED_APP` | `ON` | Strips local symbols from the installed macOS app bundle before signing |
| `Entropy_WIX_ROOT` | empty | Optional explicit path to [WiX Toolset](https://wixtoolset.org/) v3 tools; normally discovered from the build tree, `PATH`, or `WIX` |

Changing dependency linkage options such as `Entropy_STATIC_BUNDLED_DEPENDENCIES` requires rebuilding the dependency
stage. Do not reuse a dependency tree built with different linkage settings.

## Linux Packages

Linux packages should be built on the oldest supported target distribution. A binary built on a newer Linux host may
require newer `glibc`, `libstdc++`, or compiler runtime packages than older distributions provide.

Official Linux CI release builds currently produce Ubuntu 22.04 DEB/TAR.GZ packages and Fedora 43 RPM/TAR.GZ packages.
CI sets explicit labels for release artifacts so public download names stay stable.

Required tools:

- Ubuntu DEB packages: `dpkg-dev`, `fakeroot`, and `file`
- Fedora RPM packages: `rpm-build` and `file`

By default, CMake derives the Linux platform label from `/etc/os-release`, such as `Ubuntu-22.04` or `Fedora-43`.
Override `Entropy_LINUX_PACKAGE_PLATFORM_LABEL` when a specific release label is needed.

The default Linux generators are `DEB;TGZ`. On an Ubuntu 22.04 host, the default local package names are:

```text
Entropy-x.y.z.w-Ubuntu-22.04-x86_64.deb
Entropy-x.y.z.w-Ubuntu-22.04-x86_64-portable.tar.gz
```

On Fedora, configure the app stage with RPM output before packaging:

```sh
cmake --preset app-release "-DEntropy_LINUX_CPACK_GENERATORS=RPM;TGZ"
cmake --build --preset package-release --parallel
```

On a Fedora 43 host, the package names are:

```text
Entropy-x.y.z.w-Fedora-43-x86_64.rpm
Entropy-x.y.z.w-Fedora-43-x86_64-portable.tar.gz
```

The DEB and RPM install under `/usr`.

[Native File Dialog Extended](https://github.com/btzy/nativefiledialog-extended) uses the Linux desktop portal backend, so native file dialogs need a working `xdg-desktop-portal` service. This service is the desktop-neutral interface that lets applications ask the user's desktop environment to show file picker dialogs.

To create a specific Linux package type:

```sh
cpack -G DEB --config build-release/CPackConfig.cmake
cpack -G RPM --config build-release/CPackConfig.cmake
cpack -G TGZ --config build-release/CPackConfig.cmake
```

Test a Linux package:

```sh
dpkg-deb --info build-release/packages/Entropy-x.y.z.w-Ubuntu-22.04-x86_64.deb
dpkg-deb --contents build-release/packages/Entropy-x.y.z.w-Ubuntu-22.04-x86_64.deb
sudo apt install ./build-release/packages/Entropy-x.y.z.w-Ubuntu-22.04-x86_64.deb
entropy
```

Use `apt install ./<package>`, not `dpkg -i`, so dependencies are resolved automatically.

Test a staged Linux install without installing system-wide:

```sh
cmake --install build-release --prefix build-release/linux-package-install
build-release/linux-package-install/bin/entropy
```

## macOS Packages

On macOS, Entropy is built as an `.app` bundle.

Required tools:

- Xcode or Xcode Command Line Tools
- `codesign` for signed packages

CPack creates a drag-and-drop DMG and a portable ZIP:

```text
Entropy-x.y.z.w-macOS-arm64.dmg
Entropy-x.y.z.w-macOS-arm64.zip
Entropy-x.y.z.w-macOS-x86_64.dmg
Entropy-x.y.z.w-macOS-x86_64.zip
```

The arm64 and x86_64 packages are built separately. Entropy does not publish a universal macOS binary.

To force the DMG generator:

```sh
cpack -G DragNDrop --config build-release/CPackConfig.cmake
```

By default, local macOS packages use ad-hoc signing:

```sh
Entropy_MACOS_CODESIGN_IDENTITY=-
```

*TODO:* For public distribution, we will configure the app stage with a Developer ID Application identity and notarize
the final DMG or app outside the CMake package step:

```sh
cmake --preset app-release -DEntropy_MACOS_CODESIGN_IDENTITY="Developer ID Application: <developer-name> (<team-id>)"
cmake --build --preset package-release --parallel
```

Test a macOS package:

```sh
open build-release/bin/Entropy.app
cmake --install build-release --prefix build-release/macos-package-install
open build-release/macos-package-install/Entropy.app
```

Also open the DMG, drag `Entropy.app` to `/Applications`, and launch from Finder.

## Windows Packages

Required tools:

- Visual Studio 2022 C++ build tools
- PowerShell
- WiX Toolset v3.14.1 for MSI packages

The portable ZIP does not require WiX. The MSI uses CPack's WiX generator and needs `candle.exe` and `light.exe`.

On Windows, CPack creates an MSI installer and a portable ZIP:

```text
Entropy-x.y.z.w-Windows-x86_64.msi
Entropy-x.y.z.w-Windows-x86_64-portable.zip
```

Install WiX globally:

```powershell
winget install --id WiXToolset.WiXToolset --version 3.14.1.8722 `
  --accept-package-agreements `
  --accept-source-agreements
```

Or keep WiX local to the build tree. CMake automatically checks `build-release\tools\wix\tools`:

```powershell
New-Item -ItemType Directory -Force build-release\tools | Out-Null
curl.exe -L --retry 3 --retry-delay 2 --fail `
  -o build-release\tools\wix.3.14.1.nupkg `
  https://www.nuget.org/api/v2/package/wix/3.14.1
Copy-Item build-release\tools\wix.3.14.1.nupkg build-release\tools\wix.3.14.1.zip -Force
Expand-Archive build-release\tools\wix.3.14.1.zip build-release\tools\wix -Force
cmake --preset app-release
```

Create Windows packages:

```powershell
cpack -C Release --config build-release\CPackConfig.cmake
```

Create only the portable ZIP:

```powershell
cpack -G ZIP -C Release --config build-release\CPackConfig.cmake
```

Test a staged Windows install:

```powershell
cmake --install build-release --config Release --prefix build-release\windows-package-install
build-release\windows-package-install\entropy.exe
```

Also install the MSI on a clean Windows system, launch Entropy from the Start Menu, verify open/save dialogs, uninstall
it, and test the portable ZIP from an extracted folder.

*TODO:* The current MSI is unsigned. For public distribution, we will sign the MSI with a trusted code-signing
certificate after CPack creates it.

## GitHub Releases

Public GitHub Releases are created by [.github/workflows/release.yml](.github/workflows/release.yml). The workflow runs
when a tag matching `v*.*.*.*` is pushed.

Before tagging a release, update the versions in `CMakeLists.txt`:

```cmake
set(VERSION_MAJOR x)
set(VERSION_MINOR y)
set(VERSION_FEATURE z)
set(VERSION_PATCH w)
```

Create an annotated or signed tag that exactly matches `VERSION_FULL`:

```sh
git switch main
git pull --ff-only
git tag -a vx.y.z.w -m "Entropy x.y.z.w"
git push origin vx.y.z.w
```

If the tag and CMake version disagree, the release workflow fails before building packages.

The release workflow builds and uploads:

- macOS DMGs and portable archives for `arm64` and `x86_64`
- Windows MSI and portable ZIP for `x86_64`
- Ubuntu DEB and portable tarball for `x86_64`
- Fedora RPM and portable tarball for `x86_64`
- Source archives as `.zip` and `.tar.gz`

Release notes are generated by the workflow and include a downloads section plus GitHub generated notes.
