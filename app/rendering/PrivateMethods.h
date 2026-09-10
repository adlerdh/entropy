#pragma once

// This header is intentionally included inside Rendering's private section. It keeps private implementation method
// declarations out of Rendering.h's public API narrative while preserving ordinary C++ member declarations.

/// @name Renderer lifecycle and shader setup
/// @{

/**
 * @brief Compile and link all shader programs used by the renderer.
 */
void createShaderPrograms();

/**
 * @brief Compile and link one raycast isosurface shader program.
 *
 * @param program Program object to populate.
 * @param warped True to build the warped raycast shader variant.
 * @return True when the program compiled and linked successfully.
 */
static bool createRaycastIsoProgram(GLShaderProgram& program, bool warped);

/**
 * @brief Compile and link the basic mesh shader program.
 *
 * @param program Program object to populate.
 * @return True when the program compiled and linked successfully.
 */
static bool createMeshProgram(GLShaderProgram& program);

/** @brief Compile and link the mesh program that emits anti-aliased triangle topology coordinates. */
static bool createMeshEdgesProgram(GLShaderProgram& program);

/**
 * @brief Compile and link the mesh shadow-map depth shader program.
 *
 * @param program Program object to populate.
 * @return True when the program compiled and linked successfully.
 */
static bool createMeshShadowDepthProgram(GLShaderProgram& program);

/**
 * @brief Compile and link the mesh ambient occlusion geometry shader program.
 *
 * @param program Program object to populate.
 * @return True when the program compiled and linked successfully.
 */
static bool createMeshAmbientOcclusionGeometryProgram(GLShaderProgram& program);

/**
 * @brief Compile and link the mesh ambient occlusion resolve shader program.
 *
 * @param program Program object to populate.
 * @return True when the program compiled and linked successfully.
 */
static bool createMeshAmbientOcclusionResolveProgram(GLShaderProgram& program);

/** @brief Compile and link the edge-preserving mesh ambient occlusion filter. */
static bool createMeshAmbientOcclusionFilterProgram(GLShaderProgram& program);

/**
 * @brief Compile and link the mesh image-plane grayscale shader for ordinary 3D textures.
 *
 * @param program Program object to populate.
 * @return True when the program compiled and linked successfully.
 */
static bool createMeshImagePlaneGrayLinearProgram(GLShaderProgram& program);

/**
 * @brief Compile and link the mesh image-plane grayscale shader for planar 2D fallback textures.
 *
 * @param program Program object to populate.
 * @return True when the program compiled and linked successfully.
 */
static bool createMeshImagePlaneGrayLinearTexture2DProgram(GLShaderProgram& program);

/**
 * @brief Compile and link the mesh image-plane isocontour shader for ordinary 3D textures.
 *
 * @param program Program object to populate.
 * @return True when the program compiled and linked successfully.
 */
static bool createMeshImagePlaneIsoContourProgram(GLShaderProgram& program);

/**
 * @brief Compile and link the mesh image-plane isocontour shader for planar 2D fallback textures.
 *
 * @param program Program object to populate.
 * @return True when the program compiled and linked successfully.
 */
static bool createMeshImagePlaneIsoContourTexture2DProgram(GLShaderProgram& program);

/**
 * @brief Compile and link the mesh image-plane DDP initialization shader for ordinary 3D textures.
 *
 * @param program Program object to populate.
 * @return True when the program compiled and linked successfully.
 */
static bool createMeshImagePlaneDdpInitProgram(GLShaderProgram& program);

/**
 * @brief Compile and link the mesh image-plane DDP initialization shader for planar 2D fallback textures.
 *
 * @param program Program object to populate.
 * @return True when the program compiled and linked successfully.
 */
static bool createMeshImagePlaneDdpInitTexture2DProgram(GLShaderProgram& program);

/**
 * @brief Compile and link the mesh image-plane DDP peeling shader for ordinary 3D textures.
 *
 * @param program Program object to populate.
 * @return True when the program compiled and linked successfully.
 */
static bool createMeshImagePlaneDdpPeelProgram(GLShaderProgram& program);

