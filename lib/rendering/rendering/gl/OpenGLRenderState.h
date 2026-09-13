#pragma once

namespace rendering
{

/** Restore the baseline state expected by rendering passes after third-party OpenGL drawing. */
void restoreOpenGLRenderState();

/** Remove context bindings before context-owned resources are destroyed. */
void clearOpenGLBindingsForShutdown();

} // namespace rendering
