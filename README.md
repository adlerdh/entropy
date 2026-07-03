# ENTROPY MEDICAL IMAGE VIEWER
*A cross-platform tool for interactively visualizing, comparing, segmenting, and annotating 3D medical images.*

Entropy is primarily developed and maintained by Daniel H. Adler, Ph.D.

Copyright 2021-2026 Daniel H. Adler, Ph.D. and the Penn Image Computing and Science Lab (PICSL), University of Pennsylvania. All rights reserved.

## Documentation

Start here, then use the focused documents for the details you need:

| Document | What it covers |
| --- | --- |
| [README.md](README.md) | Project overview, running Entropy, project-file examples, and keyboard shortcuts. |
| [BUILDING.md](BUILDING.md) | Supported platforms, required tools, CMake options, build presets, and development build commands. |
| [PACKAGING.md](PACKAGING.md) | Release package commands for Linux and macOS, output locations, package testing, signing notes, and platform packaging details. |
| [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) | Third-party dependency versions, notices, and licenses. |
| [LICENSE.txt](LICENSE.txt) | Entropy license terms. |

## Running
Entropy can load images directly from command line arguments or from a JSON project file.

1. Images can be provided directly with repeatable `--image` options. If an image has one or more accompanying segmentations, place `--seg` immediately after that image; the segmentations are attached to the preceding image in the order provided. `--seg` may be repeated, or it may be followed by multiple segmentation file names. e.g.:

```sh
entropy --image reference_image.nii.gz --seg reference_seg1.nii.gz reference_seg2.nii.gz \
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

3. Layouts can be loaded from a standalone JSON file with `--layouts`. This overrides the default/project layouts after the project or image inputs are loaded. Layout files can also be loaded and saved from the `Layouts` menu.

Logs are output to the console and to files saved in the `log` folder. Log level can be set using the `-l` argument. See help (`-h`) for more details.

### Settings persistence
Entropy persists application settings between sessions in the standard user configuration location for each platform:

- macOS: `~/Library/Application Support/Entropy/settings.json`
- Linux: `$XDG_CONFIG_HOME/entropy/settings.json`, or `~/.config/entropy/settings.json`
- Windows: `%APPDATA%\Entropy\settings.json`

## Keyboard shortcuts

On macOS, use Command instead of CTRL for application menu shortcuts.

### File actions
| Key | Action |
| --- | --- |
| `CTRL + O` | Open image |
| `CTRL + SHIFT + O` | Open project |
| `CTRL + S` | Save project |
| `CTRL + SHIFT + S` | Save project as |
| `CTRL + Q` | Quit Entropy |

### Modes
| Key | Mode | Controls |
| --- | --- | --- |
| `v` | Crosshairs | - left button: move crosshairs<br>- ctrl + left button: rotate crosshairs<br>- middle button: pan image<br>- right button: zoom image |
| `z` | Zoom | - left button: zoom to crosshairs<br>- right button: zoom to cursor pointer |
| `x` | Pan/dolly | - left button: pan image in plane<br>- right button: dolly in/out of plane (3D views only) |
| `l` | Image adjustment | - left button: adjust image window left/right and level up/down<br>- shift + right button: adjust image opacity |
| `t` | Image translation | - left button: translate image in plane<br>- right button: translate image in/out of plane |
| `r` | Image rotation | - left button: rotate image in plane<br>- right button: rotate image in/out of plane |
| `y` | Image scaling | - left button: scale image in plane about crosshairs<br>- shift + left button: isotropic scale<br>- alt/option + left button: constrain to view-horizontal or view-vertical scale |
| `b` | Image segmentation (brush) | - left button: paint foreground label<br>- right button or shift: paint background label |

### View properties
| Key | Action |
| --- | --- |
| `w` | Toggle image visibility |
| `s` | Toggle segmentation visibility |
| `q` | Decrease active image opacity |
| `e` | Increase active image opacity |
| `shift+e` | Toggle image edges |
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
