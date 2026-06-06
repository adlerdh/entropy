# ENTROPY
*A cross-platform tool for interactively visualizing, comparing, segmenting, and annotating 3D medical images.*

Entropy is developed by Daniel H. Adler with support from the Penn Image Computing and Science Lab (PICSL), Department of Radiology, University of Pennsylvania.

Copyright 2021-2026 Penn Image Computing and Science Lab (PICSL), University of Pennsylvania, and Daniel H. Adler. All rights reserved.

## Building
Entropy requires C++23 and build generation uses CMake 3.24.0 or later. The “superbuild” pattern is used in order to retrieve and build external dependencies prior to building the Entropy application. The superbuild pattern is also used in [OpenChemistry](https://github.com/OpenChemistry/openchemistry), [ITK](https://github.com/InsightSoftwareConsortium/ITKSphinxExamples/tree/master/Superbuild), [ParaView](https://gitlab.kitware.com/paraview/common-superbuild/), [SimpleITK](https://github.com/SimpleITK/SimpleITK/tree/master/SuperBuild), and [Slicer](https://github.com/Slicer/Slicer). Here are sample build instructions.

Define build flags:
- `BUILD_TYPE`: Debug, Release, RelWithDebInfo, or MinSizeRel.
- `SHARED_LIBS`: `OFF` for static; `ON` for shared. Static libraries are recommended for distribution.
- `NPROCS`: number of concurrent processes during build (e.g., `nproc` on Linux, `sysctl -n hw.ncpu` on macOS).

```bash
BUILD_TYPE=Release
SHARED_LIBS=1
BUILD_DIR="build-${BUILD_TYPE}-shared-${SHARED_LIBS}"
NPROCS=$(python3 -c "import os; print(os.cpu_count() or 1)")
PARALLEL=$(( NPROCS > 1 ? NPROCS - 1 : 1 ))
```

Note on generators:
- For single-config generators (e.g. Ninja, Unix Makefiles), use `-DCMAKE_BUILD_TYPE=...` at configure time.
- For multi-config generators (e.g. Visual Studio, Xcode, Ninja Multi-Config), `CMAKE_BUILD_TYPE` is ignored. Instead, pass the configuration at build time via `cmake --build ... --config ${BUILD_TYPE}`.

The steps below intentionally reconfigure the same build directory: first to run the superbuild, then to build Entropy after dependencies are available.

### CMake presets
Configure and build dependencies first, then reconfigure the same build directory for the Entropy application:

```sh
cmake --preset superbuild
cmake --build --preset superbuild --parallel

cmake --preset app
cmake --build --preset app --parallel
```

The default preset build directory is `build-default`.

### macOS packaging
On macOS, the Entropy target is built as an `.app` bundle with an `Info.plist` and app icon. For packaged releases, build the Release app first:

```sh
cmake --preset release-superbuild
cmake --build --preset release-superbuild --parallel
cmake --preset release-app
cmake --build --preset release-app --parallel
```

The build-tree app is useful for development:

```sh
open build-release/bin/Entropy.app
```

For local installation testing, use CMake's install step after the application build has completed. The install step copies required third-party dynamic libraries into `Entropy.app/Contents/Frameworks` and rewrites their install names so the app does not depend on the build tree:

```sh
cmake --install build-release --prefix build-release/install
open build-release/install/Entropy.app
```

For a distributable package, build the Release package preset after the app build has completed. CPack creates a drag-and-drop DMG named like `Entropy-x.y.z.w-macOS.dmg`:

```sh
cmake --build --preset release-package --parallel
```

The install and CPack steps ad-hoc sign the copied app by default so local Finder launches work after `fixup_bundle` rewrites library paths. For a real release, configure the app build with a Developer ID Application identity:

```sh
cmake --preset release-app -Dentropy_MACOS_CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
cmake --build --preset release-package --parallel
```

To test the DMG manually, open it, drag `Entropy.app` to `/Applications`, and launch it from Finder or Launchpad. Finder-launched macOS builds write logs under `~/Library/Logs/Entropy` and UI state under `~/Library/Application Support/Entropy`; terminal launches keep the development defaults described in the Running section.

Generated DMG files and CPack scratch directories are ignored by Git. Public release builds still need Developer ID signing and notarization before distributing outside local development machines.

### Linux packaging
On Linux, build the Release app first for packaging:

```sh
cmake --preset release-superbuild
cmake --build --preset release-superbuild --parallel
cmake --preset release-app
cmake --build --preset release-app --parallel
```

The build-tree app is useful for development:

```sh
build-release/bin/Entropy
```

For local installation testing, use CMake's install step after the application build has completed. The install step copies the Entropy executable to `bin`, copies required bundled shared libraries to `lib/entropy`, installs the desktop launcher and icon, and verifies that bundled private libraries do not contain build-tree RPATH/RUNPATH entries:

```sh
cmake --install build-release --prefix build-release/linux-package-install
build-release/linux-package-install/bin/Entropy
```

For a distributable package, build the Release package preset after the app build has completed. CPack creates a Debian package named like `Entropy-x.y.z.w-Linux-x86_64.deb`:

```sh
cmake --build --preset release-package --parallel
```

To test the package manually, install it with apt and launch the installed app:

```sh
sudo apt install ./Entropy-x.y.z.w-Linux-x86_64.deb
Entropy
```

The Linux package keeps Entropy's third-party CMake-built dependencies private under `/usr/lib/entropy` and lets `dpkg-shlibdeps` record system package dependencies for non-bundled system libraries. Non-Debug CPack packages are stripped to remove symbol tables from the staged binaries. The package also recommends `xdg-desktop-portal` and a common portal backend for native file dialogs.

Additional presets are available for debug and release builds:

```sh
cmake --preset debug-superbuild
cmake --build --preset debug-superbuild --parallel
cmake --preset debug-app
cmake --build --preset debug-app --parallel

cmake --preset release-superbuild
cmake --build --preset release-superbuild --parallel
cmake --preset release-app
cmake --build --preset release-app --parallel
```

### Single-config generators (e.g. Ninja, Unix Makefiles)
These commands are the non-preset equivalent of the build flow above. Use them when you need a custom build directory, generator, build type, or shared/static library setting.

Configure and build the superbuild:
```sh
cmake -S . -B ${BUILD_DIR} \
  -DEntropy_SUPERBUILD=ON \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -DBUILD_SHARED_LIBS=${SHARED_LIBS} \
  -DSUPERBUILD_PARALLEL=${PARALLEL}

cmake --build ${BUILD_DIR} --parallel ${PARALLEL}
```

Reconfigure the same build directory to build Entropy (after dependencies are built):
```sh
cmake -S . -B ${BUILD_DIR} \
  -DEntropy_SUPERBUILD=OFF \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -DBUILD_SHARED_LIBS=${SHARED_LIBS}

cmake --build ${BUILD_DIR} --parallel ${PARALLEL}
```

### Multi-config generators (e.g. Visual Studio, Xcode, Ninja Multi-Config)
Configure and build the superbuild:
```sh
cmake -S . -B ${BUILD_DIR} \
  -DEntropy_SUPERBUILD=ON \
  -DBUILD_SHARED_LIBS=${SHARED_LIBS} \
  -DSUPERBUILD_PARALLEL=${PARALLEL}

cmake --build ${BUILD_DIR} --config ${BUILD_TYPE} --parallel ${PARALLEL}
```

Reconfigure the same build directory to build Entropy:
```sh
cmake -S . -B ${BUILD_DIR} \
  -DEntropy_SUPERBUILD=OFF \
  -DBUILD_SHARED_LIBS=${SHARED_LIBS}

cmake --build ${BUILD_DIR} --config ${BUILD_TYPE} --parallel ${PARALLEL}
```

### Supported platforms
Entropy is developed and tested on the following platforms:

- Ubuntu 22.04 and 24.04 with GCC 13.3.0 or newer
- Windows 10 and 11 with Visual Studio 2022 17.3.4 or newer
- macOS 14.6.1, 15.3.1, 15.6.1, and 26.0.1 on Apple arm64 with Apple Clang 15.0.0 or newer

Other Linux distributions may work if they provide a C++23 compiler, CMake 3.24 or newer, and the development libraries listed below. macOS Intel x86_64 is not supported.

Entropy uses the C++ standard library implementation of `std::filesystem`. The old `ghc::filesystem` compatibility dependency is no longer required for supported macOS versions.

### Development libraries for Debian Linux
Linux development builds require system development packages for OpenGL/windowing support and native file dialogs. On Debian/Ubuntu, install:

```sh
sudo apt-get install xorg-dev libgl1-mesa-dev libxcursor-dev libxkbcommon-dev libxinerama-dev libxi-dev libxrandr-dev libwayland-dev libdbus-1-dev
```

`libdbus-1-dev` is required by [Native File Dialog Extended](https://github.com/btzy/nativefiledialog-extended) when Entropy uses the xdg-desktop-portal backend on Linux. This is a build-time package, whereas Linux runtime packages should depend on `libdbus-1-3` and a working `xdg-desktop-portal` backend. On macOS and Windows, Native File Dialog Extended uses platform APIs, so no additional NFDe-specific development package is required.

### Third-party dependencies and resources
Entropy uses external projects through the CMake superbuild and also carries some vendored source and resource files in this repository. Versions, source URLs, and license notes are documented in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The current superbuild pins include GLM 1.0.3, Dear ImGui 1.92.8, ImPlot 0.17, nlohmann::json 3.12.0, and spdlog 1.17.0.

The ASCII shader rendering is inspired by [Alex Harri's ASCII rendering work](https://alexharri.com/blog/ascii-rendering) and [Yusef28's Shadertoy work](https://www.shadertoy.com/user/Yusef28).

Original attributions and licenses have been preserved and committed for all external sources and resources.

## Running
Entropy is run from the terminal. Images can be specified directly as command line arguments or from a JSON project file.
- On Linux: `./${BUILD_DIR}/bin/Entropy`
- On macOS: `open ${BUILD_DIR}/bin/Entropy.app`, or run `${BUILD_DIR}/bin/Entropy.app/Contents/MacOS/Entropy` from a terminal.
- On Windows: `.\${BUILD_DIR}\bin\Entropy.exe`

1. Images can be provided directly with repeatable `--image` options. If an image has one or more accompanying segmentations, place `--seg` immediately after that image; the segmentations are attached to the preceding image in the order provided. `--seg` may be repeated, or it may be followed by multiple segmentation file names. e.g.:

```sh
Entropy --image reference_image.nii.gz --seg reference_seg1.nii.gz reference_seg2.nii.gz \
        --image additional_image1.nii.gz \
        --image additional_image2.nii.gz --seg additional_image2_seg1.nii.gz --seg additional_image2_seg2.nii.gz
```

Direct image inputs and `--project` are mutually exclusive.

2. Images can be specified in a JSON project file that is loaded using the `-p` argument. A sample project file:
```json
{
  "reference":
  {
    "image": "reference_image.nii.gz",
    "segmentations": [
      {
        "path": "reference_seg.nii.gz"
      }
    ],
    "landmarks": [
      {
        "path": "reference_landmarks.csv",
        "inVoxelSpace": false
      }
    ],
    "annotations": "reference_annotations.json"
  },
  "additional":
  [
    {
      "image": "additional_image1.nii.gz",
      "affine": "additional_image1_affine.txt",
      "segmentations": [
        {
          "path": "additional_image1_seg1.nii.gz"
        },
        {
          "path": "additional_image1_seg2.nii.gz"
        }
      ],
      "landmarks": [
        {
          "path": "additional_image1_landmarks1.csv",
          "inVoxelSpace": false
        },
        {
          "path": "additional_image1_landmarks2.csv",
          "inVoxelSpace": false
        }
      ],
      "annotations": "additional_image1_annotations.json"
    },
    {
      "image": "additional_image2.nii.gz",
      "affine": "additional_image2_affine.txt"
    }
  ]
}
```

> The project must specify a reference image with the "reference" entry. Any number of additional (overlay) images are provided in the "additional" vector. An image can have an optional affine transformation matrix file, any number of segmentation layers, and any number of landmark files.

> Note: The project file format is subject to change!

Logs are output to the console and to files saved in the `log` folder. Log level can be set using the `-l` argument. See help (`-h`) for more details.

## Keyboard shortcuts

### Modes
| Key | Mode | Controls |
| --- | --- | --- |
| `v` | Crosshairs | - left button: move crosshairs<br>- CTRL + left button: rotate crosshairs<br>- middle button: pan image<br>- right button: zoom image |
| `z` | Zoom | - left button: zoom to crosshairs<br>- right button: zoom to cursor pointer |
| `x` | Pan/dolly | - left button: pan image in plane<br>- right button: dolly in/out of plane (3D views only) |
| `l` | Image adjustment | - left button: adjust image window left/right and level up/down<br>- right button: adjust image opacity |
| `t` | Image translation | - left button: translate image in plane<br>- right button: translate image in/out of plane |
| `r` | Image rotation | - left button: rotate image in plane<br>- right button: rotate image in/out of plane |
| `b` | Image segmentation (brush) | - left button: paint foreground label<br>- right button or shift: paint background label |

### View properties
| Key | Action |
| --- | --- |
| `w` | Toggle image visibility |
| `e` | Toggle image edges |
| `s` | Toggle segmentation visibility |
| `a` | Decrease segmentation opacity |
| `d` | Increase segmentation opacity |
| `space` | Toggle segmentation outline |
| `c` | Center views on crosshairs<br>- shift: reset zoom, recenter, and realign crosshairs |
| `o` | Cycle visibility of all UI overlays |
| `F4` | Toggle full-screen mode (ESC to exit) |

### Image navigation
| Key | Action |
| --- | --- |
| `left/right/down/up` arrows | Move crosshairs |
| `page down/up` | Scroll slices<br>- shift: cycle active image component |

### Layouts
| Key | Action |
| --- | --- |
| `[`, `]` | Cycle view layout<br>- shift: cycle active image |

### Segmentation brush
| Key | Action |
| --- | --- |
| `<`, `>` | Cycle foreground label<br>- shift: cycle background label |
| `-`, `+` | Decrease/increase brush size |
| `shift` | Use background label while painting or previewing with the brush |
