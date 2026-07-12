#pragma once

#include "common/Types.h"

#include "rendering/utility/containers/VertexAttributeInfo.h"
#include "rendering/utility/containers/VertexIndicesInfo.h"
#include "rendering/utility/gl/GLBufferObject.h"
#include "rendering/utility/gl/GLBufferTexture.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/gl/GLVertexArrayObject.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <uuid.h>

#include <optional>
#include <unordered_map>
#include <vector>

/**
 * @brief GPU resources and renderer-wide presentation state owned by the application renderer.
 *
 * RenderData is intentionally a state container: it stores OpenGL objects, texture records, cached shader uniforms, and
 * rendering options copied from app/project settings. Rendering passes read and update this object while drawing views.
 */
struct RenderData
{
  /// Display transform applied to a local normalized cross-correlation value before color mapping.
  enum class LocalNccPresentation
  {
    Dissimilarity, //!< Show poor local agreement as high values.
    Correlation    //!< Show local NCC values mapped from [-1, 1] to [0, 1].
  };

  /// Rendering style for local patch metric samples that do not have enough valid data.
  enum class LocalNccInvalidStyle
  {
    Transparent, //!< Hide patches that cannot produce a stable NCC value.
    Gray         //!< Draw unstable NCC patches as neutral gray.
  };

  /**
   * @brief Cached shader uniforms for one image or image component.
   *
   * These values are rebuilt from the image header, display settings, transforms, and derived texture metadata before
   * drawing. The renderer uploads them to image, comparison, segmentation, edge, and raycast shader programs.
   */
  struct ImageUniforms
  {
    glm::vec2 cmapSlopeIntercept{1.0f, 0.0f};  //!< Slope/intercept applied before color-map lookup.
    int cmapQuantLevels = 0;                   //!< Number of color-map quantization levels; 0 means continuous.
    glm::vec3 hsvModFactors{0.0f, 1.0f, 1.0f}; //!< HSV offsets/scales applied to the sampled color.

    glm::mat4 imgTexture_T_world{1.0f}; //!< Transform from world/LPS coordinates to image texture coordinates.
    glm::mat4 world_T_imgTexture{1.0f}; //!< Transform from image texture coordinates to world/LPS coordinates.

    glm::mat4 segTexture_T_world{1.0f}; //!< Transform from world/LPS coordinates to segmentation texture coordinates.

    glm::vec3 voxelSpacing{1.0f}; //!< Image voxel spacing in millimeters.

    glm::vec3 subjectBoxMinCorner{0.0f}; //!< Minimum corner of the image axis-aligned bounding box in subject space.
    glm::vec3 subjectBoxMaxCorner{0.0f}; //!< Maximum corner of the image axis-aligned bounding box in subject space.

    /// Columns hold one-texel texture-coordinate steps: (1/dimX, 0, 0), (0, 1/dimY, 0), and (0, 0, 1/dimZ).
    glm::mat3 textureGradientStep{1.0f};

    glm::vec2 slopeIntercept_normalized_T_texture{1.0f, 0.0f}; //!< Texture-to-normalized intensity with window/level.

    /// Per-channel texture-to-normalized intensity slopes/intercepts for color images, including window/level.
    std::vector<glm::vec2> slopeInterceptRgba_normalized_T_texture{
      {1.0f, 0.0f},
      {1.0f, 0.0f},
      {1.0f, 0.0f},
      {1.0f, 0.0f}};

    float slope_native_T_texture{1.0f};          //!< Texture-to-native intensity scale, without window/level.
    glm::vec2 largestSlopeIntercept{1.0f, 0.0f}; //!< Slope/intercept for the widest supported intensity window.

    /// Native scalar image intensity range.
    glm::vec2 minMax{0.0f, 1.0f};

    /// Scalar lower/upper display thresholds in native intensity units.
    glm::vec2 thresholds{0.0f, 1.0f};

    /// Per-channel lower/upper display thresholds for color images.
    std::vector<glm::vec2> thresholdsRgba{{0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}};

    /// Per-channel native intensity ranges for color images.
    std::vector<glm::vec2> minMaxRgba{{0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}};

    float imgOpacity{0.0f}; //!< Scalar image opacity.

    /// Per-channel opacity for color images.
    std::vector<float> imgOpacityRgba = {0.0f, 0.0f, 0.0f, 0.0f};

