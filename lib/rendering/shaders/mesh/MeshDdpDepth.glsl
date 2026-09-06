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

// Coincident images are alpha-composited into one layer before DDP. The only remaining tie is the exact intersection
// of two orthogonal composite planes, so adjacent representable depths provide a deterministic result without
// displacing their real front/back relationship around the intersection.
float ddpOrderedImagePlaneDepth(float fragmentDepth, uint depthOrder)
{
  float boundedDepth = clamp(fragmentDepth, 0.0, 1.0);
  uint depthBits = floatBitsToUint(boundedDepth);
  return uintBitsToFloat(depthOrder < depthBits ? depthBits - depthOrder : 0u);
}
