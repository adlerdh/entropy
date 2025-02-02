# ENTROPY

*A cross-platform tool for interactively visualizing, comparing, segmenting, and annotating 3D medical images.*

Copyright Daniel H. Adler and the Penn Image Computing and Science Lab, Department of Radiology, University of Pennsylvania.
All rights reserved.


## Building

Entropy requires C++20 and build generation uses CMake 3.24.0+.

### Operating systems
Entropy builds on Linux, Windows, and macOS. It is currently tested on the following:

* Ubuntu 22.04, 24.04 (with gcc 12.3.0, 14.2.0)
* Windows 10, 11 (with MSVC++ 17.3.4)
* macOS 10.14.6, Intel x86_64 architecture (with clang 11.0.0)
* macOS 14.6.1, 15.2.0, Apple arm64 architecture (with clang 15.0.0, 16.0.0)

### External libraries
Please clone/download and build these libraries:

* [Boost](https://github.com/boostorg/boost), header libraries only (1.84.0 - 1.87.0)
* [Insight Toolkit, ITK](https://github.com/InsightSoftwareConsortium/ITK) (v5.3.0 - v5.4.2), ~~built with Module_ITKVtkGlue~~
* [spdlog](https://github.com/gabime/spdlog) (v1.15.0)
* ~~[Visualization Toolkit, VTK](https://github.com/Kitware/VTK) (9.3.0)~~

Note: build Boost (headers only) with `./bootstrap.sh && ./b2 headers`

More recent versions of ITK and Boost should also work with potentially minor modification to Entropy code. Please note that from Boost, only the header-only libraries (none of the compiled libraries) are required.

The following libraries and dependencies are brought in as Git submodules to the Entropy repository:

* argparse (https://github.com/p-ranav/argparse)
* CMake Modules Collection (https://github.com/rpavlik/cmake-modules)
* CMakeRC (https://github.com/vector-of-bool/cmrc)
* Dear ImGui (https://github.com/ocornut/imgui)
* ghc::filesystem (https://github.com/gulrak/filesystem)
* GLFW (https://github.com/glfw/glfw)
* GLM (https://github.com/g-truc/glm)
* IconFontCppHeaders (https://github.com/juliettef/IconFontCppHeaders)
* imgui-filebrowser (https://github.com/AirGuanZ/imgui-filebrowser)
* imGuIZMO.quat (https://github.com/BrutPitt/imGuIZMO.quat)
* ImPlot (https://github.com/epezent/implot)
* JSON for Modern C++ (https://github.com/nlohmann/json)
* NanoVG (https://github.com/memononen/nanovg)
* ~~spdlog (https://github.com/gabime/spdlog)~~
* stduuid (https://github.com/mariusbancila/stduuid)
* TinyFSM (https://github.com/digint/tinyfsm)

Clone Entropy with `--recurse-submodules` or run the folllowing command to clone the submodules:
`git submodule update --init --recursive`

The following external sources and libraries have been committed directly to the Entropy repository:

* GLAD OpenGL loaders (generated from https://github.com/Dav1dde/glad.git)
* GridCut: fast max-flow/min-cut graph-cuts optimized for grid graphs (https://gridcut.com)
* Local modifications to the ImGui bindings for GLFW and OpenGL 3 (see originals in externals/imgui/backends)
* Local modifications to the ImGui file browser (https://github.com/AirGuanZ/imgui-filebrowser)


### Development libraries for Debian Linux

You may need to install additional development libraries for the Mesa 3D Graphics Library, Xrandr, Xinerama, Xcursor, and xi on Linux. On Debian, this can be done using

`sudo apt-get install libgl1-mesa-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev`


### External resources
The following external resources have been committed directly to the Entropy repository:

* Cousine font (https://fonts.google.com/specimen/Cousine)
* Roboto fonts (https://fonts.google.com/specimen/Roboto)
* "Library of Perceptually Uniform Colour Maps" by Peter Kovesi (https://colorcet.com)
* matplotlib color maps (https://matplotlib.org/3.1.1/gallery/color/colormap_reference.html)
* "Cividis" color map (https://www.ncl.ucar.edu/Document/Graphics/color_table_gallery.shtml)

Original attributions and licenses have been preserved and committed for all external sources and resources.


## Running

Entropy is run from the terminal. Images can be specified directly as command line arguments or from a JSON project file.

1. A list of images can be provided as positional arguments. If an image has an accompanying segmentation, then it is separated from the image filename using a comma (,) and no space. e.g.:

`./entropy reference_image.nii.gz,reference_seg.nii.gz additional_image1.nii.gz additional_image2.nii.gz,additional_image2_seg.nii.gz`

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
