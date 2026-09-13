#pragma once

#include <imgui/imgui.h>

namespace about_entropy_icon
{

constexpr int width = 128;
constexpr int height = 128;

ImTextureID textureId();

/// Release the lazily created OpenGL texture while the application context is still current.
void releaseTexture() noexcept;

} // namespace about_entropy_icon