/**
 * @brief Compile and link the mesh image-plane DDP peeling shader for planar 2D fallback textures.
 *
 * @param program Program object to populate.
 * @return True when the program compiled and linked successfully.
 */
static bool createMeshImagePlaneDdpPeelTexture2DProgram(GLShaderProgram& program);

/** Compile the shader that alpha-composes one orientation's 3D-texture image stack. */
static bool createMeshImagePlaneCompositeProgram(GLShaderProgram& program);

/** Compile the shader that alpha-composes one orientation's planar-texture image stack. */
static bool createMeshImagePlaneCompositeTexture2DProgram(GLShaderProgram& program);

/** Create the full-screen analytic image-plane border program. */
static bool createMeshImagePlaneBorderProgram(GLShaderProgram& program);

/** Compile the full-screen shader that contributes pre-composed image planes to DDP initialization. */
static bool createMeshImagePlaneCompositeDdpInitProgram(GLShaderProgram& program);

/** Compile the full-screen shader that contributes pre-composed image planes to DDP peeling. */
static bool createMeshImagePlaneCompositeDdpPeelProgram(GLShaderProgram& program);

/**
 * @brief Compile and link the mesh DDP initialization shader program.
 *
 * @param program Program object to populate.
 * @return True when the program compiled and linked successfully.
 */
static bool createMeshDdpInitProgram(GLShaderProgram& program);

/** @brief Compile and link the DDP initialization program matching the triangle-edges rasterization pipeline. */
static bool createMeshDdpInitEdgesProgram(GLShaderProgram& program);

/**
 * @brief Compile and link the mesh DDP peeling shader program.
 *
 * @param program Program object to populate.
 * @return True when the program compiled and linked successfully.
 */
static bool createMeshDdpPeelProgram(GLShaderProgram& program);

/** @brief Compile and link the DDP peel program that draws anti-aliased triangle topology edges. */
static bool createMeshDdpPeelEdgesProgram(GLShaderProgram& program);

/** @brief Compile and link the full-screen shader used to detect remaining DDP depth layers. */
static bool createMeshDdpCompletionProgram(GLShaderProgram& program);

/**
 * @brief Compile and link the mesh DDP back-color blend shader program.
 *
 * @param program Program object to populate.
 * @return True when the program compiled and linked successfully.
 */
static bool createMeshDdpBackBlendProgram(GLShaderProgram& program);

/**
 * @brief Compile and link the mesh DDP resolve shader program.
 *
 * @param program Program object to populate.
 * @return True when the program compiled and linked successfully.
 */
static bool createMeshDdpResolveProgram(GLShaderProgram& program);

/// @}
/// @name Top-level render passes
/// @{

/**
 * @brief Render image slices, segmentations, annotations, landmarks, raycasts, and view overlays for every view.
 */
void renderImageData();

/**
 * @brief Render image content and ordinary overlays for one view.
 *
 * @param view View to draw.
 * @param miewportViewBounds View bounds in mouse-oriented viewport coordinates.
 * @param worldOffsetXhairs Crosshairs position plus the view slice offset in world coordinates.
 * @param renderLandmarkAndAnnotationOverlays True to draw landmarks and annotations in this pass.
 * @param renderImageBorders True to draw image border/intersection overlays in this pass.
 * @param allowScreenPixelEdgePostProcessing True to allow the nested screen-space edge pass.
 */
void renderAllImagesForView(
  const View& view,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldOffsetXhairs,
  bool renderLandmarkAndAnnotationOverlays = true,
  bool renderImageBorders = true,
  bool allowScreenPixelEdgePostProcessing = true);

/**
 * @brief Render only image border/intersection overlays for one view.
 */
void renderAllImageBordersForView(
  const View& view,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldOffsetXhairs);

/**
 * @brief Render only landmark overlays for one view.
 */
void renderAllLandmarksForView(
  const View& view,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldOffsetXhairs);

/**
 * @brief Render only annotation overlays for one view.
 */
void renderAllAnnotationsForView(
  const View& view,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldOffsetXhairs);