    float segOpacity{0.0f}; //!< Segmentation opacity.

    bool showEdges = false;     //!< Render image-space edge overlay for this image.
    bool thresholdEdges = true; //!< Suppress edge samples below the configured edge magnitude.
    float edgeMagnitude = 0.0f; //!< Edge magnitude threshold used by edge shaders.
    bool overlayEdges = false;  //!< Composite edges over image intensity instead of replacing the image.
    bool colormapEdges = false; //!< Color edges by the current image color map instead of a fixed color.
    glm::vec4 edgeColor{0.0f};  //!< Fixed edge color as premultiplied RGBA.

    bool showPixelEdges = false;      //!< Render screen-space pixel-edge post-process overlay for this image.
    bool thresholdPixelEdges = false; //!< Suppress pixel edges below the configured post-process threshold.
    bool thinPixelEdges = true;       //!< Use thin pixel-edge rendering instead of thicker edge emphasis.
    float pixelEdgeScale = 2.0f;      //!< Pixel-edge post-process scale factor.
    float pixelEdgeThreshold = 0.2f;  //!< Pixel-edge post-process threshold.
    bool overlayPixelEdges = false;   //!< Composite pixel edges over image intensity instead of replacing the image.
  };

  /// OpenGL texture target used for an uploaded image or segmentation texture.
  enum class TextureDimension
  {
    Texture3D, //!< Texture is uploaded as GL_TEXTURE_3D.
    Texture2D  //!< Planar texture is uploaded as GL_TEXTURE_2D because it exceeds 3D texture limits.
  };

  /**
   * @brief Texture dimensionality and axis mapping for images that may be uploaded as 2D or 3D textures.
   *
   * For 3D textures, `axes` is ignored. For 2D textures, `axes` stores the two image axes represented by the texture
   * s/t coordinates, such as (0, 1) for an XY plane or (1, 2) for a YZ plane.
   */
  struct PlanarTextureLayout
  {
    TextureDimension dimension = TextureDimension::Texture3D;
    glm::ivec2 axes{0, 1};
  };

  /// Shared indexed quad geometry used by 2D image, segmentation, metric, and raycast screen-space passes.
  struct Quad
  {
    Quad();

    VertexAttributeInfo m_positionsInfo;
    VertexIndicesInfo m_indicesInfo;

    GLBufferObject m_positionsObject;
    GLBufferObject m_indicesObject;

    GLVertexArrayObject m_vao;
    GLVertexArrayObject::IndexedDrawParams m_vaoParams;
  };

  /// Shared indexed circle geometry used by circular overlays such as brush previews.
  struct Circle
  {
    Circle();

    VertexAttributeInfo m_positionsInfo;
    VertexIndicesInfo m_indicesInfo;

    GLBufferObject m_positionsObject;
    GLBufferObject m_indicesObject;

    GLVertexArrayObject m_vao;
    GLVertexArrayObject::IndexedDrawParams m_vaoParams;
  };

  /**
   * @brief CPU and GPU state for the transient segmentation brush preview texture.
   *
   * The preview texture is rebuilt while painting so the brush footprint can be rendered before voxels are committed to
   * the segmentation image. Only one of the integer data buffers is populated, according to `componentType`.
   */
  struct BrushPreview
  {
    bool visible = false;                               //!< Whether the preview should be drawn.
    uuids::uuid segUid;                                 //!< Segmentation receiving the preview.
    uuids::uuid imageUid;                               //!< Image that defines the preview geometry.
    ComponentType componentType = ComponentType::UInt8; //!< Integer component type used by the preview buffer.
    glm::uvec3 size{1};                                 //!< Preview data size in voxels.
    glm::uvec3 textureCapacity{0};                      //!< Allocated GPU texture capacity in texels.
    glm::mat4 texture_T_world{1.0f};  //!< Transform from world/LPS coordinates to preview texture space.
    glm::mat4 voxel_T_world{1.0f};    //!< Transform from world/LPS coordinates to preview voxel space.
    glm::vec4 color{1.0f};            //!< Preview color as non-premultiplied RGBA.
    bool allowFill = true;            //!< Whether the preview fills pixels or renders outline-only.
    std::vector<uint8_t> dataU8;      //!< Preview voxel buffer for UInt8 segmentations.
    std::vector<uint16_t> dataU16;    //!< Preview voxel buffer for UInt16 segmentations.
    std::vector<uint32_t> dataU32;    //!< Preview voxel buffer for UInt32 segmentations.
    std::optional<GLTexture> texture; //!< Optional GPU texture backing the preview.
  };

