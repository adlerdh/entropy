# Entropy Medical Image Viewer

[![macOS CI](https://github.com/adlerdh/entropy/actions/workflows/macos.yml/badge.svg?branch=main)](https://github.com/adlerdh/entropy/actions/workflows/macos.yml)
[![Windows CI](https://github.com/adlerdh/entropy/actions/workflows/windows.yml/badge.svg?branch=main)](https://github.com/adlerdh/entropy/actions/workflows/windows.yml)
[![Ubuntu CI](https://github.com/adlerdh/entropy/actions/workflows/ubuntu.yml/badge.svg?branch=main)](https://github.com/adlerdh/entropy/actions/workflows/ubuntu.yml)
[![Release](https://github.com/adlerdh/entropy/actions/workflows/release.yml/badge.svg)](https://github.com/adlerdh/entropy/actions/workflows/release.yml)
[![Latest Release](https://img.shields.io/github/v/release/adlerdh/entropy)](https://github.com/adlerdh/entropy/releases)
[![Static Analysis](https://github.com/adlerdh/entropy/actions/workflows/static-analysis.yml/badge.svg?branch=main)](https://github.com/adlerdh/entropy/actions/workflows/static-analysis.yml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE.txt)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)

<img src="res/icons/Linux/hicolor/128x128/apps/io.github.adlerdh.entropy.png" alt="Entropy icon" align="left" width="128" hspace="16" vspace="4">

Entropy is a cross-platform desktop application for visualizing, comparing, registering, segmenting, annotating, and
inspecting medical images.

It is designed for projects containing multiple images in a common reference space. Flexible layouts, interactive 2D
and 3D views, and several comparison modes make it easier to inspect images and evaluate their spatial alignment.

Entropy is primarily developed and maintained by Daniel H. Adler, Ph.D., with support from Professor
[James C. Gee, Ph.D.](https://www.med.upenn.edu/apps/faculty/index.php/g275/p10656).

Copyright 2021-2026 Daniel H. Adler, Ph.D. and the Penn Image Computing and Science Lab (PICSL), University of
Pennsylvania. All rights reserved.

## Get Entropy

Download the latest packages for macOS, Windows, Ubuntu, and Fedora from
[GitHub Releases](https://github.com/adlerdh/entropy/releases).

### Build from Source

Entropy uses CMake and C++23. A two-stage build first compiles the pinned dependencies and then the application:

```sh
cmake --preset deps-release
cmake --build --preset deps-release --parallel
cmake --preset app-release
cmake --build --preset app-release --parallel
ctest --test-dir build-release --parallel --output-on-failure
```

Start with [BUILDING.md](BUILDING.md) for prerequisites, platform-specific commands, tests, static analysis, and
troubleshooting. See [PACKAGING.md](PACKAGING.md) to create release packages for macOS, Windows, Ubuntu, or Fedora.

## Overview

Entropy is useful for reviewing multiple images in relation to one another in a common reference space. It handles
multimodal scalar and multi-component images in 2D, 3D, and 4D (time series), as well as segmentations, vector
annotations, and registration transformations (affine matrices and deformation fields).

### Image Visualization and Comparison

Entropy is designed for crisp, responsive rendering. It uses GPU 3D texturing and preserves native voxel component types
instead of casting to a fixed display type. A project contains a reference image that defines the common coordinate
space, plus any number of additional images.

- Flexible layouts with per-view image visibility
  - Axial, coronal, sagittal, and oblique multi-planar reconstruction (MPR)
  - Minimum, mean, and maximum intensity projections and X-ray simulation
  - Tiled lightbox views
  - Rotatable crosshairs
  - 3D image planes, image isosurfaces, and segmentation surfaces
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

Entropy does not implement its own full registration engine. Instead, it can launch external registration tools, monitor
them, and import their results into the project where the images are already loaded. Current registration backends are:

- [ANTs](https://github.com/ANTsX/ANTs)
- [FireANTs](https://fireants.readthedocs.io/en/latest/)
- [Greedy](https://greedy.readthedocs.io/en/latest/)

The Image Registration workflow lets users select fixed and moving images, choose parameters for each backend, inspect
the command preview, monitor progress and logs, and import generated outputs. Imported outputs can include transformed
moving images, affine matrices, inverse warp fields, and forward warp fields. Backend executables are configured in
Application Settings.

### Segmentation, Annotations, and Landmarks

The same views used for image comparison can be used to paint segmentation overlays, draw vector-based annotations, and
place point landmarks:

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

- [Neuroimaging Informatics Technology Initiative (NIfTI)](https://nifti-imaging.github.io/nifti1_overview.html) and Analyze header/image pairs (`.nii`, `.nii.gz`, `.hdr`, `.img`)
- [Nearly Raw Raster Data (NRRD)](https://teem.sourceforge.net/nrrd/format.html) (`.nrrd`, `.nhdr`)
- [MetaImage](https://insightsoftwareconsortium.github.io/ITKWikiArchive/Wiki/ITK/MetaIO/Documentation/) (`.mha`, `.mhd`, with companion raw data `.raw`, `.zraw`, or `.raw.gz`)
- [DICOM](https://www.dicomstandard.org/current/) via [GDCM](https://gdcm.sourceforge.net/) (`.dcm` and DICOM series)
- Standard 2D image formats: JPEG (`.jpg`, `.jpeg`), PNG (`.png`), TIFF (`.tif`, `.tiff`), and BMP (`.bmp`, `.dib`)

Entropy also displays complete image header information, DICOM metadata, and editable spatial geometry for standard 2D
raster images that do not carry medical image headers. It loads the supporting files needed to make a review complete:
segmentations, landmarks, annotations, affine transforms, deformation warp fields, layouts, and project files.

### Multi-Component and Time Series Images

Entropy supports 2D, 3D, and 4D (time series) scalar and multi-component images.

Component types:
- Integer (signed/unsigned 8, 16, and 32 bits)
- Floating-point (32 and 64 bits)

Pixel/voxel types:
- Scalar
- Complex
- RGB and RGBA
- Vector fields (e.g. representing deformable registration warps)
- General multi-component images

Images with multiple components can be viewed by individual component, magnitude, RGB/RGBA color, and as various derived
maps (divergence, curl, and Jacobian determinant). Time series images can be reviewed with per-image and global time
controls.

## Technical Notes

Entropy is a native C++ application built for interactive desktop performance across multiple platforms. Third-party
dependencies are pinned from source.

|  |  |
| --- | --- |
| Platforms | macOS, Windows, Ubuntu, and Fedora |
| Language | C++23 |
| Build system | CMake presets with separate dependency and app stages |
| Toolchains | Apple Clang, MSVC/Visual Studio, and GCC in CI |
| Tests | Unit tests, static analysis, and coverage reports run with GitHub Actions |
| Rendering | OpenGL 3.3 Core with GLSL shaders |
| Image I/O | Medical image loading through [ITK](https://itk.org/) and [GDCM](https://gdcm.sourceforge.net/) |
| UI | [Dear ImGui](https://github.com/ocornut/imgui) with [Docking support](https://github.com/ocornut/imgui/wiki/Docking) and native platform menus and dialog integration |

Entropy targets OpenGL 3.3 Core with GLSL 3.30 shaders to maximize compatibility across platforms and operating systems.
Detailed compiler versions, operating system versions, development packages, and coverage workflows are documented in
[BUILDING.md](BUILDING.md).

### Continuous Integration

GitHub Actions builds and tests Entropy on macOS, Windows, Ubuntu 22.04 and 24.04, and Fedora. It also checks formatting,
spelling, include hygiene, and static-analysis findings. See [BUILDING.md](BUILDING.md) for the current CI matrix and
tooling.

## Core Concepts

### Reference Image

The reference image defines the main project space. Additional images are compared against it and may have affine
transformations or deformable warps that relate them to the reference and each other.

### Active Image

The active image is the image currently targeted by many editing, inspection, transformation, and menu actions.

### Image Geometry and Affine Transformations

Entropy separates image geometry from affine transformations loaded or edited by the user:

1. The image header defines the image's native voxel to subject geometry
2. The initial/imported affine transformation is used for a loaded alignment or an alignment computed by registration
3. The manual affine transformation is intended for interactive adjustment

For standard 2D raster images such as JPEG, PNG, TIFF, and BMP, Entropy asks for spacing, origin, and direction
information when the image is loaded and saves that geometry in the project file.

This separation makes it possible to inspect and revise alignment without losing the original image geometry.

### Deformable Warp Fields

Entropy uses inverse warp fields for image sampling and forward warp fields for moving spatial objects, such as
landmarks and annotations. A project with one image can still apply a warp to that image, treating the image as its own
reference space.

### Application Settings and Project Settings

Application settings store personal UI preferences and backend configuration. Project settings store presentation and
review state that is packaged with a project, such as layouts, comparison settings, 3D rendering settings, segmentation
display defaults, and transformation assignments.

## Quick Start

One typical workflow is to load a reference image and one or more images to compare with it:

1. Open images, a DICOM series, or an existing project from the opening screen, the **File** menu, or the command line.
2. The first image becomes the **reference image** and defines the common coordinate space. You can select a different
   reference image later.
3. The most recently loaded image becomes the **active image**. Many display, editing, and transformation actions apply
   to this image. Choose another active image from the Images panel or with a [keyboard shortcut](#keyboard-shortcuts).
4. Choose a layout and view types for the review or analysis task.
5. Use image visibility, opacity, comparison modes, and the opacity mixer to compare images.
6. Use the voxel inspector to verify coordinates and sampled values.
7. Add segmentations, annotations, landmarks, affine transformations, or warp fields as needed.
8. Save the work as an Entropy JSON project file. The file preserves the image list, derived data,
   transformations, layouts, and non-default project settings.

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
```

Image, DICOM, and project inputs are separate modes and cannot be combined. Positional image paths may be used instead
of `--image`. Run `entropy --help` for the complete command-line reference.

### Project Files

Entropy project files preserve all state needed to reopen a review, except for the image data referenced on disk.
They include a reference image, additional images, segmentations, landmarks, annotations, transformations, layouts, and
presentation settings. Default values are omitted, so missing settings should be interpreted as Entropy defaults. Image
entries are stored in `images`. The first entry is the reference image. Project-wide presentation settings are grouped
under `settings`. Minimal example:

```json
{
  "version": {"major": 1, "minor": 0},
  "images": [{"path": "ref.nii.gz"}, {"path": "mov_1.nii.gz"}, {"path": "mov_2.nii.gz"}]
}
```

> The project format may evolve as Entropy develops.

### Keyboard Shortcuts

The complete, current list is available in **Help > Keyboard Shortcuts**.

| Shortcut | Action |
| --- | --- |
| `V` | Pointer (crosshairs) mode |
| `L` | Window width/level adjustment mode |
| `Z` | Zoom mode |
| `P` | Pan mode |
| `B` | Segmentation brush mode |
| `R` | Active image rotation mode |
| `T` | Active image translation mode |
| `Y` | Active image scaling mode |
| `Left` / `Right` / `Down` / `Up` | Move crosshairs |
| `Page Down` / `Page Up` | Previous/next slice |
| `[` / `]` | Previous/next view layout |
| `Shift + [` / `Shift + ]` | Set previous/next image as active |
| `W` | Toggle active image visibility |
| `Q` / `E` | Reduce/increase active image opacity |
| `S` | Toggle active segmentation visibility |
| `A` / `D` | Reduce/increase active segmentation opacity |
| `C` | Recenter views on crosshairs |
| `Shift + C` | Reset view orientation, zoom, and centering |
| `X` | Toggle crosshairs visibility |
| `U` | Toggle user interface |
| `I` | Toggle voxel inspector |
| `O` | Cycle view overlays |

## Runtime Files

Entropy writes a small number of user-level runtime files outside project files.

### Settings

Entropy persists application settings in the standard location for each platform:

- macOS: `~/Library/Application Support/Entropy/settings.json`
- Windows: `%APPDATA%\Entropy\settings.json`
- Linux: `${XDG_CONFIG_HOME:-~/.config}/entropy/settings.json`

Settings owned by a project are saved in project JSON files instead of the application settings file.

### Logging

Entropy writes logs to the console and to daily log files. Log verbosity can be changed from Application Settings or
with `--log-level`. Default log locations:

- macOS: `~/Library/Logs/Entropy/`
- Windows: `%APPDATA%\Entropy\Logs\`
- Linux: `${XDG_STATE_HOME:-~/.local/state}/entropy/logs/`

## License

Entropy source code and official release packages are distributed under the Apache License 2.0
([LICENSE.txt](LICENSE.txt)). Required project notices are in [NOTICE.txt](NOTICE.txt). Third-party dependency notices
are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
