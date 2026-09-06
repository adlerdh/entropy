/**
 * @brief Encapsulate logic for whether to render the fragment based on the view render mode
 * @param[in] clipPos Clip space position
 * @param[in] checkerCoord Checkerboard square coords
 * @return True iff the fragment should be rendered
 */
bool doRender(vec2 clipPos, vec2 checkerCoord)
{
  // Indicator for which crosshairs quadrant the fragment is in:
  bvec2 quadrant = bvec2(clipPos.x <= u_clipCrosshairs.x, clipPos.y > u_clipCrosshairs.y);

  // Distance of the fragment from the crosshairs, accounting for aspect ratio:
  vec2 flashlightOffset = clipPos - u_clipCrosshairs;
  flashlightOffset.x *= max(abs(u_aspectRatio), 1.0e-6);
  float flashlightDist = length(flashlightOffset);

  // Flag indicating whether the fragment is rendered
  bool render = (IMAGE_RENDER_MODE == u_renderMode);

  // Check whether to render the fragment based on the mode (Checkerboard/Quadrants/Flashlight):
  render = render || ((CHECKER_RENDER_MODE == u_renderMode) &&
                      (u_showFix == bool(mod(floor(checkerCoord.x) + floor(checkerCoord.y), 2.0) > 0.5)));

  render = render || ((QUADRANTS_RENDER_MODE == u_renderMode) &&
                      (u_showFix == ((!u_quadrants.x || quadrant.x) == (!u_quadrants.y || quadrant.y))));

  render =
    render || ((FLASHLIGHT_RENDER_MODE == u_renderMode) &&
               ((u_showFix == (flashlightDist > u_flashlightRadius)) || (u_flashlightMovingOnFixed && u_showFix)));

  return render;
}
