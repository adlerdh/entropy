# ENTROPY
*A cross-platform tool for interactively visualizing, comparing, segmenting, and annotating 3D medical images.*

Copyright Daniel H. Adler and the Penn Image Computing and Science Lab, Department of Radiology, University of Pennsylvania.
All rights reserved.

## Building
Entropy requires C++20 and build generation uses CMake 3.24.0. The "superbuild" pattern is used order to get and build external dependencies prior to building the Entropy application. The superbuild pattern is also used in [OpenChemistry](https://github.com/OpenChemistry/openchemistry), [ParaView](https://gitlab.kitware.com/paraview/common-superbuild/), [SimpleITK](https://github.com/SimpleITK/SimpleITK/tree/master/SuperBuild), and [Slicer](https://github.com/Slicer/Slicer). Here are sample build instructions.

Define build flags:
- `BUILD_TYPE`: Debug, Release, RelWithDebInfo, or MinSizeRel
- `SHARED_LIBS`: 0 for static; 1 for shared
- `NPROCS`: number of concurrent processes during build
```bash
BUILD_TYPE=Release
SHARED_LIBS=0
NPROCS=24
BUILD_DIR=build-${BUILD_TYPE}-shared-${SHARED_LIBS}
```

Execute superbuild and set flags:
```
cmake -S . -B ${BUILD_DIR} -DEntropy_SUPERBUILD=1 -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DBUILD_SHARED_LIBS=${SHARED_LIBS};
cmake --build ${BUILD_DIR} -- -j ${NPROCS};
```

Execute Entropy build:
```
cmake -S . -B ${BUILD_DIR} -DEntropy_SUPERBUILD=0;
cmake --build ${BUILD_DIR} -- -j ${NPROCS};
```

### Operating systems
Entropy builds on Linux, Windows, and macOS and is currently tested on
* Ubuntu 22.04, 24.04 (with gcc 12.3.0, 13.3.0)
* Windows 10, 11 (with MSVC++ 17.3.4)
* macOS 14.6.1, 15.3.1, Apple arm64 architecture (with clang 15.0.0, 16.0.0)
* ~~macOS 10.14.6, Intel x86_64 architecture (with clang 11.0.0)~~

### External dependencies
The following dependencies are added as external projects during CMake superbuild generation:
* [argparse](https://github.com/p-ranav/argparse/tree/v3.2) (v3.2)
* [Boost](https://github.com/boostorg/boost/tree/boost-1.87.0), headers only (v1.87.0)
* [CMakeRC](https://github.com/vector-of-bool/cmrc/tree/2.0.1) (v2.0.1)
* [Dear ImGui](https://github.com/ocornut/imgui/tree/v1.91.8) (v1.91.8)
* [ghc::filesystem](https://github.com/gulrak/filesystem/tree/v1.5.14) (v1.5.14)
* [GLFW](https://github.com/glfw/glfw/tree/3.4) (v3.4)
* [GLM](https://github.com/g-truc/glm/tree/1.0.1) (v1.0.1)
* [IconFontCppHeaders](https://github.com/juliettef/IconFontCppHeaders/commit/ef464d2fe5a568d30d7c88138e78d7fac7cfebc5) (ef464d2)
* [ImPlot](https://github.com/epezent/implot/tree/v0.16) (v0.16)
* [Insight Toolkit (ITK)](https://github.com/InsightSoftwareConsortium/ITK/tree/v5.4.2) (v5.4.2)
* [NanoVG](https://github.com/memononen/nanovg/commit/f93799c078fa11ed61c078c65a53914c8782c00b) (f93799c)
* [nlohmann::json](https://github.com/nlohmann/json/tree/v3.11.3) (v3.11.3)
* [spdlog](https://github.com/gabime/spdlog/tree/v1.15.1) (v1.15.1)
* [stduuid](https://github.com/mariusbancila/stduuid/tree/v1.2.3) (v1.2.3)
* [TinyFSM](https://github.com/digint/tinyfsm/tree/v0.3.3) (v1.15.1)

The following external sources and libraries have been committed directly to the Entropy repository:
* [GLAD OpenGL loaders](https://github.com/Dav1dde/glad.git) (generated from webservice)
* [GridCut](https://gridcut.com): fast max-flow/min-cut graph-cuts optimized for grid graphs 
* [ImGui file browser](https://github.com/AirGuanZ/imgui-filebrowser) with local modifications
* [imGuIZMO.quat](https://github.com/AirGuanZ/imgui-filebrowser) with local modifications

### Development libraries for Debian Linux
You may need to install additional development libraries for Mesa 3D Graphics, Wayland, Xorg, Xrandr, Xinerama, Xcursor, xkbcommon, and xi on Linux. On Debian, this can be done using

`sudo apt-get install libgl1-mesa-dev libwayland-dev xorg-dev libxkbcommon-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev`

### External resources
The following external resources have been committed directly to the Entropy repository:

* [Cousine font](https://fonts.google.com/specimen/Cousine)
* [Roboto fonts](https://fonts.google.com/specimen/Roboto)
* ["Library of Perceptually Uniform Colour Maps"](https://colorcet.com) (by Peter Kovesi)
* [matplotlib color maps](https://matplotlib.org/3.1.1/gallery/color/colormap_reference.html)
* ["Cividis" color map](https://www.ncl.ucar.edu/Document/Graphics/color_table_gallery.shtml)

Original attributions and licenses have been preserved and committed for all external sources and resources.

## Running
Entropy is run from the terminal. Images can be specified directly as command line arguments or from a JSON project file.

1. A list of images can be provided as positional arguments. If an image has an accompanying segmentation, then it is separated from the image filename using a comma (,) and no space. e.g.:

`./Entropy reference_image.nii.gz,reference_seg.nii.gz additional_image1.nii.gz additional_image2.nii.gz,additional_image2_seg.nii.gz`

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