/**
 * @brief Render vector-field arrows and warped-grid overlays for all eligible views.
 */
void renderVectorOverlays();

/**
 * @brief Render shader-based warped-grid overlays for vector-field images in one view.
 *
 * @param view View receiving the overlay.
 * @param worldOffsetXhairs Crosshairs position including the view slice offset.
 * @param displayModeUniform Shader comparison-mode value for the current view.
 * @param sourceImages Original image ids to test for vector-field grid overlays.
 */
void renderVectorWarpedGridOverlaysForView(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  int displayModeUniform,
  const CurrentImages& sourceImages);

/// @}
/// @name 2D image, segmentation, metric, and contour passes
/// @{

/**
 * @brief Render one set of image/segmentation pairs with the supplied image shader program.
 *
 * @param view View receiving the rendered image.
 * @param worldOffsetXhairs Crosshairs position including the view slice offset.
 * @param program Linked shader program for the selected image render mode.
 * @param imageSegPairs Image/segmentation ids to render.
 * @param showEdges True when edge overlays should be included in this image pass.
 * @param metricUsesWorldSampling Whether a warped metric program samples offsets in world space.
 */
void renderOneImage(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  GLShaderProgram& program,
  const CurrentImages& imageSegPairs,
  bool showEdges,
  bool metricUsesWorldSampling = false);

/**
 * @brief Render NanoVG overlays associated with one rendered image pass.
 */
void renderOneImage_overlays(
  const View& view,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldOffsetXhairs,
  const CurrentImages& imageSegPairs,
  bool renderLandmarkAndAnnotationOverlays,
  bool renderImageBorders);

/**
 * @brief Render the segmentation paint-brush preview for one image/segmentation pair.
 */
void renderBrushPreview(const View& view, const glm::vec3& worldOffsetXhairs, const ImgSegPair& imgSegPair);

/**
 * @brief Render the active comparison or metric mode for one view.
 */
void renderMetricImagesForView(const View& view, const glm::vec3& worldOffsetXhairs);

/**
 * @brief Render image isosurfaces for one 3D view, using meshes and a transient raycast when needed.
 * @return True when any isosurface scene content was rendered.
 */
bool renderVolumeImagesForView(const View& view, bool interactiveOverlay = false);

/** Append visible user-imported meshes for the images rendered by a 3D view. */
void appendImportedMeshesForView(
  const View& view,
  const CurrentImages& imageSegPairs,
  std::vector<rendering::mesh::MeshRenderable>& renderables);

/**
 * @brief Apply completed background mesh extraction jobs to the CPU mesh cache.
 */
void consumeCompletedMeshExtractions();

/** Release extracted meshes whose geometry keys are no longer represented by current application state. */
void reconcileExtractedMeshResources();

/**
 * @brief Publish the current background mesh extraction count to the UI status popup.
 */
void updateMeshExtractionStatus();

/**
 * @brief Clear one 3D mesh view viewport with the configured 3D background color.
 *
 * @param view View whose device viewport should be cleared.
 */
void clearMeshViewBackgroundForView(const View& view);

/**
 * @brief Render one mesh render list into a 3D view using the renderer's configured compositing paths.
 *
 * @param view View receiving the mesh draw.
 * @param list Opaque, transparent, additive, and multiplicative mesh buckets to render.
 */
void drawMeshRenderListForView(
  const View& view,
  const rendering::mesh::MeshRenderList& list,
  const rendering::mesh::MeshImagePlaneRenderList* imagePlaneList = nullptr);

/**
 * @brief Render textured image-plane mesh renderables into a 3D view.
 *
 * @param view View receiving the image planes.
 * @param list Filtered image-plane renderables to draw.
 */
void drawMeshImagePlaneRenderListForView(const View& view, const rendering::mesh::MeshImagePlaneRenderList& list);

/**
 * @brief Draw image-plane depth bounds into the active DDP initialization framebuffer.
 *
 * @param view View whose camera uniforms are used.
 * @param list Image-plane renderables participating in DDP.
 * @param context Mesh draw context shared by the DDP pass.
 */
