#pragma once

class Uniforms;

namespace rendering
{

/**
 * @brief Return whether an image shader registry exposes the complete intensity-projection interface.
 *
 * Shader programs may omit intensity projection entirely. A registry that exposes only part of the interface is a
 * programming error and causes this function to throw.
 */
bool supportsIntensityProjection(const Uniforms& uniforms);

} // namespace rendering
