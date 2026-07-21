# Entropy Project Files

Entropy projects are JSON files that describe an image-review session. A project records which images are loaded, how
they are spatially related, which overlays and derived objects belong to them, and how the views should be restored when
the project is opened again.

The project file is intentionally lightweight. Large data objects stay in their original files and are referenced by
path. This keeps project saves fast and avoids duplicating image, segmentation, warp, annotation, and landmark data.

The implementation is in:

| Code | Purpose |
|---|---|
| `app/logic/serialization/ProjectSerialization.h` | Project data structures |
| `app/logic/serialization/ProjectSerialization.cpp` | Project JSON read/write logic |
| `app/logic/app/ProjectSnapshotSettings.cpp` | Conversion between application state and serialized project settings |
| `lib/layout/LayoutSpecJson.cpp` | Layout JSON read/write logic |

## Format Overview

Entropy stores two kinds of information:

- paths to large or independently editable files
- compact state directly embedded in the project JSON

The main project object is organized around image entries. `reference` is the primary image entry and becomes image
index 0 in the application. `additional` is an array of the other loaded image entries, which become image indices 1,
2, 3, and so on. Each image entry can point to its image file, segmentations, transforms, warp fields, annotations,
landmarks, isosurfaces, and per-image display settings.

The project file references these files by path:

| Stored by path | JSON entry |
|---|---|
| Images | `reference.image`, `additional[].image` |
| Segmentations | `reference.segmentations[].path`, `additional[].segmentations[].path` |
| Initial/imported affine transform files | `reference.affine`, `additional[].affine` |
| Inverse warp fields | `reference.inverseWarp`, `additional[].inverseWarp` |
| Forward warp fields | `reference.forwardWarp`, `additional[].forwardWarp` |
| Annotation JSON files | `reference.annotations`, `additional[].annotations` |
| Landmark CSV files | `reference.landmarks[].path`, `additional[].landmarks[].path` |
| Registration result artifacts | `registrationResults[].manifest`, `registrationResults[].warpedImage`, `registrationResults[].inverseWarp`, `registrationResults[].forwardWarp`, `registrationResults[].affineTransform`, `registrationResults[].warpedSegmentations[]`, `registrationResults[].transformedSurfaces[]`, `registrationResults[].transformedLandmarks[]` |

The project file stores these compact objects directly in JSON:

| Stored directly in the project JSON | JSON entry |
|---|---|
| Manual affine matrices | `reference.manualTransformation`, `additional[].manualTransformation` |
| User-provided 2D raster image geometry | `reference.spatialMetadata`, `additional[].spatialMetadata` |
| DICOM source metadata | `reference.dicomSource`, `additional[].dicomSource` |
| Layout definitions, unless an external layout file is used | `layouts[]` |
| Current layout selection | `currentLayoutIndex` |
| Per-image rendering settings | `reference.settings`, `additional[].settings` |
| Per-segmentation rendering settings | `reference.segmentations[].settings`, `additional[].segmentations[].settings` |
| Isosurface definitions and material/rim lighting settings | `reference.isosurfaces[]`, `additional[].isosurfaces[]` |
| Project-wide interface settings | `interface` |
| Project-wide view settings | `view` |
| Project-wide comparison settings | `comparison` |
| Project-wide raycasting settings | `raycasting` |
| Project-wide intensity projection settings | `intensityProjection` |
| Project-wide segmentation display settings | `segmentationDisplay` |
| Project-wide isosurface/isocontour display settings | `isosurfaces` |
| Project-wide annotation/landmark display settings | `annotationDisplay` |
| Registration result metadata and warnings | `registrationResults[].backend`, `registrationResults[].fixedImageUid`, `registrationResults[].movingImageUid`, `registrationResults[].warnings` |

Paths are saved relative to the project file where possible. On load, relative paths are resolved relative to the
project file.

## Minimal Project

A small project only needs a `reference` image. Additional images are stored in the optional `additional` array:

```json
{
  "reference": {
    "image": "reference.nii.gz"
  },
  "additional": [
    {
      "image": "moving.nii.gz",
      "affine": "moving_to_reference_initial_affine.tfm",
      "inverseWarp": "moving_to_reference_inverse_warp.nii.gz"
    }
  ]
}
```

Most settings blocks are optional. Missing values are filled from the application defaults.

### Multiple Additional Images

Each entry in `additional` has the same image-entry format as `reference`, so a project can contain any number of
additional images in the common project space:

```json
{
  "reference": {
    "image": "reference.nii.gz"
  },
  "additional": [
    {
      "image": "moving_1.nii.gz",
      "affine": "moving_1_initial_affine.tfm"
    },
    {
      "image": "moving_2.nii.gz",
      "inverseWarp": "moving_2_inverse_warp.nii.gz",
      "forwardWarp": "moving_2_forward_warp.nii.gz"
    },
    {
      "image": "derived_3.png",
      "spatialMetadata": {
        "spacingMm": [1.0, 1.0, 1.0],
        "originMm": [0.0, 0.0, 0.0],
        "directions": [
          [1.0, 0.0, 0.0],
          [0.0, 1.0, 0.0],
          [0.0, 0.0, 1.0]
        ]
      }
    }
  ]
}
```

The image order matters. `reference` is image index 0. Entries in `additional` become image indices 1, 2, 3, and so on.
Layout image selections and some saved view state refer to images by these zero-based indices. Reordering images in the
project file can therefore change which images appear in saved views.

## Path Handling

Project files support both absolute and relative paths. When Entropy saves a project, it attempts to write paths
relative to the project file location. When Entropy opens a project, relative paths are resolved against the project
file location.

This applies to image paths, segmentation paths, transforms, warp fields, annotations, landmarks, layout files, DICOM
source paths, and registration result artifact paths.

| Path kind | Supported behavior |
|---|---|
| Relative path | Preferred for portable projects; resolved relative to the project JSON file |
| Absolute path | Supported when a relative path cannot be made or when the project was authored that way |
| Missing path | The project may partially load, but the referenced object cannot be restored |

## External File Formats

The project file can reference several kinds of external files:

| Referenced data | JSON entry | Supported file types |
|---|---|---|
| Images | `reference.image`, `additional[].image` | Medical images and standard 2D raster images listed below |
| Segmentations | `reference.segmentations[].path`, `additional[].segmentations[].path` | Same image file readers as images; normally label images |
| Inverse warp fields | `reference.inverseWarp`, `additional[].inverseWarp` | Image formats readable by Entropy/ITK, normally vector displacement fields |
| Forward warp fields | `reference.forwardWarp`, `additional[].forwardWarp` | Image formats readable by Entropy/ITK, normally vector displacement fields |
| Registration artifacts | `registrationResults[]` path fields | Backend outputs, usually images, transforms, JSON manifests, and derived CSV/mesh/image files |
| Affine transforms | `reference.affine`, `additional[].affine` | Entropy 4x4 text matrices; native dialogs allow `.txt`, `.mat`, and `.tfm` |
| Annotations | `reference.annotations`, `additional[].annotations` | Entropy annotation JSON |
| Landmarks | `reference.landmarks[].path`, `additional[].landmarks[].path` | Entropy landmark CSV |

### Images, Segmentations, and Warp Fields

