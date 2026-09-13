#version 330 core

uniform sampler2D u_frontColorTex;
uniform sampler2D u_backColorTex;
uniform ivec2 u_viewportOrigin;

layout(location = 0) out vec4 outColor;

const float k_lumaThreshold = 1.0 / 12.0;
const float k_relativeLumaThreshold = 1.0 / 6.0;
const float k_directionReduceScale = 1.0 / 8.0;
const float k_directionReduceMinimum = 1.0 / 128.0;
const float k_maxSearchDistancePixels = 8.0;

vec4 compositeAt(ivec2 pixelCoord, ivec2 textureSizePx)
{
  ivec2 clampedCoord = clamp(pixelCoord, ivec2(0), textureSizePx - ivec2(1));
  vec4 frontColor = texelFetch(u_frontColorTex, clampedCoord, 0);
  vec4 backColor = texelFetch(u_backColorTex, clampedCoord, 0);
  return frontColor + (1.0 - frontColor.a) * backColor;
}

vec4 bilinearCompositeAt(vec2 pixelPosition, ivec2 textureSizePx)
{
  vec2 halfTexel = 0.5 / vec2(textureSizePx);
  vec2 textureCoord = clamp(pixelPosition / vec2(textureSizePx), halfTexel, vec2(1.0) - halfTexel);
  vec4 frontColor = texture(u_frontColorTex, textureCoord);
  vec4 backColor = texture(u_backColorTex, textureCoord);
  return frontColor + (1.0 - frontColor.a) * backColor;
}

float luminance(vec4 premultipliedColor)
{
  vec3 color = premultipliedColor.a > 1.0e-6 ? premultipliedColor.rgb / premultipliedColor.a : vec3(0.0);
  return dot(color, vec3(0.299, 0.587, 0.114));
}

vec4 antialiasedComposite(ivec2 pixelCoord, ivec2 textureSizePx)
{
  vec4 centerColor = compositeAt(pixelCoord, textureSizePx);
  float centerLuma = luminance(centerColor);
  float northWestLuma = luminance(compositeAt(pixelCoord + ivec2(-1, 1), textureSizePx));
  float northEastLuma = luminance(compositeAt(pixelCoord + ivec2(1, 1), textureSizePx));
  float southWestLuma = luminance(compositeAt(pixelCoord + ivec2(-1, -1), textureSizePx));
  float southEastLuma = luminance(compositeAt(pixelCoord + ivec2(1, -1), textureSizePx));

  float minimumLuma = min(centerLuma, min(min(northWestLuma, northEastLuma), min(southWestLuma, southEastLuma)));
  float maximumLuma = max(centerLuma, max(max(northWestLuma, northEastLuma), max(southWestLuma, southEastLuma)));
  float lumaRange = maximumLuma - minimumLuma;
  if (lumaRange < max(k_lumaThreshold, maximumLuma * k_relativeLumaThreshold)) {
    return centerColor;
  }

  vec2 direction;
  direction.x = -((northWestLuma + northEastLuma) - (southWestLuma + southEastLuma));
  direction.y = (northWestLuma + southWestLuma) - (northEastLuma + southEastLuma);
  float directionReduce = max(
    (northWestLuma + northEastLuma + southWestLuma + southEastLuma) * 0.25 * k_directionReduceScale,
    k_directionReduceMinimum);
  float inverseMinimumDirection = 1.0 / (min(abs(direction.x), abs(direction.y)) + directionReduce);
  direction =
    clamp(direction * inverseMinimumDirection, vec2(-k_maxSearchDistancePixels), vec2(k_maxSearchDistancePixels));

  vec2 centerPosition = vec2(pixelCoord) + vec2(0.5);
  vec4 colorA = 0.5 * (bilinearCompositeAt(centerPosition + direction * (1.0 / 3.0 - 0.5), textureSizePx) +
                       bilinearCompositeAt(centerPosition + direction * (2.0 / 3.0 - 0.5), textureSizePx));
  vec4 colorB = colorA * 0.5 + 0.25 * (bilinearCompositeAt(centerPosition - direction * 0.5, textureSizePx) +
                                       bilinearCompositeAt(centerPosition + direction * 0.5, textureSizePx));
  float colorBLuma = luminance(colorB);
  return colorBLuma < minimumLuma || colorBLuma > maximumLuma ? colorA : colorB;
}

void main()
{
  ivec2 pixelCoord = ivec2(gl_FragCoord.xy) - u_viewportOrigin;
  ivec2 textureSizePx = textureSize(u_frontColorTex, 0);
  if (pixelCoord.x < 0 || pixelCoord.y < 0 || pixelCoord.x >= textureSizePx.x || pixelCoord.y >= textureSizePx.y) {
    discard;
  }

  // The window's multisampling cannot affect the single-sample DDP attachments. Apply a conservative FXAA resolve to
  // the already composited, premultiplied transparency result so thin mesh silhouettes remain stable at every angle.
  outColor = antialiasedComposite(pixelCoord, textureSizePx);
}