  RenderData();

  /**
   * @brief Set the photon energy used for x-ray intensity projection mode.
   * @param energyKeV Monoenergetic x-ray photon energy in keV.
   */
  void setXrayEnergy(float energyKeV);

  /// Shared full-viewport/image-plane quad.
  Quad m_quad;

  /// Shared circle mesh used by overlay rendering.
  Circle m_circle;

  /// Uploaded image textures keyed by image UID, with one texture per renderable image component.
  std::unordered_map<uuids::uuid, std::vector<GLTexture> > m_imageTextures;

  /// Texture dimensionality and axis mapping for each image texture.
  std::unordered_map<uuids::uuid, PlanarTextureLayout> m_imageTextureLayouts;

  /// Distance-map textures keyed by image UID and image component.
  std::unordered_map<uuids::uuid, std::unordered_map<uint32_t, GLTexture> > m_distanceMapTextures;

  /// Uploaded segmentation textures keyed by segmentation UID.
  std::unordered_map<uuids::uuid, GLTexture> m_segTextures;

  /// Texture dimensionality and axis mapping for each segmentation texture.
  std::unordered_map<uuids::uuid, PlanarTextureLayout> m_segTextureLayouts;

  /// Transient brush preview textures keyed by segmentation UID.
  std::unordered_map<uuids::uuid, BrushPreview> m_brushPreviews;

  /// Per-segmentation label table buffer textures.
  std::unordered_map<uuids::uuid, GLBufferTexture> m_labelBufferTextures;

  /// Uploaded color-map textures keyed by color-map UID.
  std::unordered_map<uuids::uuid, GLTexture> m_colormapTextures;

  /// Black transparent fallback image texture bound when a shader needs an empty image input.
  GLTexture m_blankImageBlackTransparentTexture;

  /// White opaque fallback image texture bound when a shader needs an opaque empty image input.
  GLTexture m_blankImageWhiteOpaqueTexture;

  /// Empty fallback segmentation texture bound when no segmentation texture is available.
  GLTexture m_blankSegTexture;

  /// Empty fallback distance-map texture bound when no distance map is available.
  GLTexture m_blankDistMapTexture;

  /// Cached image uniforms keyed by image UID.
  std::unordered_map<uuids::uuid, ImageUniforms> m_uniforms;

  /// Crosshairs snapping mode used when interactions update the crosshairs position.
  CrosshairsSnapping m_snapCrosshairs;

  /// Whether segmentation opacity is multiplied by the corresponding image opacity.
  bool m_modulateSegOpacityWithImageOpacity;

  /// Whether isocontour opacity is multiplied by the corresponding image opacity.
  bool m_modulateIsocontourOpacityWithImageOpacity;

  /// Whether grayscale image rendering uses floating-point interpolation instead of the 8-bit fixed-point path.
  bool m_imageGrayFloatingPointInterpolation;

  /// Whether isocontour rendering uses floating-point interpolation instead of the 8-bit fixed-point path.
  bool m_isocontourFloatingPointInterpolation;

  /// Whether image opacity controls are interpreted as pairwise opacity-mix controls.
  bool m_opacityMixMode;

  /// Intensity projection slab thickness in millimeters.
  float m_intensityProjectionSlabThickness;

  /// Whether intensity projection samples across the maximum image extent instead of the configured slab thickness.
  bool m_doMaxExtentIntensityProjection;

  /// Window width used for contrast adjustment in X-ray intensity projection mode.
  float m_xrayIntensityWindow;

  /// Window level used for contrast adjustment in X-ray intensity projection mode.
  float m_xrayIntensityLevel;

  /// Monoenergetic photon energy, in keV, used by X-ray intensity projection mode.
  float m_xrayEnergyKeV;

  /// Linear attenuation coefficient of water, in cm^-1, for the current X-ray photon energy.
  float m_waterMassAttenCoeff;

  /// Linear attenuation coefficient of air, in cm^-1, for the current X-ray photon energy.
  float m_airMassAttenCoeff;

  /// Background clear color for 2D views.
  glm::vec3 m_2dBackgroundColor;

