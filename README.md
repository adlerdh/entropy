# ENTROPY
*A cross-platform tool for interactively visualizing, comparing, segmenting, and annotating 3D medical images.*

Copyright Daniel H. Adler and the Penn Image Computing and Science Lab, Department of Radiology, University of Pennsylvania. All rights reserved.

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

### Single-config generators (e.g. Ninja, Unix Makefiles)
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

### Operating systems
Entropy builds and runs on the following versions of Linux, Windows, and macOS:
* Ubuntu 22.04, 24.04 (with gcc 13.3.0+)
* Windows 10, 11 (with MSVC++ 17.3.4+)
* macOS 14.6.1, 15.3.1, 15.6.1, 26.0.1 Apple arm64 architecture (with clang 15.0.0+)
* ~~macOS Intel x86_64 architecture~~ (not supported)

### Development libraries for Debian Linux
You may need to install additional development libraries for Mesa 3D Graphics, Wayland, Xorg, Xrandr, Xinerama, Xcursor, xkbcommon, and xi on Linux. On Debian, this can be done using

`sudo apt-get install xorg-dev libgl1-mesa-dev libxcursor-dev libxkbcommon-dev libxinerama-dev libxi-dev libxrandr-dev libwayland-dev`

### Third-party dependencies and resources
Entropy uses external projects through the CMake superbuild and also carries some vendored source and resource files in this repository. Versions, source URLs, and license notes are documented in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

The ASCII shader rendering is inspired by [Alex Harri's ASCII rendering work](https://alexharri.com/blog/ascii-rendering) and [Yusef28's Shadertoy work](https://www.shadertoy.com/user/Yusef28).

Original attributions and licenses have been preserved and committed for all external sources and resources.

## Running
Entropy is run from the terminal. Images can be specified directly as command line arguments or from a JSON project file.
- On Linux or macOS: `./${BUILD_DIR}/bin/Entropy`
- On Windows: `.\${BUILD_DIR}\bin\Entropy.exe`

1. A list of images can be provided as positional arguments. If an image has an accompanying segmentation, then it is separated from the image filename using a comma (,) and no space. e.g.:

`Entropy reference_image.nii.gz,reference_seg.nii.gz additional_image1.nii.gz additional_image2.nii.gz,additional_image2_seg.nii.gz`

With this input format, each image may have only one segmentation.

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

Logs are output to the console and to files saved in the `logs` folder. Log level can be set using the `-l` argument. See help (`-h`) for more details.

## Keyboard shortcuts

### Modes
* **v**: Crosshairs Mode
  - *left button*: move crosshairs
  - *CTRL + left button*: rotate crosshairs
  - *middle button*: pan image
  - *right button*: zoom image
* **z**: Zoom Mode
  - *left button*: zoom to crosshairs
  - *right button*: zoom to cursor pointer
* **x**: Pan/Dolly Mode
  - *left button*: pan image in plane
  - *right button*: dolly in/out of plane (3D views only)
* **l**: Image Adjustment Mode
  - *left button*: adjust image window (left/right) and level (up/down)
  - *right button*: adjust image opacity
* **t**: Image Translation Mode
  - *left button*: translate image in plane
  - *right button*: translate image in/out of plane
* **r**: Image Rotation Mode
  - *left button*: rotate image in plane
  - *right button*: rotate image in/out of plane
* **b**: Image Segmentation Mode (brush)
  - *left button*: paint foreground label
  - *right button*: paint background label

### View properties
* **w**: Toggle image visibility
* **e**: Toggle image edges
* **s**: Toggle segmentation visibility
* **a**: Reduce segmentation opacity
* **d**: Increase segmentation opacity
* **space**: Toggle segmentation outline
* **c**: Center views on crosshairs
  - *shift*: Reset zoom; recenter and realign crosshairs
* **o**: Cycle visibility of all UI overlays
* **F4**: Toggle full-screen mode (ESC to exit)

### Image navigation
* **left/right/down/up** arrows: Move crosshairs
* **page down/up**: Scroll slices
  - *shift*: Cycle active image component

### Layouts
* **[**, **]**: Cycle view layout
  - *shift*: Cycle active image

### Segmentation brush
* **<**, **>**: Cycle foreground label
  - *shift*: Cycle background label
* **-**, **+**: Decrease/increase brush size
