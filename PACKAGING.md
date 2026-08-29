# Packaging Entropy

This guide covers local release packaging and GitHub release behavior. For compiler requirements, dependency builds,
tests, and general source build instructions, see [BUILDING.md](BUILDING.md).

Entropy packages are created with [CPack](https://cmake.org/cmake/help/latest/module/CPack.html) from the Release
application build and written to `build-release/packages/`.

Run all commands in this guide from the repository root unless stated otherwise.

All packages include the runtime application plus `README.md`, `LICENSE.txt`, `NOTICE.txt`, and
`THIRD_PARTY_NOTICES.md`.

Release package builds link bundled third-party libraries statically where practical. Qt and platform system libraries
remain dynamic and are bundled or referenced according to the platform package format.

Portable ZIP and TAR.GZ archives contain the same runtime application as installable packages without installer
metadata or shortcuts.

## Build Packages Locally

Use this sequence for local release packages on Linux, macOS, and Windows:

```sh
cmake --preset deps-release
cmake --build --preset deps-release --parallel

cmake --preset app-release
cmake --build --preset app-release --parallel

ctest --test-dir build-release -C Release --parallel --output-on-failure

cmake --build --preset package-release --parallel
```

The last command runs CPack and writes the packages to `build-release/packages/`. Do not distribute a package unless
the Release tests pass.

If the release app is already configured and built, CPack can be run directly:

```sh
# Single-config builds, such as Make or Ninja
cpack --config build-release/CPackConfig.cmake

# Multi-config builds, such as Visual Studio
cpack -C Release --config build-release/CPackConfig.cmake
```

## Linux Packages

Linux packages should be built on the oldest supported target distribution. A binary built on a newer Linux host may
require newer `glibc`, `libstdc++`, or compiler runtime packages than older distributions provide.

Official Linux CI release builds currently produce Ubuntu 22.04 DEB/TAR.GZ packages and Fedora 43 RPM/TAR.GZ packages.
CI sets explicit labels for release artifacts so public download names stay stable.

Required tools:

- Ubuntu DEB packages: `binutils`, `dpkg-dev`, `fakeroot`, and `file`
- Fedora RPM packages: `binutils`, `rpm-build`, and `file`

Install them before configuring the application stage:

```sh
# Ubuntu
sudo apt-get install --no-install-recommends -y binutils dpkg-dev fakeroot file

# Fedora
sudo dnf install -y binutils rpm-build file
```

The packaging configuration requires `readelf`, which is provided by `binutils`.

By default, CMake derives the Linux platform label from `/etc/os-release`, such as `Ubuntu-22.04` or `Fedora-43`.
Override `Entropy_LINUX_PACKAGE_PLATFORM_LABEL` when a specific release label is needed.

The default Linux generators are DEB and TGZ. On an Ubuntu 22.04 x86_64 build, the default package names are:

```text
Entropy-x.y.z.w-Ubuntu-22.04-x86_64.deb
Entropy-x.y.z.w-Ubuntu-22.04-x86_64-portable.tar.gz
```

On Fedora, select RPM and TGZ output while configuring the application stage:

```sh
cmake --preset app-release "-DEntropy_LINUX_CPACK_GENERATORS=RPM;TGZ"
cmake --build --preset package-release --parallel
```

On a Fedora 43 x86_64 build, the package names are:

```text
Entropy-x.y.z.w-Fedora-43-x86_64.rpm
Entropy-x.y.z.w-Fedora-43-x86_64-portable.tar.gz
```

The DEB and RPM install under `/usr`. The portable tarball contains the corresponding `bin`, `lib`, and `share`
directory tree for extraction at a user-selected location.

[Native File Dialog Extended](https://github.com/btzy/nativefiledialog-extended) uses the Linux desktop portal backend,
so native file dialogs need a working `xdg-desktop-portal` service. This service lets applications ask the user's
desktop environment to show file picker dialogs.

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

Inspect and test an RPM with:

```sh
rpm -qpi build-release/packages/Entropy-x.y.z.w-Fedora-43-x86_64.rpm
rpm -qpl build-release/packages/Entropy-x.y.z.w-Fedora-43-x86_64.rpm
sudo dnf install ./build-release/packages/Entropy-x.y.z.w-Fedora-43-x86_64.rpm
entropy
```

Test a staged Linux install without installing system-wide:

```sh
cmake --install build-release --prefix build-release/linux-package-install
build-release/linux-package-install/bin/entropy --help
```

Also extract the portable tarball into an empty directory and run `bin/entropy --help` from the extracted tree. Test
the graphical application from a desktop session to verify rendering and native file dialogs.

## macOS Packages

On macOS, Entropy is built as an `.app` bundle.

Required tools:

- Xcode or Xcode Command Line Tools, which provide `codesign`

Future notarized packages will also use `xcrun notarytool` and `xcrun stapler`.

CPack creates a drag-and-drop DMG and a portable ZIP:

```text
Entropy-x.y.z.w-macOS-arm64.dmg
Entropy-x.y.z.w-macOS-arm64.zip
Entropy-x.y.z.w-macOS-x86_64.dmg
Entropy-x.y.z.w-macOS-x86_64.zip
```

The arm64 and x86_64 packages are built separately. Entropy does not publish a universal macOS binary.

The architecture must be selected during both the dependency and application configure stages. Use a separate build
directory for each architecture. For example, an x86_64 build on an Intel Mac uses:

```sh
cmake --preset deps-release -B build-release-x86_64 \
  -D CMAKE_OSX_ARCHITECTURES=x86_64 \
  -D CMAKE_OSX_DEPLOYMENT_TARGET=13.3
cmake --build build-release-x86_64 --parallel
cmake --preset app-release -B build-release-x86_64 \
  -D CMAKE_OSX_ARCHITECTURES=x86_64 \
  -D CMAKE_OSX_DEPLOYMENT_TARGET=13.3
cmake --build build-release-x86_64 --parallel
ctest --test-dir build-release-x86_64 --parallel --output-on-failure
cmake --build build-release-x86_64 --target package --parallel
```

Replace `x86_64` with `arm64` for an Apple Silicon build. Setting `Entropy_PACKAGE_ARCHITECTURE` alone only changes the
filename label. It does not change the compiled architecture. Official CI packages target macOS 13.3.

Set `CMAKE_OSX_DEPLOYMENT_TARGET` during both stages to control compiled binary compatibility. Do not use
`Entropy_MACOSX_BUNDLE_MINIMUM_SYSTEM_VERSION` as a substitute because it changes Info.plist metadata only.

To force the DMG generator:

```sh
cpack -G DragNDrop --config build-release/CPackConfig.cmake
```

By default, local and CI macOS packages use ad-hoc signing. This validates bundle integrity but does not establish a
trusted developer identity:

```text
Entropy_MACOS_CODESIGN_IDENTITY=-
```

Current macOS releases are not Developer ID signed or notarized. The package step can use a Developer ID Application
identity, but it does not enable the hardened runtime or perform notarization:

```sh
cmake --preset app-release -DEntropy_MACOS_CODESIGN_IDENTITY="Developer ID Application: <developer-name> (<team-id>)"
cmake --build --preset package-release --parallel
```

A future trusted release must also use the required hardened-runtime options and entitlements, submit the final
artifact to Apple, and staple the notarization ticket.

Test a macOS package:

```sh
open build-release/bin/Entropy.app
cmake --install build-release --config Release --prefix build-release/macos-package-install
codesign --verify --deep --strict --verbose=2 build-release/macos-package-install/Entropy.app
open build-release/macos-package-install/Entropy.app
```

Also open the DMG, drag `Entropy.app` to `/Applications`, and launch it from Finder. Extract the ZIP into an empty
directory and launch that copy as well. Ad-hoc signed builds are expected to lack a successful Gatekeeper assessment.

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
& .\build-release\windows-package-install\entropy.exe --help
```

Also install the MSI on a clean Windows system, launch Entropy from the Start Menu, verify open/save dialogs, uninstall
it, and test the portable ZIP from an empty extracted folder. Run `entropy.exe --help` from the extracted ZIP before
testing the graphical application.

The current MSI and portable executable are unsigned. Public distribution without SmartScreen warnings will require
signing the executable before packaging and signing the final MSI with a trusted code-signing certificate. Verify
future signatures with `Get-AuthenticodeSignature` or `signtool verify`.

## Optional Package Settings

Most local packages need no extra settings. Pass an override while configuring the application stage with
`cmake --preset app-release -DNAME=value`.

General settings:

| Option | Default | Purpose |
| --- | --- | --- |
| `Entropy_PACKAGE_OUTPUT_DIR` | `build-release/packages` | Selects the package output directory |
| `Entropy_PACKAGE_ARCHITECTURE` | target architecture | Changes the filename label only. It does not configure cross-compilation |
| `Entropy_STATIC_BUNDLED_DEPENDENCIES` | `ON` | Links bundled dependencies statically where practical |

Linux settings:

| Option | Default | Purpose |
| --- | --- | --- |
| `Entropy_LINUX_PACKAGE_PLATFORM_LABEL` | value from `/etc/os-release` | Changes the platform label in package filenames |
| `Entropy_LINUX_CPACK_GENERATORS` | DEB and TGZ | Selects package types. Fedora uses RPM and TGZ |

macOS settings:

| Option | Default | Purpose |
| --- | --- | --- |
| `Entropy_MACOS_CODESIGN_IDENTITY` | `-` | Selects the signing identity. `-` uses ad-hoc signing. An empty value skips signing |
| `Entropy_MACOSX_BUNDLE_IDENTIFIER` | `io.github.adlerdh.entropy` | Sets the application bundle identifier |
| `Entropy_MACOSX_BUNDLE_MINIMUM_SYSTEM_VERSION` | deployment target or `13.0` | Sets the minimum system version in Info.plist only |
| `Entropy_MACOSX_BUNDLE_VERSION` | `major.minor.feature` | Sets `CFBundleVersion` |
| `Entropy_STRIP_PACKAGED_APP` | `ON` | Removes local symbols before signing the packaged application |

Windows setting:

| Option | Default | Purpose |
| --- | --- | --- |
| `Entropy_WIX_ROOT` | empty | Locates WiX v3 when it is not found in the build tree, `PATH`, or `WIX` |

Changing `Entropy_STATIC_BUNDLED_DEPENDENCIES` requires a fresh dependency build. Do not reuse dependencies built with
a different value.

## GitHub Releases

Public GitHub Releases are created by [.github/workflows/release.yml](.github/workflows/release.yml). The workflow runs
when a tag matching `v*.*.*.*` is pushed.

The platform CI workflows can also build packages on a schedule or by manual dispatch. Run those package jobs before a
release when packaging, dependencies, icons, launch behavior, or runtime resources have changed.

The tag-driven release workflow uses this matrix:

| Platform | Runner | Packages | Validation |
| --- | --- | --- | --- |
| Windows x86_64 | `windows-2022` | MSI and portable ZIP | Release tests and staged-install smoke test |
| Ubuntu 22.04 x86_64 | `ubuntu-22.04` | DEB and portable TAR.GZ | Release tests, DEB inspection, and staged-install smoke test |
| macOS arm64 | `macos-14` | DMG and ZIP | Release tests and staged-install smoke test |
| macOS x86_64 | `macos-15-intel` | DMG and ZIP | Release tests and staged-install smoke test |
| Fedora 43 x86_64 | Fedora container on `ubuntu-24.04` | RPM and portable TAR.GZ | Release tests, RPM dependency checks, and staged-install smoke test |

Before tagging, make sure `main` is clean and current and all required CI checks pass.

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

Each platform job builds the Release configuration, runs tests, creates its packages, and tests a staged install. The
final job downloads the packages and creates source archives from files tracked by Git. It checks for all 12 expected
files, generates release notes, and creates the GitHub Release. It will not replace an existing release for the same
tag.

The release also includes source archives in ZIP and TAR.GZ formats. They contain the tagged files tracked by Git and
do not include downloaded dependencies or build products. Release notes contain a short download guide followed by
GitHub-generated notes.
