const float kDdpMaxDepth = 1.0;

// Repeated DDP draws use invariant vertex positions, so a previously stored boundary depth must compare exactly with
// the fragment that produced it. A fixed tolerance merges distinct surfaces as they approach one another and makes
// their compositing order change with the camera.
bool ddpDepthIsOutside(float fragmentDepth, vec2 depthBounds)
{
  float nearestDepth = -depthBounds.x;
  float farthestDepth = depthBounds.y;
  return fragmentDepth < nearestDepth || fragmentDepth > farthestDepth;
}

bool ddpDepthIsInterior(float fragmentDepth, vec2 depthBounds)
{
  float nearestDepth = -depthBounds.x;
  float farthestDepth = depthBounds.y;
  return fragmentDepth > nearestDepth && fragmentDepth < farthestDepth;
}

bool ddpDepthIsNearest(float fragmentDepth, vec2 depthBounds)
{
  return fragmentDepth == -depthBounds.x;
}

bool ddpDepthBoundsAreValid(vec2 depthBounds)
{
  return -depthBounds.x <= depthBounds.y;
}

// Coplanar image planes need a deterministic secondary order because GL_MAX cannot alpha-composite fragments at an
// identical depth. Moving by representable float values preserves physical depth ordering far better than subtracting
// a fixed window-depth bias. Higher order values are composited in front, matching the 2D image stack.
float ddpOrderedImagePlaneDepth(float fragmentDepth, uint order)
{
  float boundedDepth = clamp(fragmentDepth, 0.0, 1.0);
  uint depthBits = floatBitsToUint(boundedDepth);
  return uintBitsToFloat(order < depthBits ? depthBits - order : 0u);
}