  /// Raycast background color for 3D views as non-premultiplied RGBA.
  glm::vec4 m_3dBackgroundColor;

  /// Whether a 3D view remains transparent when an eye ray does not hit the image bounding box.
  bool m_3dTransparentIfNoHit;

  /// Crosshairs color as non-premultiplied RGBA.
  glm::vec4 m_crosshairsColor;

  /// Whether crosshairs are rendered in 2D views.
  bool m_showCrosshairs;

  /// Whether crosshairs are rendered in lightbox views when crosshairs are globally enabled.
  bool m_showCrosshairsInLightboxViews;

  /// Anatomical label text color as non-premultiplied RGBA.
  glm::vec4 m_anatomicalLabelColor;

  /// Whether anatomical direction labels are rendered in 2D views.
  bool m_showAnatomicalLabels;

  /// Whether anatomical direction labels are rendered in lightbox views when labels are globally enabled.
  bool m_showAnatomicalLabelsInLightboxViews;

  /// Scale multiplier for anatomical direction label text.
  float m_anatomicalLabelScale = 1.0f;

  /// Anatomical label vocabulary used for direction labels.
  AnatomicalLabelType m_anatomicalLabelType = AnatomicalLabelType::Human;

  /// Whether scale bars are rendered in 2D views.
  bool m_showScaleBars;

  /// Whether scale bars are rendered in lightbox views when scale bars are globally enabled.
  bool m_showScaleBarsInLightboxViews;

  /// Scale bar color as non-premultiplied RGBA.
  glm::vec4 m_scaleBarColor;

  /// Scale bar placement within each view.
  ScaleBarPosition m_scaleBarPosition;

  /// Scale bar orientation.
  ScaleBarOrientation m_scaleBarOrientation;

  /// Scale bar tick style.
  ScaleBarTicks m_scaleBarTicks;

  /// Target fraction of the view width or height occupied by the scale bar before nice-length rounding.
  float m_scaleBarTargetFraction;

  /// Scale bar margin from the selected view edge in screen pixels.
  float m_scaleBarMarginPx;

  /// Whether lightbox offset labels are rendered.
  bool m_showLightboxOffsetLabels;

  /// Lightbox offset label color as non-premultiplied RGBA.
  glm::vec4 m_lightboxOffsetLabelColor;

  /// Whether front-facing image box faces are considered during 3D raycasting.
  bool m_renderFrontFaces;

  /// Whether back-facing image box faces are considered during 3D raycasting.
  bool m_renderBackFaces;

  /// Raycast sampling distance in image voxel units.
  float m_raycastSamplingFactor;

  /// Reserved adaptive raycast sampling flag; hidden in the UI while the controller is disabled.
  bool m_adaptiveRaycastSamplingEnabled;

  /// Target frame rate for adaptive raycast sampling.
  float m_adaptiveRaycastTargetFrameRate;

  /// Effective raycast sampling factor chosen by adaptive sampling.
  float m_adaptiveRaycastEffectiveSamplingFactor;

  /// Measured raycast update frame rate used by adaptive sampling diagnostics.
  float m_adaptiveRaycastMeasuredFrameRate;

  /// Whether raycast background edge brightening is enabled for rays crossing the image box.
  bool m_raycastBackgroundEdgeBrighteningEnabled;

  /// Whether the 3D crosshairs glyph is rendered in raycast views.
  bool m_showCrosshairsIn3D;

  /// Diameter of the 3D crosshairs glyph in image voxel-diagonal units.
  float m_crosshairs3DGlyphDiameterVoxelDiagonals;

  /// Whether the last-interacted 3D camera frustum is rendered as an overlay in 2D views.
  bool m_showThreeDCameraFrustumIn2DViews;

  /// Whether 3D rotate-about-eye drag direction is reversed.
  bool m_reverseThreeDRotateAboutEye;

  /// 3D camera frustum overlay color as non-premultiplied RGBA.
  glm::vec4 m_threeDCameraFrustumColor;

  /// UID of the 3D view whose camera frustum should be shown in 2D views.
  std::optional<uuids::uuid> m_lastInteractedThreeDViewUid;

