#pragma once

/**
 * @brief Validate and log the current OpenGL context, then enable driver diagnostics when available.
 *
 * GLAD must be initialized and an OpenGL context must be current before this function is called. Entropy requires a
 * desktop OpenGL 3.3 core context or newer.
 */
void validateOpenGLContext();
