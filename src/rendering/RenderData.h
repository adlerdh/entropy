#ifndef RENDER_DATA_H
#define RENDER_DATA_H

#include "common/Types.h"

#include "rendering/utility/containers/VertexAttributeInfo.h"
#include "rendering/utility/containers/VertexIndicesInfo.h"
#include "rendering/utility/gl/GLBufferObject.h"
#include "rendering/utility/gl/GLBufferTexture.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/gl/GLVertexArrayObject.h"

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <uuid.h>

#include <map>
#include <unordered_map>
#include <vector>

/**
 * @brief Objects that encapsulate OpenGL state
 */
struct RenderData
{
  /**
     * @brief Uniforms for a single image component
     */
  struct ImageUniforms
  {
    glm::vec2 cmapSlopeIntercept{1.0f, 0.0f};  // Slope and intercept for image colormap
    int cmapQuantLevels = 0;                   // Number of image colormap quantization levels
    glm::vec3 hsvModFactors{0.0f, 1.0f, 1.0f}; // HSV modification factors

    glm::mat4 imgTexture_T_world{1.0f}; // Mapping from World space to image Texture space
    glm::mat4 world_T_imgTexture{1.0f}; // Mapping from image Texture space to World space

    glm::mat4 segTexture_T_world{1.0f}; // Mapping from World to segmentation Texture space
    glm::mat4 segVoxel_T_world{1.0f};   // Mapping from World to segmentation Voxel space

    glm::vec3 voxelSpacing{1.0f}; // Image voxel spacing (mm)

    glm::vec3 subjectBoxMinCorner{0.0f}; // Min corner of image AABB in Subject space
    glm::vec3 subjectBoxMaxCorner{0.0f}; // Max corner of image AABB in Subject space

    // Columns of this matrix hold the small texture gradient steps, which equal
    // (1.0 / dimX, 0, 0), (0, 1.0 / dimY, 0), and (0, 0, 1.0 / dimZ)
    glm::mat3 textureGradientStep{1.0f};

    glm::vec2 slopeIntercept_normalized_T_texture{
      1.0f, 0.0f
    }; // Image intensity slope and intercept (with W-L)

    // Image intensity slope and intercept (with W-L) for color images
    std::vector<glm::vec2>
      slopeInterceptRgba_normalized_T_texture{{1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f}};

    float slope_native_T_texture{1.0f}; // Map texture to native intensity (NO W-L)
    glm::vec2 largestSlopeIntercept{
      1.0f, 0.0f
    }; // Image intensity slope and intercept (giving the largest window)

    // Image min and max:
    glm::vec2 minMax{0.0f, 1.0f};

    // Image intensity lower & upper thresholds:
    glm::vec2 thresholds{0.0f, 1.0f};

    std::vector<glm::vec2> thresholdsRgba{{0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}};

    std::vector<glm::vec2> minMaxRgba{{0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f}};

    float imgOpacity{0.0f}; // Image opacity

    // Image opacity for color images
    std::vector<float> imgOpacityRgba = {0.0f, 0.0f, 0.0f, 0.0f};

    float segOpacity{0.0f}; // Segmentation opacity

    bool showEdges = false;
    bool thresholdEdges = true;
    float edgeMagnitude = 0.0f;
    bool useFreiChen = false;
    //        bool windowedEdges = false;
    bool overlayEdges = false;
    bool colormapEdges = false;
    glm::vec4 edgeColor{0.0f}; // RGBA, premultiplied by alpha
  };

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

  RenderData();

  /**
     * @brief Set the energy of x-rays used for x-ray intensity projection mode
     * @param energyMeV X-ray energy in MeV
     */
  void setXrayEnergy(float energyMeV);

  Quad m_quad;
  Circle m_circle;

  /// For each image, a vector of image textures (one per component)
  std::unordered_map<uuids::uuid, std::vector<GLTexture> > m_imageTextures;

  /// For each image, a map of image component to distance map textures
  std::unordered_map<uuids::uuid, std::unordered_map<uint32_t, GLTexture> > m_distanceMapTextures;

  std::unordered_map<uuids::uuid, GLTexture> m_segTextures;

  std::unordered_map<uuids::uuid, GLBufferTexture> m_labelBufferTextures;
  std::unordered_map<uuids::uuid, GLTexture> m_colormapTextures;

  // Blank textures that are bound to image and segmentation units
  // in case no image or segmentation is loaded from disk:
  GLTexture m_blankImageBlackTransparentTexture;
  GLTexture m_blankImageWhiteOpaqueTexture;
  GLTexture m_blankSegTexture;

  // Blank texture in case no distance map is created:
  GLTexture m_blankDistMapTexture;

  // Map of image uniforms, keyed by image UID
  std::unordered_map<uuids::uuid, ImageUniforms> m_uniforms;

  /// Should crosshairs snap to voxels?
  CrosshairsSnapping m_snapCrosshairs;

  // Should the images only be shown inside of masked regions?
  bool m_maskedImages;

  // Should image segmentation opacity be modulated by the image opacity?
  bool m_modulateSegOpacityWithImageOpacity;

  // Flag that image opacities are adjusted in "mix" mode, which allows
  // blending between a pair of images
  bool m_opacityMixMode;

  // Intensity projection slab thickness (in mm)
  float m_intensityProjectionSlabThickness;

  // Flag to compute intensity projection over the maximum image extent
  bool m_doMaxExtentIntensityProjection;

  // Window and level used for adjusting contrast of the x-ray intensity projections
  float m_xrayIntensityWindow;
  float m_xrayIntensityLevel;