  /// Segmentation masking behavior used by 3D raycast rendering.
  enum class SegMaskingForRaycasting
  {
    SegMasksIn,  //!< Render raycast samples inside the segmentation mask.
    SegMasksOut, //!< Render raycast samples outside the segmentation mask.
    Disabled     //!< Do not use the segmentation mask for raycast rendering.
  };

  /// Current segmentation masking behavior for raycasting.
  SegMaskingForRaycasting m_segMasking;

  /// Global segmentation outline style.
  SegmentationOutlineStyle m_segOutlineStyle = SegmentationOutlineStyle::Disabled;

  /// Opacity of segmentation interiors when outline rendering is enabled.
  float m_segInteriorOpacity = 0.2f;

  /// Threshold used when rendering linearly interpolated segmentation samples.
  float m_segInterpCutoff = 0.5f;

  /// Shared color-map and masking parameters used by comparison and local metric render modes.
  struct MetricParams
  {
    /// Index of the color map used to render metric values.
    size_t m_colorMapIndex = 0;

    /// Slope/intercept applied before color-map lookup; updated when the color map or inversion flag changes.
    glm::vec2 m_cmapSlopeIntercept{1.0f, 0.0f};

    /// Display slope/intercept applied to raw metric values.
    glm::vec2 m_slopeIntercept{1.0f, 0.0f};

    /// Whether the metric color map is inverted.
    bool m_invertCmap = false;

    /// Whether metric rendering samples the color map continuously.
    bool m_cmapContinuous = true;

    /// Number of color-map quantization levels used when continuous color-map sampling is disabled.
    int m_cmapQuantizationLevels = 8;

    /// Whether metric values should only be computed inside the current mask.
    bool m_doMasking = false;

    /// Reserved volumetric metric flag; currently not implemented.
    bool m_volumetric = false;
  };

  /// Display parameters for squared-difference comparison mode.
  MetricParams m_squaredDifferenceParams;

  /// Display parameters for local normalized cross-correlation mode.
  MetricParams m_localNccParams;

  /// Display parameters for local linear residual mode.
  MetricParams m_localLinearResidualParams;

  /// Display parameters for joint histogram mode.
  MetricParams m_jointHistogramParams;

  int m_localNccPatchRadius = 3;                   //!< Radius of the view-plane NCC patch in samples.
  float m_localNccSampleSpacing = 1.0f;            //!< Sample spacing in reference-image voxel steps.
  float m_localNccMinValidFraction = 0.75f;        //!< Required overlap fraction for paired patch samples.
  float m_localNccVarianceEpsilon = 1.0e-5f;       //!< Minimum local variance before a patch is invalid.
  bool m_localNccIgnoreNegativeCorrelation = true; //!< Treat negative NCC as maximum mismatch.
  LocalNccPresentation m_localNccPresentation = LocalNccPresentation::Dissimilarity; //!< Display transform.
  LocalNccInvalidStyle m_localNccInvalidStyle = LocalNccInvalidStyle::Transparent;   //!< Invalid patch display.

  int m_localLinearResidualPatchRadius = 3;             //!< Radius of the view-plane patch in samples.
  float m_localLinearResidualSampleSpacing = 1.0f;      //!< Sample spacing in reference-image voxel steps.
  float m_localLinearResidualMinValidFraction = 0.75f;  //!< Required overlap fraction for paired patch samples.
  float m_localLinearResidualVarianceEpsilon = 1.0e-5f; //!< Minimum reference variance before a patch is invalid.
  LocalNccInvalidStyle m_localLinearResidualInvalidStyle =
    LocalNccInvalidStyle::Transparent; //!< Invalid patch display.

  /// Edge detection magnitude threshold and smoothing factor.
  glm::vec2 m_edgeMagnitudeSmoothing;

  /// Number of squares along the longest displayed dimension for checkerboard comparison mode.
  int m_numCheckerboardSquares;

  /// Whether overlay comparison uses cyan/magenta/white instead of red/green/yellow.
  bool m_overlayMagentaCyan;

  /// Quadrant split direction flags used by quadrant comparison mode.
  glm::ivec2 m_quadrants;

  /// Whether difference mode uses squared difference instead of absolute difference.
  bool m_useSquare;

  /// Whether ASCII rendering is enabled for image views.
  bool m_asciiEnabled = false;

  /// ASCII cell size in screen pixels.
  glm::vec2 m_asciiCellSizePx{8.f, 16.f};