void drawMeshImagePlaneDdpDepthBoundsForView(
  const View& view,
  const rendering::mesh::MeshImagePlaneRenderList& list,
  const rendering::mesh::MeshDrawContext& context);

/**
 * @brief Peel image-plane layers into the active DDP ping-pong framebuffer.
 *
 * @param view View whose camera uniforms are used.
 * @param list Image-plane renderables participating in DDP.
 * @param context Mesh draw context shared by the DDP pass.
 * @param previousDepthBounds Previous DDP depth-bounds texture.
 * @param previousFrontColor Previous DDP accumulated front-color texture.
 */
void drawMeshImagePlaneDdpPeelLayersForView(
  const View& view,
  const rendering::mesh::MeshImagePlaneRenderList& list,
  const rendering::mesh::MeshDrawContext& context,
  GLTexture& previousDepthBounds,
  GLTexture& previousFrontColor);

/** Alpha-compose each orientation's coincident image stack before it participates in DDP. */
void prepareMeshImagePlaneDdpCompositesForView(
  const View& view,
  const rendering::mesh::MeshImagePlaneRenderList& list,
  const rendering::mesh::MeshDrawContext& context);

/**
 * @brief Build enabled 3D image-plane renderables without drawing them.
 *
 * @param view 3D view whose visible images and crosshairs position define the planes.
 * @param[out] borderRenderables Ordinary mesh renderables for image-plane borders and image boxes.
 * @return Textured image-plane renderables for DDP composition.
 */
std::vector<rendering::mesh::MeshImagePlaneRenderable> collectMeshImagePlaneRenderablesForView(
  const View& view,
  std::vector<rendering::mesh::MeshRenderable>& borderRenderables);

/**
 * @brief Render enabled image planes and the crosshairs glyph as one correctly composited 3D scene.
 *
 * @param view 3D view receiving the scene.
 * @return True when at least one plane, border, or crosshairs glyph was rendered.
 */
bool renderMeshImagePlanesAndCrosshairsForView(const View& view);

/**
 * @brief Build the viewer-facing octant cutaway for one 3D view.
 *
 * @return Enabled cutaway based on the view camera and crosshairs, or a disabled cutaway when the global setting is
 * off.
 */
rendering::mesh::MeshOctantCutaway meshCutawayForView(const View& view) const;

/**
 * @brief Render committed isosurfaces through the mesh path when they no longer require raycasting.
 *
 * @param view 3D view receiving the mesh draw.
 * @param imageSegPairs Image/segmentation pairs selected for 3D rendering.
 * @return True when at least one isosurface mesh was rendered and raycasting can be skipped.
 */
bool renderIsosurfaceMeshesForView(
  const View& view,
  const CurrentImages& imageSegPairs,
  std::vector<rendering::mesh::MeshRenderable>* accumulatedRenderables = nullptr);

/**
 * @brief Render the active segmentation of the selected 3D image as label meshes.
 */
bool renderSegmentationMeshesForView(
  const View& view,
  std::vector<rendering::mesh::MeshRenderable>* accumulatedRenderables = nullptr);

/** @brief Render segmentation meshes and isosurfaces in one depth-peeled mesh scene. */
bool renderCombinedSurfaceMeshesForView(const View& view);

/**
 * @brief Return a revision-aware inventory of label values present in a segmentation time point.
 *
 * @return Cached label set, or null when the segmentation pixels cannot be read.
 */
const rendering::mesh::SegmentationLabelInventory*
presentSegmentationLabels(const uuids::uuid& segmentationUid, const Image& segmentation, uint32_t timePoint);

/**
 * @brief Append the 3D crosshairs glyph to a mesh scene when it is visible for the view.
 *
 * @param view 3D view receiving the mesh draw.
 * @param renderables Mesh renderable list to extend.
 * @return True when a glyph renderable was appended.
 */
bool appendMeshCrosshairsRenderableForView(const View& view, std::vector<rendering::mesh::MeshRenderable>& renderables);

/**
 * @brief Render the 3D crosshairs glyph through the mesh path.
 */
