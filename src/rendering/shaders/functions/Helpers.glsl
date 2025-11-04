// 3D texture coordinates (s,t,p) are in [0.0, 1.0]^3
#define MIN_IMAGE_TEXCOORD vec3(0.0)
#define MAX_IMAGE_TEXCOORD vec3(1.0)

/**
 * @brief Check if coordinates are inside the image texture
 */
bool isInsideTexture(vec3 texCoord)
{
  return (all(greaterThanEqual(texCoord, MIN_IMAGE_TEXCOORD)) &&
          all(lessThanEqual(texCoord, MAX_IMAGE_TEXCOORD)));
}

/**
 * @brief Hard lower and upper thresholding
 */
float hardThreshold(float value, vec2 thresholds)
{
  return float(thresholds[0] <= value && value <= thresholds[1]);
}

/*
float smoothThreshold(float value, vec2 thresholds)
{
  return smoothstep(thresholds[0] - 0.01, thresholds[0], value) -
         smoothstep(thresholds[1], thresholds[1] + 0.01, value);
}
*/
