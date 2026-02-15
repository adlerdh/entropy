#pragma once

#include <uuid.h>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <optional>
#include <string>

/**
 * @brief Image pixel component types
 */
enum class ComponentType
{
  // These types are supported in Entropy. If an input image does not have
  // one of these types, then a cast is made.
  Int8,
  UInt8,
  Int16,
  UInt16,
  Int32,
  UInt32,
  Float32,

  // These types are NOT supported in Entropy, because they are not supported
  // as OpenGL texture formats:
  Float64,
  ULong,
  Long,
  ULongLong,
  LongLong,
  LongDouble,
  Undefined
};

bool isComponentFloatingPoint(const ComponentType&);
bool isComponentUnsignedInt(const ComponentType&);
bool isValidSegmentationComponentType(const ComponentType&);

std::string componentTypeString(const ComponentType&);

bool isIntegerType(const ComponentType&);
bool isSignedIntegerType(const ComponentType&);
bool isUnsignedIntegerType(const ComponentType&);
bool isFloatingType(const ComponentType&);

/**
 * @brief Image pixel types
 */
enum class PixelType
{
  Scalar,
  RGB,
  RGBA,
  Offset,
  Vector,
  Point,
  CovariantVector,
  SymmetricSecondRankTensor,
  DiffusionTensor3D,
  Complex,
  FixedArray,
  Array,
  Matrix,
  VariableLengthVector,
  VariableSizeMatrix,
  Undefined
};

/// Computable by sequential updates in a single linear scan
struct OnlineStats
{
  long double min = 0.0;
  long double max = 0.0;
  long double mean = 0.0;
  long double stdev = 0.0;
  long double variance = 0.0;
  long double sum = 0.0;
  std::size_t count = 0;
};

/**
 * @brief Statistics of a single image component
 */
struct ComponentStats
{
  OnlineStats onlineStats;

  /// Order statistics
  std::array<long double, 101> quantiles;
};

struct QuantileOfValue
{
  double lowerQuantile = 0.0;
  double upperQuantile = 0.0;
  std::size_t lowerIndex = 0; //!< Ranges [0, N-1]
  std::size_t upperIndex = 0; //!< Ranges [0, N]
  double lowerValue = 0.0;
  double upperValue = 0.0;
  bool foundValue = false;
};

/**
 * @brief Image interpolation (resampling) mode for rendering
 */
enum class InterpolationMode
{
  NearestNeighbor,
  Linear,

  /// Fast separable tricubic B-spline texture filtering (cubic B-spline convolution)
  /// using trilinear hardware (via 8 trilinear fetches): B-spline filter applied to raw
  /// texture values, which produces a smoothed (non-interpolating) approximation.
  /// Take the texture samples as the control values and reconstruct by weighting the
  /// 4×4×4 neighborhood with the cubic B-spline basis (separable in x/y/z).
  /// -B-spline filtering/convolution on the stored texels
  /// -separable tricubic in 3D
  /// -accelerated by factoring the weights into a small number of trilinear samples
  CubicBsplineConvolution,

  /// Cubic B-spline interpolation via B-spline coefficient (prefilter) decomposition:
  /// A spline defined by coefficients computed from the original samples, just like
  /// \c itk::BSplineInterpolateImageFunction (true spline interpolation model) that
  /// first converts the sampled image values into B-spline coefficients via
  /// \c itk::BSplineDecompositionImageFilter. It valuates a uniform cubic B-spline
  /// basis expansion where the coefficients are obtained by B-spline decomposition
  /// (an inverse filtering step) so that the reconstructed function interpolates the
  /// original samples at grid points.
  // CubicBsplineInterpolation
};

/**
 * @brief Array of all available interpolation modes
 */
inline std::array<InterpolationMode, 3> const AllInterpolationModes{
  InterpolationMode::NearestNeighbor,
  InterpolationMode::Linear,
  InterpolationMode::CubicBsplineConvolution};

/**
 * @brief The current mouse mode
 */
enum class MouseMode
{
  Pointer,         //!< Move the crosshairs
  WindowLevel,     //!< Adjust window and level of the active image
  Segment,         //!< Segment the active image
  Annotate,        //!< Annotate the active image
  CameraTranslate, //!< Translate the view camera in plane
  CameraRotate,    //!< Rotate the view camera in plane and out of plane
  CameraZoom,      //!< Zoom the view camera
  CrosshairsRotate,//!< Crosshairs rotation
  ImageTranslate,  //!< Translate the active image in 2D and 3D
  ImageRotate,     //!< Rotate the active image in 2D and 3D
  ImageScale       //!< Scale the active image in 2D
};

/**
 * @brief Array of all available mouse modes in the Toolbar
 */
inline std::array<MouseMode, 10> const AllMouseModes{
  MouseMode::Pointer,
  MouseMode::WindowLevel,
  MouseMode::CameraZoom,
  MouseMode::CameraTranslate,
  MouseMode::CameraRotate,
  MouseMode::CrosshairsRotate,
  MouseMode::Segment,
  MouseMode::Annotate,
  MouseMode::ImageTranslate,
  MouseMode::ImageRotate
};

/// Get the mouse mode as a string
std::string typeString(const MouseMode& mouseMode);

/// Get the interpolation mode as a string
std::string typeString(const InterpolationMode& mode);

/// Get the toolbar button corresponding to a mouse mode
const char* toolbarButtonIcon(const MouseMode& mouseMode);

/**
 * @brief How should view zooming behave?
 */
enum class ZoomBehavior
{
  ToCrosshairs,    //!< Zoom to/from the crosshairs position
  ToStartPosition, //!< Zoom to/from the mouse start position
  ToViewCenter     //!< Zoom to/from the view center position
};

