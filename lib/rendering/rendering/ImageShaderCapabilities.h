#pragma once

class Uniforms;

namespace rendering
{

enum class ImageSamplingTransform
{
  Direct,
  Deformation
};

/**
 * @brief Return whether an image shader registry exposes the complete intensity-projection interface.
 *
 * Shader programs may omit intensity projection entirely. A registry that exposes only part of the interface is a
 * programming error and causes this function to throw.
 */
bool supportsIntensityProjection(const Uniforms& uniforms);

/**
 * @brief Validate that an image shader exposes exactly the requested sampling-transform interface.
 *
 * Direct shaders transform world positions with `u_tex_T_world`. Deformation shaders reconstruct texture coordinates
 * in the fragment shader from the complete deformation-uniform group. Mixing these interfaces, or exposing only part
 * of the deformation group, is a shader setup error.
 */
void validateImageSamplingTransform(const Uniforms& uniforms, ImageSamplingTransform expected);

/**
 * @brief Validate the corresponding two-image sampling interface used by comparison and metric shaders.
 */
void validateMetricSamplingTransform(const Uniforms& uniforms, ImageSamplingTransform expected);

} // namespace rendering
