#pragma once

// This header is intentionally included inside Rendering's private section. It keeps private implementation method
// declarations out of Rendering.h's public API narrative while preserving ordinary C++ member declarations.

/// @name Renderer lifecycle and shader setup
/// @{

/**
 * @brief Restore the OpenGL state expected by Entropy after third-party drawing calls.
 */
void setupOpenGLState();

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
bool createRaycastIsoProgram(GLShaderProgram& program, bool warped);

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
 * @param allowImagePostProcessing True to allow ASCII and pixel-edge post-processing.
 */
void renderAllImagesForView(
  const View& view,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldOffsetXhairs,
  bool renderLandmarkAndAnnotationOverlays = true,
  bool renderImageBorders = true,
  bool allowImagePostProcessing = true);

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
 */
void renderOneImage(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  GLShaderProgram& program,
  const CurrentImages& imageSegPairs,
  bool showEdges);

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
 * @brief Render the volume/raycast mode for one 3D view.
 */
void renderVolumeImagesForView(const View& view);

/**
 * @brief Return the image/segmentation pair currently eligible for 3D raycast rendering in a view.
 */
std::optional<ImgSegPair> raycastImageForView(const View& view);

void renderSegmentationForImage(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  const ImgSegPair& imgSegPair,
  const uuids::uuid& imageUid,
  const RenderData::ImageUniforms& uniforms,
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
  const RenderData::ImageUniforms& uniforms,
  const RenderData::PlanarTextureLayout& imageTextureLayout,
  bool renderWarped,
  const std::optional<uuids::uuid>& deformationUid,
  int displayModeUniform,
  bool isFixedImage);

void renderGrayImageForImage(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  const ImgSegPair& imgSegPair,
  const Image& image,
  const uuids::uuid& imageUid,
  const RenderData::ImageUniforms& uniforms,
  const RenderData::PlanarTextureLayout& imageTextureLayout,
  bool renderWarped,
  const std::optional<uuids::uuid>& deformationUid,
  int displayModeUniform,
  bool isFixedImage,
  bool allowImagePostProcessing);

void renderIsoContoursForImage(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  const ImgSegPair& imgSegPair,
  const Image& image,
  const uuids::uuid& imageUid,
  const RenderData::ImageUniforms& uniforms,
  const RenderData::PlanarTextureLayout& imageTextureLayout,
  bool renderWarped,
  const std::optional<uuids::uuid>& deformationUid,
  int displayModeUniform,
  bool isFixedImage);

void renderVectorImageForImage(
  const View& view,
  const glm::vec3& worldOffsetXhairs,
  const ImgSegPair& imgSegPair,
  const Image& image,
  const RenderData::ImageUniforms& uniforms,
  const RenderData::PlanarTextureLayout& imageTextureLayout,
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
  const View& view,
  const ImgSegPair& imgSegPair,
  const Image& image,
  const RenderData::ImageUniforms& uniforms,
  bool renderWarped,
  const std::optional<uuids::uuid>& deformationUid);

/// @}
/// @name Texture binding and deformation uniforms
/// @{

/**
 * @brief Bind scalar image textures and associated color-map textures for one image/segmentation pair.
 */
std::list<std::reference_wrapper<GLTexture>> bindScalarImageTextures(const ImgSegPair& p);

/**
 * @brief Bind multi-component color image textures for one image/segmentation pair.
 */
std::list<std::reference_wrapper<GLTexture>> bindColorImageTextures(const ImgSegPair& p);

/**
 * @brief Bind the segmentation texture for one image/segmentation pair.
 */
std::list<std::reference_wrapper<GLTexture>> bindSegTextures(const ImgSegPair& p);

/**
 * @brief Bind a deformation field with the default deformation shader sampler slots.
 */
std::list<std::reference_wrapper<GLTexture>> bindDeformationTextures(const uuids::uuid& defUid);

/**
 * @brief Bind a deformation field with explicit sampler slots.
 */
std::list<std::reference_wrapper<GLTexture>> bindDeformationTextures(
  const uuids::uuid& defUid,
  const Uniforms::SamplerIndexVectorType& samplers);

/**
 * @brief Unbind every texture returned by one of the texture binding helpers.
 */
void unbindTextures(const std::list<std::reference_wrapper<GLTexture>>& textures);

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
std::list<std::reference_wrapper<GLTexture>> bindMetricImageTextures(
  const CurrentImages& imageSegPairs,
  const ViewRenderMode& metricType);

/**
 * @brief Bind buffer textures such as segmentation label color tables.
 */
std::list<std::reference_wrapper<GLBufferTexture>> bindSegBufferTextures(const ImgSegPair& p);

/**
 * @brief Unbind every buffer texture returned by a buffer texture binding helper.
 */
void unbindBufferTextures(const std::list<std::reference_wrapper<GLBufferTexture>>& textures);

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