/**
 * @brief Defines axis constraints for mouse/pointer rotation interactions
 */
enum class AxisConstraint
{
  X,
  Y,
  Z,
  None
};

/**
 * @brief Defines the origin of rotation for a view camera
 */
enum class RotationOrigin
{
  CameraEye,  //!< Camera's eye position
  Crosshairs, //!< Crosshairs origin
  ViewCenter  //!< Center of the view
};

/**
 * @brief Describes a type of image selection
 */
enum class ImageSelection
{
  /// The unique reference image that defines the World coordinate system.
  /// There is one reference image in the app at a given time.
  ReferenceImage,

  /// The unique image that is being actively transformed or modified.
  /// There is one active image in the app at a given time.
  ActiveImage,

  /// The unique reference and active images.
  ReferenceAndActiveImages,

  /// All visible images in a given view.
  /// Each view has its own set of visible images.
  VisibleImagesInView,

  /// The fixed image in a view that is currently rendering a metric.
  FixedImageInView,

  /// The moving image in a view that is currently rendering a metric.
  MovingImageInView,

  /// The fixed and moving images in a view thatis currently rendering a metric.
  FixedAndMovingImagesInView,

  /// All images loaded in the application.
  AllLoadedImages
};

/**
 * @brief Describes modes for offsetting the position of the view's image plane
 * (along the view camera's front axis) relative to the World-space crosshairs position.
 * Typically, this is used to offset the views in tiled layouts by a certain number of steps
 * (along the camera's front axis)
 */
enum class ViewOffsetMode
{
  /// Offset by a given number of view scrolls relative to the reference image
  RelativeToRefImageScrolls,

  /// Offset by a given number of view scrolls relative to an image
  RelativeToImageScrolls,

  /// Offset by an absolute distance (in physical units)
  Absolute,

  /// No offset
  None
};

/**
 * @brief Describes an offset setting for a view
 */
struct ViewOffsetSetting
{
  /// Offset mode
  ViewOffsetMode m_offsetMode = ViewOffsetMode::None;

  /// Absolute offset distance, which is used if m_offsetMode is \c OffsetMode::Absolute
  float m_absoluteOffset = 0.0f;

  /// Relative number of offset scrolls, which is used if m_offsetMode is
  /// \c OffsetMode::RelativeToRefImageScrolls or \c OffsetMode::RelativeToImageScrolls
  int m_relativeOffsetSteps = 0;

  /// If m_offsetMode is \c OffsetMode::RelativeToImageScrolls, then this holds the
  /// unique ID of the image relative which offsets are computed. If the image ID is
  /// not specified, then the offset is ignored (i.e. assumed to be zero).
  std::optional<uuids::uuid> m_offsetImage = std::nullopt;
};

/**
 * @brief Anatomical label type
 */
enum class AnatomicalLabelType
{
  Human,
  Rodent,
  Disabled
};

/**
 * @brief View orientation convention
 */
enum class ViewConvention
{
  Radiological, //!< Patient left on view right
  Neurological  //!< Patient left on view left (aka surgical)
};

/**
 * @brief Which image should crosshairs snap to?
 */
enum class CrosshairsSnapping
{
  Disabled,
  ReferenceImage,
  ActiveImage
};

/**
 * @brief What do views align to?
  /// 1) World XYZ/LPS (if m_lockAnatomicalCoordinateAxesWithReferenceImage is false)
  /// 2)
 */
enum class ViewAlignmentMode
{
  /// Align to either the Reference image XYZ/LPS axes
  /// (if m_lockAnatomicalCoordinateAxesWithReferenceImage is true)
  /// or to the World XYZ (LPS) axes (if m_lockAnatomicalCoordinateAxesWithReferenceImage is false)
  WorldOrReferenceImage,

  /// Align to crosshairs XYZ axes (which may be rotated)
  Crosshairs
};

/**
 * @brief Style of segmentation outline
 */
enum class SegmentationOutlineStyle
{
  ImageVoxel, //!< Outline the outer voxels of the segmentation regions
  ViewPixel,  //!< Outline the outer view pixels of the segmentation regions
  Disabled    //!< Disabled outlining
};

/**
 * @brief Information needed for positioning a single anatomical label and the crosshair
 * that corresponds to this label.
 */
struct AnatomicalLabelPosInfo
{
  AnatomicalLabelPosInfo() : labelIndex(0) {}
  AnatomicalLabelPosInfo(int l) : labelIndex(l) {}

  /// The anatomical label index (0: L, 1: P, 2: S)
  int labelIndex;

  /// Mouse crosshairs center position (in Miewport space)
  glm::vec2 miewportXhairCenterPos{0.0f, 0.0f};

  /// Normalized direction vector of the label (in View Clip space)
  glm::vec2 viewClipDir{0.0f, 0.0f};

  /// Position of the label and the opposite label of its pair (in Miewport space)
  std::array<glm::vec2, 2> miewportLabelPositions;

  /// Positions of the crosshair-view intersections (in Miewport space).
  /// Equal to std::nullopt if there is no intersection of the crosshair with the
  /// view AABB for this label.
  std::optional<std::array<glm::vec2, 2>> miewportXhairPositions = std::nullopt;
};

/**
 * @brief Frame bounds
 */
union FrameBounds
{
  FrameBounds(glm::vec4 v)
    : viewport(std::move(v)) {}

  glm::vec4 viewport;

  struct
  {
    float xoffset;
    float yoffset;
    float width;
    float height;
  } bounds;
};