void renderMeshCrosshairsForView(const View& view);

/**
 * @brief Render landmark groups assigned to the selected 3D image through the mesh path.
 */
void renderMeshLandmarksForView(const View& view);

/**
 * @brief Render the internal opt-in synthetic mesh scene for one 3D view.
 */
void renderSyntheticMeshSceneForView(const View& view);

/**
 * @brief Return the image/segmentation pair currently eligible for 3D raycast rendering in a view.
 */
std::optional<ImgSegPair> raycastImageForView(const View& view);

/**
 * @brief Return all image/segmentation pairs currently eligible for 3D rendering in a view.
 */
CurrentImages raycastImagesForView(const View& view);

/** Return the first texture-backed image available to mesh-rendered 3D scene features. */
std::optional<ImgSegPair> meshSceneImageForView(const View& view);

/**
 * Return visible images with uploaded 2D or 3D textures for mesh-rendered 3D scene features.
 *
 * Unlike raycastImagesForView(), planar GL_TEXTURE_2D fallback images are intentionally included.
 */
CurrentImages meshSceneImagesForView(const View& view);

/**
 * @brief Return the selected isosurface currently being interactively edited, if any.
 *
 * @param imageSegPairs Image/segmentation pairs selected for 3D rendering.
 * @return Edited isosurface and its owning image pair, or nullopt when no isovalue edit is active.
 */
std::optional<ActiveIsosurfaceEdit> activeIsosurfaceEdit(const CurrentImages& imageSegPairs) const;

void renderSegmentationForImage(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  const ImgSegPair& imgSegPair,
  const uuids::uuid& imageUid,
  const rendering::RenderDerivedData::ImageUniforms& uniforms,
  bool renderWarped,
  const std::optional<uuids::uuid>& deformationUid,
  int displayModeUniform,
  bool isFixedImage);

void renderColorImageForImage(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  const ImgSegPair& imgSegPair,
  const Image& image,
  const uuids::uuid& imageUid,
  const rendering::RenderDerivedData::ImageUniforms& uniforms,
  const rendering::PlanarTextureLayout& imageTextureLayout,
  bool renderWarped,
  const std::optional<uuids::uuid>& deformationUid,
  int displayModeUniform,
  bool isFixedImage,
  bool allowScreenPixelEdgePostProcessing);

void renderGrayImageForImage(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  const ImgSegPair& imgSegPair,
  const Image& image,
  const uuids::uuid& imageUid,
  const rendering::RenderDerivedData::ImageUniforms& uniforms,
  const rendering::PlanarTextureLayout& imageTextureLayout,
  bool renderWarped,
  const std::optional<uuids::uuid>& deformationUid,
  int displayModeUniform,
  bool isFixedImage,
  bool allowScreenPixelEdgePostProcessing);

void renderIsoContoursForImage(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  const ImgSegPair& imgSegPair,
  const Image& image,
  const uuids::uuid& imageUid,
  const rendering::RenderDerivedData::ImageUniforms& uniforms,
  const rendering::PlanarTextureLayout& imageTextureLayout,
  bool renderWarped,
  const std::optional<uuids::uuid>& deformationUid,
  int displayModeUniform,
  bool isFixedImage);

void renderVectorImageForImage(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  const ImgSegPair& imgSegPair,
  const Image& image,
  const rendering::RenderDerivedData::ImageUniforms& uniforms,
  const rendering::PlanarTextureLayout& imageTextureLayout,
  int displayModeUniform,
  bool isFixedImage);

/// @}
/// @name 3D raycast rendering
/// @{

/**
 * @brief Raycast one volume-rendered image into a 3D view.
 */
void volumeRenderOneImage(
  const View& view,
  GLShaderProgram& program,
  const glm::mat4& texture_T_world,
  const CurrentImages& imageSegPairs);

/**
 * @brief Upload raycast shader uniforms for one image, including optional deformation sampling state.
 */
void setRaycastIsoUniforms(
  GLShaderProgram& program,
  const ImgSegPair& imgSegPair,
  const Image& image,
  const rendering::RenderDerivedData::ImageUniforms& uniforms,
  bool renderWarped,
  const std::optional<uuids::uuid>& deformationUid);

