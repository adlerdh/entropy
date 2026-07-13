# Entropy Medical Image Viewer

[![macOS CI](https://github.com/adlerdh/entropy/actions/workflows/macos.yml/badge.svg?branch=main)](https://github.com/adlerdh/entropy/actions/workflows/macos.yml)
[![Windows CI](https://github.com/adlerdh/entropy/actions/workflows/windows.yml/badge.svg?branch=main)](https://github.com/adlerdh/entropy/actions/workflows/windows.yml)
[![Ubuntu CI](https://github.com/adlerdh/entropy/actions/workflows/ubuntu.yml/badge.svg?branch=main)](https://github.com/adlerdh/entropy/actions/workflows/ubuntu.yml)
[![Release](https://github.com/adlerdh/entropy/actions/workflows/release.yml/badge.svg)](https://github.com/adlerdh/entropy/actions/workflows/release.yml)
[![Coverage](https://adlerdh.github.io/entropy/coverage/badge.svg)](https://adlerdh.github.io/entropy/coverage/)
[![Latest Release](https://img.shields.io/github/v/release/adlerdh/entropy)](https://github.com/adlerdh/entropy/releases)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE.txt)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)

<img src="res/icons/Linux/hicolor/128x128/apps/io.github.adlerdh.entropy.png" alt="Entropy icon" align="left" width="128" hspace="16" vspace="4">

Entropy is a cross-platform tool for visualizing, comparing, registering, segmenting, annotating, and inspecting medical images.

It is built to handle projects with multiple images in a common reference space. It can load any number of images, arrange them in flexible layouts, render them with fast shader-based MPR views, and display their values, coordinates, and transforms. Different rendering modes help with comparing images and evaluating spatial alignment.

Entropy is primarily developed and maintained by Daniel H. Adler, Ph.D., with support from Professor [James C. Gee, Ph.D.](https://www.med.upenn.edu/apps/faculty/index.php/g275/p10656).

Copyright 2021-2026 Daniel H. Adler, Ph.D. and the Penn Image Computing and Science Lab (PICSL), University of Pennsylvania. All rights reserved.

## Building

Entropy uses CMake and C++23. Build the pinned third-party libraries first, then configure and build Entropy against them following the "superbuild" pattern:

```sh
BUILD_TYPE=debug # or release

# Dependencies
cmake --preset deps-${BUILD_TYPE}
cmake --build --preset deps-${BUILD_TYPE} --parallel

# Application
cmake --preset app-${BUILD_TYPE}
cmake --build --preset app-${BUILD_TYPE} --parallel
```

Reduce the parallelism if it runs out of memory. Run with:

```sh
open build-${BUILD_TYPE}/bin/Entropy.app # macOS
.\build-${BUILD_TYPE}\bin\entropy.exe # Windows
build-${BUILD_TYPE}/bin/entropy # Linux
```

Release packages are generated with preset `package-release` for:

- macOS Apple Silicon
- macOS Intel
- Windows, x86_64
- Ubuntu Linux, x86_64
- Fedora Linux, x86_64

See [BUILDING.md](BUILDING.md) for detailed platform and build requirements, compiler versions, Linux development packages, build options, tests, and coverage instructions. Packaging commands and artifact layouts are detailed in [PACKAGING.md](PACKAGING.md).

## Overview

Entropy is useful for reviewing multiple images in relation to one another in a common reference space. It handles multimodal scalar and multi-component images in 2D, 3D, and 4D (time series), as well as segmentations, vector annotations, and registration transformations (affine and deformation fields).

### Image Visualization and Comparison

Entropy is designed for crisp, responsive rendering. It uses GPU 3D texturing and preserves native voxel component types instead of casting to a fixed display type. A project contains a reference image that defines the common coordinate space, plus any number of additional images.

- Flexible layouts with per-view image visibility
  - Axial, coronal, sagittal, and oblique multi-planar reconstruction (MPR)
  - Minimum, mean, and maximum intensity projections and X-ray simulation
  - Tiled lightbox views
  - Rotatable crosshairs
  - 3D rendering of image isosurfaces
- Rendering modes
  - Layered images with opacity blending
  - Horizontal and vertical swiping comparison
  - Flashlight and checkerboard comparison
  - Visual metrics: difference, overlap, local normalized cross-correlation (NCC), and local linear residual
- Image adjustments
  - Window width and level
  - Layer opacity
  - Color maps, continuous and discrete
  - Image edge filtering
  - Nearest neighbor, linear, and cubic B-spline interpolation
- Precise image interrogation
  - Voxel intensities and percentiles
  - Spatial coordinates, including after transformation
  - Iso-contouring
- View and crosshairs synchronization
  - Across linked views and layouts
  - With other Entropy sessions or [ITK-SNAP](https://www.itksnap.org/)

### Transformations and Warps

Entropy has advanced support for image transformations:

- Affine transformations (from voxel to physical space) from image headers
- Import additional affine transformations
- Manual translation, rotation, and scaling
- Load inverse and forward deformable warp fields computed from external registration tools
  - Interactively apply warps to images
- Vector field visualization
  - Vector field arrows and warped grid lines
  - Compute maps of field magnitude, Jacobian determinant, Laplacian, divergence, and curl
  - Compute matching inverse or forward warp fields for loaded deformation fields

### Registration Backends

Entropy does not implement its own full registration engine. Instead, it can launch external registration tools, monitor them, and import their results into the project where the images are already loaded. Current registration backends are:

- [ANTs](https://github.com/ANTsX/ANTs)
- [FireANTs](https://fireants.readthedocs.io/en/latest/)
- [Greedy](https://greedy.readthedocs.io/en/latest/)

The Image Registration workflow lets users select fixed and moving images, choose parameters for each backend, inspect the command preview, monitor progress and logs, and import generated outputs. Imported outputs can include transformed moving images, affine matrices, inverse warp fields, and forward warp fields. Backend executables are configured in Application Settings.

### Segmentation, Annotations, and Landmarks

The same views used for image comparison can be used to paint segmentation overlays, draw vector-based annotations, and place point landmarks:

- Save, clear, remove, and inspect segmentation layers
  - Multiple segmentations per image
  - Filled or outline rendering
- Paint segmentations with foreground and background labels
  - Adjust brush shape (2D/3D, round/square) and behavior
- Draw vector-based annotations, including on oblique planes
  - Fill annotations to create segmentations
- Create and save groups of point landmarks in physical or voxel coordinates

## Supported Formats

Entropy uses [ITK](https://itk.org/) for image I/O of these common medical image formats:

- [Neuroimaging Informatics Technology Initiative (NIfTI)](https://nifti.nimh.nih.gov/nifti-1/) and Analyze header/image pairs (`.nii`, `.nii.gz`, `.hdr`, `.img`)
- [Nearly Raw Raster Data (NRRD)](https://teem.sourceforge.net/nrrd/format.html) (`.nrrd`, `.nhdr`)
- [MetaImage](https://insightsoftwareconsortium.github.io/ITKWikiArchive/Wiki/ITK/MetaIO/Documentation/) (`.mha`, `.mhd`, with companion raw data `.raw`, `.zraw`, or `.raw.gz`)
- [DICOM](https://www.dicomstandard.org/current/) via [GDCM](https://gdcm.sourceforge.net/) (`.dcm` and DICOM series)

Entropy also displays complete image header information and DICOM metadata. It loads the supporting files needed to make a review complete: segmentations, landmarks, annotations, affine transforms, deformation warp fields, layouts, and project files.

### Multi-Component and Time Series Images

Supported image categories include:

- 2D and 3D scalar images
- 4D time series
- Integer (signed/unsigned 8/16/32-bit) and floating-point component types
- Complex-valued images
- RGB and RGBA images
- Vector fields and deformation warp fields
- General images with N components
- Segmentations and label images

Images with multiple components can be viewed by individual component, magnitude, RGB/RGBA interpretation, and derived projections where appropriate. Time series images can be reviewed with per-image and global time controls.

## Technical Notes

Entropy is a native C++ application built for interactive desktop performance across multiple platforms. Third-party dependencies are pinned from source.

|  |  |
| --- | --- |
| Platforms | macOS, Windows, Ubuntu, and Fedora |
| Language | C++23 |
| Build system | CMake presets with separate dependency and app stages |
| Toolchains | Apple Clang, MSVC/Visual Studio, and GCC in CI |
| Tests | Unit tests and coverage reports run with GitHub Actions |
| Rendering | OpenGL 3.3 Core with GLSL shaders |
| Image I/O | Medical image loading through [ITK](https://itk.org/) and [GDCM](https://gdcm.sourceforge.net/) |
| UI | [Dear ImGui](https://github.com/ocornut/imgui) with [Docking support](https://github.com/ocornut/imgui/wiki/Docking) and native platform menus and dialog integration |

Entropy targets OpenGL 3.3 Core with GLSL 3.30 shaders to maximize compatibility across platforms and operating system versions. Detailed compiler versions, operating system versions, development packages, and coverage workflows are documented in [BUILDING.md](BUILDING.md).

### Continuous Integration

GitHub Actions builds and tests Entropy on macOS, Windows, Ubuntu, and Fedora. Current CI coverage includes macOS 14.6.1, 15.3.1, 15.6.1, and 26.0.1; Windows 10 and 11; Ubuntu 22.04 and 24.04; and Fedora 43. The CI toolchains are Apple Clang 15.0.0+, MSVC/Visual Studio 2022 17.3.4+, and GCC 13.3.0+.

## Core Concepts

### Reference Image

The reference image defines the main project space. Additional images are compared against it and may have affine transformations or deformable warps that relate them to the reference and each other.

### Active Image

The active image is the image currently targeted by many editing, inspection, transformation, and menu actions.

### Image Geometry and Affine Transformations

Entropy separates image geometry from affine transformations loaded or edited by the user:

1. The image header defines the image's native voxel to subject geometry
2. The initial/imported affine transformation is used for a loaded alignment or an alignment computed by registration
3. The manual affine transformation is intended for interactive adjustment

This separation makes it possible to inspect and revise alignment without losing the original image geometry.

### Deformable Warp Fields

Entropy uses inverse warp fields for image sampling and forward warp fields for moving spatial objects, such as landmarks and annotations. A project with one image can still apply a warp to that image, treating the image as its own reference space.

### Application Settings and Project Settings

Application settings store personal UI preferences and backend configuration. Project settings store presentation and review state that is packaged with a project, such as layouts, comparison settings, raycasting settings, segmentation display defaults, and transformation assignments.

## Quick Start

One typical workflow is to load a reference image and one image to compare against it:

1. Open one or more images from the File menu, the opening screen, or the command line
2. Set the image that you want to compare against as the reference image
3. Choose a layout and view types for the review or analysis task
4. Use image visibility, opacity, comparison modes, and the opacity mixer to compare images
5. Use the voxel inspector to verify coordinates and sampled values
6. Add segmentations, annotations, landmarks, affine transformations, or warp fields as needed
7. Save the work as an Entropy project

### Command Line

The command line accepts image, DICOM, and project inputs:

| Option | Meaning |
| --- | --- |
| `--image`, `-i` | Image path, repeat for multiple images |
| `--seg`, `-s` | Segmentation path(s) for the preceding image |
| `--dicom`, `-d` | DICOM folder or file to scan |
| `--project`, `-p` | Entropy project JSON file, mutually exclusive with `--image`, `--seg`, and `--dicom` |
| `--layouts` | View layouts specification JSON file |
| `--log-level`, `-l` | Console log level |

Examples:
```sh
entropy -i ref.nii.gz -s ref_seg.nii.gz -i moving.nii.gz
entropy -p project.json
entropy -d /path/to/dicom_folder
```

Image inputs, DICOM inputs, and `--project` are mutually exclusive. Use `entropy --help` for the complete command-line reference.

### Project Files

Entropy project files preserve all state needed to reopen a review. They include a reference image, additional images, segmentations, landmarks, annotations, transformations, layouts, and presentation settings. Minimal example:

```json
{
  "reference": {
    "image": "reference_image.nii.gz"
  },
  "additional": [
    {
      "image": "moving_image_1.nii.gz"
    },
    {
      "image": "moving_image_2.nii.gz"
    }
  ]
}
```

Including segmentations, landmarks, annotations, and an affine transformation:

```json
{
  "reference": {
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
  "additional": [
    {
      "image": "moving_image.nii.gz",
      "affine": "moving_image_affine.txt",
      "segmentations": [
        {
          "path": "moving_image_seg.nii.gz"
        }
      ],
      "landmarks": [
        {
          "path": "moving_image_landmarks.csv",
          "inVoxelSpace": false
        }
      ],
      "annotations": "moving_image_annotations.json"
    }
  ]
}
```

> The project format may evolve as Entropy develops.

### Keyboard Shortcuts

The complete list is available under menu *Help > Keyboard Shortcuts*.

| Shortcut | Action |
| --- | --- |
| `V` | Pointer and crosshairs mode |
| `L` | Window/level mode |
| `Z` | Zoom mode |
| `P` | Pan mode |
| `R` | Image rotation mode |
| `T` | Image translation mode |
| `B` | Segmentation brush mode |
| `[` / `]` | Previous or next layout |
| `Shift + [` / `Shift + ]` | Previous/next active image |
| `Left` / `Right` / `Down` / `Up` | Move crosshairs |
| `Page Down` / `Page Up` | Previous/next slice |
| `W` | Toggle image visibility |
| `S` | Toggle segmentation visibility |
| `C` | Recenter views on crosshairs |
| `Shift + C` | Reset view orientation, zoom, and centering |
| `X` | Show/hide crosshairs |
| `U` | Show/hide user interface |
| `I` | Show/hide voxel inspector |
| `O` | Cycle view overlays |

## Runtime Files

Entropy writes a small number of user-level runtime files outside of project files

### Settings

Entropy persists application settings in the standard location for each platform:

- macOS: `~/Library/Application Support/Entropy/settings.json`
- Windows: `%APPDATA%\Entropy\settings.json`
- Linux: `${XDG_CONFIG_HOME:-~/.config}/entropy/settings.json`

Settings owned by a project are saved in project JSON files instead of the application settings file.

### Logging

Entropy writes logs to the console and to daily log files. Log verbosity can be changed from Application Settings or with `--log-level`. Default log locations:

- macOS: `~/Library/Logs/Entropy/`
- Windows: `%APPDATA%\Entropy\Logs\`
- Linux: `${XDG_STATE_HOME:-~/.local/state}/entropy/logs/`

## License

Entropy source code and official release packages are distributed under the Apache License 2.0 ([LICENSE.txt](LICENSE.txt)). Required project notices are in [NOTICE.txt](NOTICE.txt). Third-party dependency notices are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