  /// Index of the active ASCII character set.
  int m_asciiCharsetIndex = 0;

  /// ASCII foreground color used when color-map foreground mode is disabled.
  glm::vec3 m_asciiFgColor{1.f, 1.f, 1.f};

  /// ASCII background color.
  glm::vec3 m_asciiBgColor{0.f, 0.f, 0.f};

  /// ASCII background opacity.
  float m_asciiBgAlpha = 1.f;

  /// Whether ASCII foreground color is sampled from the image color map.
  bool m_asciiUseColormap = false;

  /// Whether ASCII glyph selection uses 3x2 spatial region matching instead of mean intensity only.
  bool m_asciiSpatialMode = false;

  /// Exponent used to shape local-maximum weighting in ASCII spatial matching; 1.0 is identity.
  float m_asciiSpatialExponent = 1.0f;

  /// Whether the ASCII atlas must be rebuilt because the active character set changed.
  bool m_asciiAtlasNeedsRebuild = false;

  /// Flashlight radius in normalized view coordinates.
  float m_flashlightRadius;

  /// Whether flashlight mode overlays the moving image over the fixed image instead of replacing the fixed image.
  bool m_flashlightOverlays;

  /// Whether the manual frame-rate limiter is enabled.
  bool m_manualFramerateLimiter = false;

  /// Target frame time, in seconds, used by the manual frame-rate limiter.
  double m_targetFrameTimeSeconds = 1.0 / 60.0;

  /// Global landmark rendering parameters.
  struct LandmarkParams
  {
    float strokeWidth = 1.0f;  //!< Landmark stroke width in screen pixels.
    glm::vec3 textColor{0.0f}; //!< Landmark label text color.

    /// Whether landmarks render above all image planes instead of being depth-ordered with the image stack.
    bool renderOnTopOfAllImagePlanes = false;
  };

  /// Global annotation rendering parameters.
  struct AnnotationParams
  {
    glm::vec3 textColor; //!< Annotation label text color.

    /// Whether annotations render above all image planes instead of being depth-ordered with the image stack.
    bool renderOnTopOfAllImagePlanes = false;

    /// Whether polygon vertices are hidden for vector annotations.
    bool hidePolygonVertices = false;
  };

  /// Rendering parameters for slice-plane intersection overlays.
  struct SliceIntersectionParams
  {
    float strokeWidth = 1.0f; //!< Slice intersection stroke width in screen pixels.

    /// Whether inactive image/view-plane intersections are rendered in regular views.
    bool renderInactiveImageViewIntersections = true;

    /// Whether inactive image/view-plane intersections are rendered in lightbox views.
    bool renderInactiveImageViewIntersectionsInLightboxViews = false;
  };

  /// Global landmark rendering parameters.
  LandmarkParams m_globalLandmarkParams;

  /// Global annotation rendering parameters.
  AnnotationParams m_globalAnnotationParams;

  /// Global slice intersection overlay parameters.
  SliceIntersectionParams m_globalSliceIntersectionParams;

  /**
   * @brief Packed isosurface parameters uploaded to the raycast shader.
   *
   * The current raycast shader path expects fixed-size vectors for a small number of isosurfaces. This structure
   * mirrors that shader interface and can be replaced when isosurface rendering owns a more flexible GPU buffer.
   */
  struct IsosurfaceData
  {
    IsosurfaceData();

    int numIsos{0u};                         //!< Number of active isosurfaces.
    std::vector<float> values;               //!< Isovalues in native image intensity units.
    std::vector<float> opacities;            //!< Base isosurface opacities.
    std::vector<float> rimOpacityStrengths;  //!< Rim-lighting opacity modulation strengths.
    std::vector<float> rimEmissionStrengths; //!< Rim-lighting emission/glow strengths.
    std::vector<float> rimPowers;            //!< Rim-lighting falloff exponents.
    std::vector<glm::vec3> colors;           //!< Base isosurface colors.

    std::vector<glm::vec3> ambient;  //!< Ambient material colors.
    std::vector<glm::vec3> diffuse;  //!< Diffuse material colors.
    std::vector<glm::vec3> specular; //!< Specular material colors.
    std::vector<float> shininesses;  //!< Specular shininess values.
  };

  /// Packed isosurface parameters used by the current 3D raycast shader path.
  IsosurfaceData m_isosurfaceData;
};