Entropy uses [ITK](https://itk.org/) for image I/O. The native file dialogs expose these project input formats:

| Format | Long name | Extensions | Notes |
|---|---|---|---|
| [NIfTI](https://nifti.nimh.nih.gov/nifti-1/) | Neuroimaging Informatics Technology Initiative | `.nii`, `.nii.gz` | Common medical volume format |
| Analyze | Analyze 7.5 header/image pair | `.hdr`, `.img`, `.hdr.gz`, `.img.gz` | Header/image pair format |
| [NRRD](https://teem.sourceforge.net/nrrd/format.html) | Nearly Raw Raster Data | `.nrrd`, `.nhdr`, `.nrrd.gz`, `.nhdr.gz` | Common scalar, vector, and metadata-rich format |
| [MetaImage](https://insightsoftwareconsortium.github.io/ITKWikiArchive/Wiki/ITK/MetaIO/Documentation/) | MetaImage | `.mha`, `.mhd`, `.mhd.gz` | `.mhd` may reference companion raw data |
| [DICOM](https://www.dicomstandard.org/current/) | Digital Imaging and Communications in Medicine | `.dcm` or DICOM series folders | Loaded through Entropy's DICOM series path |
| [JPEG](https://jpeg.org/jpeg/) | Joint Photographic Experts Group JPEG 1 | `.jpg`, `.jpeg`, `.jpe` | Standard 2D raster image; user spatial metadata is saved in the project |
| [PNG](https://www.w3.org/TR/png-3/) | Portable Network Graphics | `.png` | Standard 2D raster image; user spatial metadata is saved in the project |
| [TIFF](https://www.loc.gov/preservation/digital/formats/fdd/fdd000022.shtml) | Tagged Image File Format | `.tif`, `.tiff` | Standard 2D raster image; user spatial metadata is saved in the project |
| [BMP/DIB](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapfileheader) | Windows Bitmap / Device-Independent Bitmap | `.bmp`, `.dib` | Standard 2D raster image; user spatial metadata is saved in the project |

Entropy's internal format-name helper also recognizes GIPL (`.gipl`, `.gipl.gz`), MINC (`.mnc`, `.mnc2`, `.mnc.gz`),
VTK image data (`.vtk`, `.vti`), and HDF5 (`.hdf5`). These are delegated to ITK and may work when ITK has the
corresponding reader available, but they are not emphasized in the native image-open filter.

Segmentations use the same image-reading path as images, but Entropy treats them as label images. Warp fields also use
image-reading paths and are expected to be multi-component displacement/vector fields whose domain matches the image or
reference space expected by the assignment.

For non-DICOM image files, Entropy can load scalar images, RGB/RGBA images, complex images, vector or other
multi-component images, and time series when the selected ITK reader exposes that data. Standard 2D raster images
usually do not contain medical spatial metadata, so Entropy stores user-specified `spatialMetadata` in the project file.

### Inverse and Forward Warp Fields

Entropy distinguishes inverse and forward warp fields because they are used for different operations:

| Warp field | Meaning in Entropy | Used for |
|---|---|---|
| Inverse warp | A reference-space sampling field that tells Entropy where to sample the moving image | Rendering a transformed moving image, segmentation rendering, voxel inspection in warped coordinates |
| Forward warp | A moving-space point/object field that maps moving image locations into reference space | Displaying transformed landmarks and annotations |

The inverse warp is the one used for image resampling. The forward warp is for spatial objects. A registration backend
may produce one or both, and Entropy can store paths for both. Keeping the two directions explicit avoids silently using
a deformation field with the wrong interpretation.

### Affine Transform Files

Entropy's manual affine transform loader/writer uses a simple plain-text 4x4 matrix. The file contains four rows, each
with four numeric values:

```text
1 0 0 0
0 1 0 0
0 0 1 0
0 0 0 1
```

The text file is written in row-major visual order. Internally, Entropy stores matrices in GLM column-major layout, so
the reader assigns file row `r`, column `c` into matrix element `[c][r]`.

The native file dialog allows `.txt`, `.mat`, and `.tfm`, but this manual load/save path expects the 4x4 text matrix
format shown above. Registration backend artifacts may use backend-native transform formats. Those artifacts are
recorded by path in `registrationResults` and imported through backend-specific logic when supported.

### Annotation JSON Files

Annotation files are JSON arrays of annotation objects. Each object stores display settings, the subject-space plane,
and a 2D polygon drawn in that plane:

| JSON key | Required | Saved content |
|---|---|---|
| `name` | No | Annotation display name |
| `visible` | No | Visibility |
| `opacity` | No | Annotation opacity |
| `lineThickness` | No | Line thickness |
| `lineColor` | No | RGBA line color |
| `fillColor` | No | RGBA fill color |
| `verticesVisible` | No | Whether polygon vertices are displayed |
| `closed` | No | Whether the polygon is closed |
| `filled` | No | Whether the polygon is filled |
| `smoothed` | No | Whether polygon smoothing is enabled |
| `smoothingFactor` | No | Smoothing amount |
| `subjectPlaneNormal` | Yes | Subject-space plane normal |
| `subjectPlaneOffset` | Yes | Subject-space plane offset |
| `subjectPlaneOrigin` | Written | Subject-space origin used when the annotation was saved |
| `subjectPlaneAxes` | Written | Two subject-space axes spanning the annotation plane |
| `polygon` | Yes | Array of 2D vertices in annotation-plane coordinates |

The polygon is stored as an array of `[x, y]` coordinate pairs. The reader requires `subjectPlaneNormal`,
`subjectPlaneOffset`, and `polygon`; most display fields are optional and fall back to defaults. `subjectPlaneOrigin`
and `subjectPlaneAxes` are written for context and future compatibility.

### Landmark CSV Files

Landmark groups are stored as CSV files. Entropy writes this header:

```csv
ID,X,Y,Z,Name
```

Each data row contains:

| Column | Meaning |
|---|---|
| `ID` | Non-negative landmark index |
| `X` | X coordinate |
| `Y` | Y coordinate |
| `Z` | Z coordinate |
| `Name` | Landmark name |

Entropy writes all five columns, including `Name`. The reader parses `ID`, `X`, `Y`, and `Z`, and treats a fifth column
as the landmark name. The project file's `inVoxelSpace` flag records whether these coordinates should be interpreted as
image voxel coordinates or physical/subject coordinates.

Landmark CSV files are plain comma-separated files. Landmark names should not contain commas.

## DICOM Support

Entropy reads DICOM through ITK's GDCM backend. Relevant standard references are the current
[DICOM Standard](https://www.dicomstandard.org/current/), PS3.3
[Information Object Definitions](https://dicom.nema.org/medical/DICOM/current/output/chtml/part03/PS3.3.html), PS3.6
[Data Dictionary](https://dicom.nema.org/medical/DICOM/current/output/chtml/part06/PS3.6.html), the PS3.3
[Image Plane Module](https://dicom.nema.org/medical/DICOM/current/output/chtml/part03/sect_C.7.6.2.html), and the PS3.3
[Multi-frame Module](https://dicom.nema.org/medical/DICOM/current/output/chtml/part03/sect_C.7.6.6.html).

Entropy's DICOM support is series-oriented:

| Capability | Current behavior |
|---|---|
| Discovery | Scans folders recursively, or scans the parent folder when given a DICOM file |
| Series grouping | Uses GDCM series discovery and records the selected Series Instance UID |
| Loaded image data | Scalar image volumes |
| Single-file multiframe/cine | Supported for scalar data when a single DICOM object contains multiple frames |
| Time metadata | Uses Number of Frames `(0028,0008)`, Frame Time `(0018,1063)`, or Cine Rate `(0018,0040)` when present |
| Geometry | Uses ITK/GDCM image geometry derived from DICOM metadata |
| Display metadata | Shows a privacy-filtered DICOM metadata table in the UI |
| Private tags | Hidden from the display summary by default |
| PHI tags | Hidden from the display summary |
| Unsupported DICOM object types | RTSTRUCT, RTPLAN, RTDOSE, SEG, SR, and PR are flagged as non-image modalities |

The geometry loaded by Entropy depends on the image information exposed by ITK/GDCM. Important standard tags include:

| DICOM tag | Name | Entropy use |
|---|---|---|
| `(0020,000D)` | Study Instance UID | Saved in `dicomSource.studyInstanceUid` |
| `(0020,000E)` | Series Instance UID | Saved in `dicomSource.seriesInstanceUid` and used to rediscover the series |
| `(0028,0010)` | Rows | Image height |
| `(0028,0011)` | Columns | Image width |
| `(0028,0030)` | Pixel Spacing | In-plane spacing |
| `(0020,0032)` | Image Position Patient | Physical origin/slice positions through ITK/GDCM |
| `(0020,0037)` | Image Orientation Patient | Row/column direction cosines through ITK/GDCM |
| `(0018,0050)` | Slice Thickness | Display metadata and geometry context |
| `(0018,0088)` | Spacing Between Slices | Display metadata and geometry context |
| `(0028,0002)` | Samples per Pixel | Display metadata; Entropy's DICOM series loader currently expects scalar images |
| `(0028,0004)` | Photometric Interpretation | Display metadata |
| `(0028,0100)` | Bits Allocated | Display metadata and ITK pixel interpretation |
| `(0028,0101)` | Bits Stored | Display metadata |
| `(0028,0103)` | Pixel Representation | Display metadata and ITK pixel interpretation |
| `(0028,1052)` | Rescale Intercept | Display metadata; image values are read through ITK/GDCM |
| `(0028,1053)` | Rescale Slope | Display metadata; image values are read through ITK/GDCM |

The project records enough DICOM source information to rediscover the selected series later:

| Project JSON entry | Meaning |
|---|---|
| `dicomSource.root` | Folder used for series discovery |
| `dicomSource.studyInstanceUid` | Study UID saved for disambiguation/context |
| `dicomSource.seriesInstanceUid` | Series UID used to identify the selected series |
| `dicomSource.files` | Slice files saved in series order |

## Project JSON Reference

The sections below list the JSON objects saved directly in the project file. They are intended as a readable reference,
not as a formal JSON schema.

## Top-Level Object

| JSON key | Saved content |
|---|---|
| `reference` | The reference image entry |
| `additional` | Additional image entries, if any |
| `layoutsFile` | Optional external layout file path |
| `layouts` | Embedded layout definitions when no external layout file is used |
| `currentLayoutIndex` | Currently selected layout index |
| `interface` | Project-level interface behavior |
| `view` | Project-level view overlay/display behavior |
| `comparison` | Comparison and metric display settings |
| `raycasting` | 3D raycasting settings |
| `intensityProjection` | MIP/mean/min/X-ray projection settings |
| `segmentationDisplay` | Global segmentation display settings |
| `isosurfaces` | Global isosurface/isocontour display settings |
| `annotationDisplay` | Global annotation/landmark display settings |
| `registrationResults` | Completed registration result artifact paths and warnings |

## Image Entry

Each image entry describes one loaded image and the data attached to it. The reference image and every entry in
`additional` use this same object shape.

| JSON key | Saved content |
|---|---|
| `image` | Image file path |
| `spatialMetadata` | User-provided spacing, origin, and directions for standard 2D raster images |
| `dicomSource` | DICOM source metadata for reloading a DICOM series |
| `affine` | Initial/imported affine transform path |
| `inverseWarp` | Inverse warp path |
| `inverseWarpReferenceImage` | Image domain/reference path for the inverse warp |
| `forwardWarp` | Forward warp path |
| `manualTransformation` | Manual affine matrix embedded as a 4x4 matrix |
| `annotations` | Annotation JSON file path |
| `segmentations` | Segmentation image entries |
| `landmarks` | Landmark group CSV entries |
| `isosurfaces` | Per-image, per-component isosurface definitions |
| `settings` | Per-image display/rendering settings |

## Image Spatial Metadata

`spatialMetadata` is used for standard 2D raster images such as PNG, JPEG, TIFF, or BMP when the file does not provide
medical image geometry. Medical image formats such as NIfTI, NRRD, MetaImage, Analyze, and DICOM normally provide their
own geometry through the image header or DICOM metadata.

| JSON key | Saved content |
|---|---|
| `spacingMm` | Image axis spacing in millimeters |
| `originMm` | Physical coordinate of voxel `(0, 0, 0)` |
| `directions` | 3x3 image axis direction matrix, stored as direction columns |

The three direction columns correspond to image axes `i`, `j`, and `k`. For a standard 2D raster image, the `k`
direction is inferred from the edited in-plane directions so that the saved matrix remains orthonormal.

## DICOM Source

`dicomSource` allows a project to reload a DICOM series as a series, rather than treating one slice as a standalone
file.

| JSON key | Saved content |
|---|---|
| `root` | Root folder used to discover the series |
| `studyInstanceUid` | DICOM Study Instance UID |
| `seriesInstanceUid` | DICOM Series Instance UID |
| `files` | Slice file paths in series order |

## Segmentation Entry

Segmentations are stored as external image files. The project records their paths and optional display settings.

| JSON key | Saved content |
|---|---|
| `path` | Segmentation image path |
| `settings` | Optional per-segmentation display settings |

## Landmark Group

Landmarks are stored in external CSV files. The project records the path and whether coordinates are image voxel
coordinates or physical coordinates.

| JSON key | Saved content |
|---|---|
| `path` | Landmark CSV path |
| `inVoxelSpace` | Whether coordinates are voxel-space rather than physical/subject-space |

## Per-Image Settings

The image `settings` block stores display state. It does not duplicate image header metadata or voxel data.

| Setting group | JSON keys saved |
|---|---|
| Identity/display | `displayName`, `globalVisibility`, `globalOpacity`, `borderColor` |
| Transform/warp display | `lockedToReference`, `warpEnabled`, `warpStrength`, `allowExaggeratedWarp` |
| Window/threshold/opacity | `level`, `window`, `thresholdLow`, `thresholdHigh`, `opacity` |
| Component/time state | `activeComponent`, `activeTimePoint`, `timePlaybackLoop`, `timePlaybackPlaying`, `timePlaybackSpeed` |
| Multi-component rendering | `componentRenderMode`, `complexPhaseUnit`, `complexPhaseRange`, `ignoreAlpha`, `colorInterpolationMode` |
| Vector arrows | `vectorArrowOverlayVisible`, `vectorArrowOverlayOnImage`, `vectorArrowOverlayDensity`, `vectorArrowOverlayVoxelSpacing`, `vectorArrowOverlayMillimeterSpacing`, `vectorArrowOverlaySpacingMode`, `vectorArrowOverlayColor`, `vectorArrowOverlayUseDirectionColor`, `vectorArrowOverlayLineThickness`, `vectorArrowOverlayOpacity`, `vectorArrowOverlayScaleByMagnitude`, `vectorArrowOverlayScaleFactor` |
| Warped grid | `vectorWarpedGridVisible`, `vectorWarpedGridOverlayOnImage`, `vectorWarpedGridConvention`, `vectorWarpedGridPixelSpacing`, `vectorWarpedGridVoxelSpacing`, `vectorWarpedGridMillimeterSpacing`, `vectorWarpedGridSpacingMode`, `vectorWarpedGridLineThickness`, `vectorWarpedGridScaleFactor`, `vectorWarpedGridForegroundColor`, `vectorWarpedGridBackgroundColor` |
| Vector-derived rendering | `vectorPlanarProjectionSignedColors`, `vectorLogJacobianDeterminant` |
| Per-component arrays | `componentLevels`, `componentWindows`, `componentThresholdLows`, `componentThresholdHighs`, `componentVisibility`, `componentOpacities` |
| Per-component colormaps | `colorMapIndices`, `colorMapInverted`, `colorMapContinuous`, `colorMapLevels`, `colorMapHsvModifiers` |
| Per-component interpolation | `interpolationModes` |
| Distance-map thresholds | `foregroundThresholdLows`, `foregroundThresholdHighs` |
| Edge rendering | `edgeDetectionMethod`, `showEdges`, `hardEdges`, `thinPixelEdges`, `overlayEdges`, `colormapEdges`, `edgeMagnitude`, `pixelEdgeScale`, `pixelEdgeThreshold`, `edgeColor`, `edgeOpacity` |
| Isosurfaces/isocontours | `useDistanceMapForRaycasting`, `isosurfacesVisible`, `applyImageColormapToIsosurfaces`, `showIsocontoursIn2D`, `isocontourLineWidthIn2D`, `isosurfaceOpacityModulator` |

## Per-Segmentation Settings

The segmentation `settings` block stores display state for a segmentation attached to an image.

| JSON key | Saved content |
|---|---|
| `displayName` | Segmentation display name |
| `visibility` | Segmentation visibility |
| `opacity` | Segmentation opacity |
| `activeComponent` | Active segmentation component |
| `componentVisibility` | Per-component visibility |
| `componentOpacities` | Per-component opacity |
| `labelTableIndices` | Per-component label table indices |
| `interpolationModes` | Per-component interpolation modes |

## Isosurface Definition

Isosurface definitions are saved in the project because they are small user-editable descriptions. Generated meshes,
distance maps, and GPU resources are rebuilt as needed.

| JSON key | Saved content |
|---|---|
| `component` | Image component index |
| `surface.name` | Isosurface name |
| `surface.value` | Isovalue |
| `surface.color` | Surface color |
| `surface.opacity` | Surface opacity |
| `surface.fillOpacity` | 2D contour fill opacity |
| `surface.visible` | Visibility |
| `surface.showIn2d` | Whether to show 2D isocontours |
| `surface.material` | Ambient, diffuse, specular, and shininess material settings |
| `surface.rimLightingEnabled` | 3D rim lighting enable flag |
| `surface.rimOpacityStrength` | Rim opacity contribution |
| `surface.rimEmissionStrength` | Rim glow/emission contribution |
| `surface.rimPower` | Rim falloff exponent |

## Layouts

Layouts can be embedded directly in the project or referenced through `layoutsFile`. A layout describes the arrangement
of views and their default rendering state.

| JSON key | Saved content |
|---|---|
| `kind` | Layout kind: custom, four-up, three-up, one-up, grid, lightbox, etc |
| `displayName` | Layout name, if present |
| `isLightbox` | Whether layout is lightbox-style |
| `viewType` | Default view type |
| `renderMode` | Default render mode |
| `intensityProjectionMode` | Default projection mode |
| `preferredDefaultRenderedImages` | Preferred image indices |
| `defaultRenderAllImages` | Whether generated views render all images by default |
| `imageSelection` | Layout-level rendered, volume-rendered, and metric image selections |
| `views` | Per-view specs |

## Per-View Layout State

Each entry in `views` describes one viewport in the layout. View rectangles are stored in normalized layout coordinates,
not screen pixels.

| JSON key | Saved content |
|---|---|
| `viewport` | Normalized view rectangle: `left`, `bottom`, `width`, `height` |
| `viewType` | Axial, coronal, sagittal, oblique, or 3D |
| `renderMode` | Image, overlay, difference, checkerboard, flashlight, volume render, etc |
| `intensityProjectionMode` | None, maximum, mean, minimum, or X-ray |
| `offset` | Offset mode, absolute offset, relative steps, offset image index |
| `sync` | Rotation, translation, and zoom sync source groups |
| `syncMembership` | Rotation, translation, and zoom sync membership groups |
| `preferredDefaultRenderedImages` | Preferred image indices for this view |
| `defaultRenderAllImages` | Whether this view defaults to all images |
| `imageSelection` | Rendered, volume-rendered, and metric image index selections |
| `threeD` | 3D projection type, orbit target mode, camera-follow flag, perspective zoom, orthographic zoom |

## Project-Wide Settings

Project-wide settings preserve rendering and display choices that are part of the image-review state. Application
preferences such as recent files and window positions are not stored here.

| JSON block | JSON keys saved |
|---|---|
| `interface` | `synchronizeTimeSeries` |
| `view` | `showImageBorders`, `showImageBordersInLightboxViews`, `showCrosshairs`, `showCrosshairsInLightboxViews`, `showAnatomicalLabels`, `showAnatomicalLabelsInLightboxViews`, `showScaleBars`, `showScaleBarsInLightboxViews`, `anatomicalLabelType`, `lockAnatomicalDirectionsToReferenceImage`, `crosshairsSnapping` |
| `comparison.difference` | `squared`, `metric` |
| `comparison.localNormalizedCrossCorrelation` | `metric`, `presentation`, `negativeCorrelationAsMismatch`, `patchRadius`, `sampleSpacing`, `minimumValidFraction`, `varianceEpsilon`, `invalidStyle` |
| `comparison.localLinearResidual` | `metric`, `patchRadius`, `sampleSpacing`, `minimumValidFraction`, `varianceEpsilon`, `invalidStyle` |
| `comparison.overlay` | `magentaCyan` |
| `comparison.quadrants` | `x`, `y` |
| `comparison.checkerboard` | `squares` |
| `comparison.flashlight` | `radiusFraction`, `overlayMovingImage` |
| `raycasting` | `samplingFactor`, `transparentBackgroundWhenNoHit`, `backgroundEdgeBrighteningEnabled`, `showCrosshairsIn3D`, `crosshairs3DGlyphDiameterVoxelDiagonals`, `showThreeDCameraFrustumIn2DViews`, `reverseThreeDRotateAboutEye`, `threeDCameraFrustumColor`, `renderFrontFaces`, `renderBackFaces`, `segmentationMasking` |
| `intensityProjection` | `useMaximumImageExtent`, `slabThicknessMm`, `xrayEnergyKeV`, `xrayWindow`, `xrayLevel` |
| `segmentationDisplay` | `modulateOpacityWithImageOpacity`, `outlineStyle`, `interiorOpacity`, `erosionFactor` |
| `isosurfaces` | `floatingPointInterpolationPolicy`, `modulateOpacityWithImageOpacity` |
| `annotationDisplay` | `annotationsOnTop`, `landmarksOnTop`, `hideAnnotationVertices` |

## Registration Results

Completed registration results are saved as artifact references. The project records where the backend outputs were
written and which fixed/moving image UIDs were involved.

| JSON key | Saved content |
|---|---|
| `backend` | Backend name |
| `fixedImageUid` | Fixed/reference image UID at registration time |
| `movingImageUid` | Moving image UID at registration time |
| `manifest` | Result manifest path |
| `warpedImage` | Warped image path |
| `inverseWarp` | Inverse warp path |
| `forwardWarp` | Forward warp path |
| `affineTransform` | Affine/composite transform output path |
| `warpedSegmentations` | Warped segmentation output paths |
| `transformedSurfaces` | Transformed surface output paths |
| `transformedLandmarks` | Transformed landmark output paths |
| `warnings` | Non-fatal warnings from backend/import |

## Data Not Saved in the Project File

| Not saved | Where it lives / why |
|---|---|
| Image voxel data | External image files referenced by path |
| Segmentation voxel data | External segmentation image files referenced by path |
| Annotation geometry | External annotation JSON referenced by path |
| Landmark coordinates | External CSV files referenced by path |
| Warp field voxel data | External warp images referenced by path |
| Initial/imported affine contents | External affine transform file referenced by path |
| Derived GPU textures, distance maps, projections, cached magnitudes | Recomputed runtime/cache state |
| In-progress registration jobs and live logs | Runtime job state, not part of saved project snapshot |
| Application preferences | Saved separately in user preferences JSON, not in project JSON |
| Recent files, projects, and DICOM series | App preferences, not project state |
| Window positions and docking UI state | App/UI preferences, not project data |

## Practical Notes

| Topic | Current behavior |
|---|---|
| Path handling | Project save converts paths to relative paths where possible; project load resolves them against the project path |
| Optional fields | Many nested fields are omitted when unset or empty; load fills missing values from defaults |
| Referenced files | Moving or deleting referenced files can make a project partially unloadable |
| Project versioning | There is no explicit project schema/version key yet |
| Layout versioning | External layout files have a separate versioned format |
| Portability | A project is most portable when image data and derived artifacts are stored near the project file |