  // Map of photon mass attenuation coefficients of liquid water, normalized by water density,
  // (in units of [cm^2/g]) at varying photon energy levels [MeV]:
  /// @see https://physics.nist.gov/PhysRefData/XrayMassCoef/ComTab/water.html
  static const std::map<float, float> msk_waterMassAttenCoeffs;

  // Map of photon mass attenuation coefficients of dry air at sea level, normalized by air density,
  // (in units of [cm^2/g]) at varying photon energy levels [MeV]:
  /// @see https://physics.nist.gov/PhysRefData/XrayMassCoef/ComTab/air.html
  static const std::map<float, float> msk_airMassAttenCoeffs;

  // Current energy (in KeV) for photons used in x-ray intensity projection
  float m_xrayEnergyKeV;

  // Current water and air photon mass attenuation coefficients
  float m_waterMassAttenCoeff;
  float m_airMassAttenCoeff;

  // Background (clear) color of 2D views
  glm::vec3 m_2dBackgroundColor;

  // Background color of 3D views (non-premultiplied by alpha)
  glm::vec4 m_3dBackgroundColor;

  // Flag to make background transparent in 3D views if there is no ray hit on the volume bounding box
  bool m_3dTransparentIfNoHit;

  glm::vec4 m_crosshairsColor;      // Crosshairs color (non-premultiplied by alpha)
  glm::vec4 m_anatomicalLabelColor; // Anatomical label text color (non-premultiplied by alpha)

  AnatomicalLabelType m_anatomicalLabelType;

  bool m_renderFrontFaces; // Flag to render back faces in 3D raycasting
  bool m_renderBackFaces;  // Flag to render back faces in 3D raycasting

  float m_raycastSamplingFactor; // Sampling factor for raycasting

  enum class SegMaskingForRaycasting
  {
    SegMasksIn,
    SegMasksOut,
    Disabled
  };

  SegMaskingForRaycasting m_segMasking;

  // Segmentation outline style
  SegmentationOutlineStyle m_segOutlineStyle;

  SegmentationInterpolation m_segInterpolation;

  // Opacity of interior of segmentation, when outlining is applied
  float m_segInteriorOpacity;

  // Cutoff for segmentation with linear interpolation used
  float m_segInterpCutoff;

  /// @brief Metric parameters
  struct MetricParams
  {
    // Index of the colormap to apply to metric images
    size_t m_colorMapIndex = 0;

    // Slope and intercept to apply to metric values prior to indexing into the colormap.
    // This value gets updated when m_colorMapIndex or m_invertCmap changes.
    glm::vec2 m_cmapSlopeIntercept{1.0f, 0.0f};

    // Slope and intercept to apply to metric values
    glm::vec2 m_slopeIntercept{1.0f, 0.0f};

    // Is the color map inverted?
    bool m_invertCmap = false;

    // Is the color map continuous?
    bool m_cmapContinuous = true;

    // Number of color map quantization levels
    int m_cmapQuantizationLevels = 8;

    // Should the metric only be computed inside the masked region?
    bool m_doMasking = false;

    // Should the metric be computed in 3D (across the full volume) or
    // in 2D (across only the current slice)?
    // Not currently implemented.
    bool m_volumetric = false;
  };

  MetricParams m_squaredDifferenceParams;
  MetricParams m_crossCorrelationParams;
  MetricParams m_jointHistogramParams;

  // Edge detection magnitude and smoothing
  glm::vec2 m_edgeMagnitudeSmoothing;

  // Number of squares along the longest dimensions for the checkerboard shader
  int m_numCheckerboardSquares;

  // Magenta/cyan (true) overlay colors or red/green (false)?
  bool m_overlayMagentaCyan;

  // Should comparison be done in x,y directions?
  glm::ivec2 m_quadrants;

  // Should the difference metric use squared difference (true) or absolute difference (false)?
  bool m_useSquare;

  // Flashlight radius
  float m_flashlightRadius;

  // When true, the flashlight overlays the moving image on top of fixed image.
  // When false, the flashlight replaces the fixed image with the moving image.
  bool m_flashlightOverlays;

  struct LandmarkParams
  {
    float strokeWidth = 1.0f;
    glm::vec3 textColor{0.0f};

    /// Flag to either render landmarks on top of all image planes (true)
    /// or interspersed with each image plane (false)
    bool renderOnTopOfAllImagePlanes = false;
  };

  struct AnnotationParams
  {
    glm::vec3 textColor;

    /// Flag to either render annotations on top of all image planes (true)
    /// or interspersed with each image plane (false)
    bool renderOnTopOfAllImagePlanes = false;

    /// Flag to never render polygon vertices
    bool hidePolygonVertices = false;
  };

  struct SliceIntersectionParams
  {
    float strokeWidth = 1.0f;

    /// Render the intersections of inactive images with the view planes?
    bool renderInactiveImageViewIntersections = true;
  };

  LandmarkParams m_globalLandmarkParams;
  AnnotationParams m_globalAnnotationParams;
  SliceIntersectionParams m_globalSliceIntersectionParams;

  struct IsosurfaceData
  {
    IsosurfaceData();

    /// Maximum number of isosurfaces
    static constexpr size_t MAX_NUM_ISOSURFACES = 16;

    std::vector<float> values;
    std::vector<float> opacities;
    std::vector<float> edgeStrengths;
    std::vector<glm::vec3> colors;

    // Material lighting colors:
    std::vector<glm::vec3> ambientLights;
    std::vector<glm::vec3> diffuseLights;
    std::vector<glm::vec3> specularLights;
    std::vector<float> shininesses;

    float widthIn2d;
  };

  IsosurfaceData m_isosurfaceData;
};

#endif // RENDER_DATA_H