/// @}
/// @name Texture binding and deformation uniforms
/// @{

template<typename Texture>
struct BoundTexture
{
  BoundTexture(Texture& textureArg, uint32_t unitArg) : texture(textureArg), unit(unitArg) {}

  std::reference_wrapper<Texture> texture;
  uint32_t unit;
};

using BoundTextures = std::list<BoundTexture<GLTexture>>;
using BoundBufferTextures = std::list<BoundTexture<GLBufferTexture>>;

/**
 * @brief Bind scalar image textures and associated color-map textures for one image/segmentation pair.
 */
BoundTextures bindScalarImageTextures(const ImgSegPair& p);

/**
 * @brief Bind multi-component color image textures for one image/segmentation pair.
 */
BoundTextures bindColorImageTextures(const ImgSegPair& p);

/**
 * @brief Bind the segmentation texture for one image/segmentation pair.
 */
BoundTextures bindSegTextures(const ImgSegPair& p);

/**
 * @brief Bind a deformation field with the default deformation shader sampler slots.
 */
BoundTextures bindDeformationTextures(const uuids::uuid& defUid);

/**
 * @brief Bind a deformation field with explicit sampler slots.
 */
BoundTextures bindDeformationTextures(const uuids::uuid& defUid, const Uniforms::SamplerIndexVectorType& samplers);

/**
 * @brief Unbind every texture returned by one of the texture binding helpers.
 */
static void unbindTextures(const BoundTextures& textures);

/**
 * @brief Ensure that a deformation field has a GPU texture available for rendering.
 */
bool ensureDeformationTexture(const uuids::uuid& defUid);

/**
 * @brief Return the active inverse deformation field for an image when it is renderable.
 */
std::optional<uuids::uuid> activeRenderableDeformationUid(const uuids::uuid& imageUid);

/**
 *  Return the reference-space image used to draw a warped image, if any.
 */
std::optional<uuids::uuid> activeRenderableDeformationReferenceImageUid(const uuids::uuid& imageUid);

/**
 * @brief Set deformation uniforms used by image and segmentation shader paths.
 */
void setDeformationUniforms(
  GLShaderProgram& program,
  const uuids::uuid& imageUid,
  const uuids::uuid& defUid,
  const glm::mat4& sampleTex_T_world) const;

/**
 * @brief Upload either a direct world-to-texture transform or the mutually exclusive deformation-sampling inputs.
 */
void setImageSamplingTransformUniforms(
  GLShaderProgram& program,
  const uuids::uuid& imageUid,
  const std::optional<uuids::uuid>& deformationUid,
  const glm::mat4& sampleTex_T_world) const;

/**
 * @brief Set deformation uniforms for one input slot of a metric/comparison shader.
 */
void setMetricDeformationUniforms(
  GLShaderProgram& program,
  std::size_t slot,
  const uuids::uuid& imageUid,
  const uuids::uuid& defUid,
  const glm::mat4& sampleTex_T_world) const;

/**
 * @brief Bind textures needed by metric and comparison shaders.
 */
BoundTextures bindMetricImageTextures(const CurrentImages& imageSegPairs, const ViewRenderMode& metricType);

/**
 * @brief Bind buffer textures such as segmentation label color tables.
 */
BoundBufferTextures bindSegBufferTextures(const ImgSegPair& p);

/**
 * @brief Unbind every buffer texture returned by a buffer texture binding helper.
 */
static void unbindBufferTextures(const BoundBufferTextures& textures);

/// @}
/// @name Image selection helpers
/// @{

/**
 * @brief Get current image and segmentation ids to render in metric shaders.
 */
CurrentImages getImageAndSegUidsForMetricShaders(const std::list<uuids::uuid>& metricImageUids) const;

/**
 * @brief Get current image and segmentation ids to render in image shaders.
 */
CurrentImages getImageAndSegUidsForImageShaders(const std::list<uuids::uuid>& imageUids) const;

/// @}
