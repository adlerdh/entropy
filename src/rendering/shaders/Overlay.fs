#version 330 core

#define MIN_IMAGE_TEXCOORD vec3(0.0)
#define MAX_IMAGE_TEXCOORD vec3(1.0)

in VS_OUT
{
  vec3 v_texCoord[2];
} fs_in;

layout (location = 0) out vec4 o_color; // Output RGBA color (premultiplied alpha)

uniform sampler3D u_imgTex[2]; // Texture units 0/1: images

uniform vec2 u_imgSlopeIntercept[2]; // Slopes and intercepts for image window-leveling
uniform vec2 u_imgMinMax[2]; // Min and max image values
uniform vec2 u_imgThresholds[2]; // Image lower and upper thresholds, mapped to OpenGL texture intensity
uniform float u_imgOpacity[2]; // Image opacities
uniform bool u_magentaCyan;

float hardThreshold(float value, vec2 thresholds)
{
  return float(thresholds[0] <= value && value <= thresholds[1]);
}

// float textureLookup(sampler3D texture, vec3 texCoords);
{{TEXTURE_LOOKUP_FUNCTION}}

void main()
{
  vec4 overlapColor = vec4(0.0, 0.0, 0.0, 0.0);

  for (int i = 0; i < 2; ++i)
  {
    float val;
    switch (i) {
    case 0: {
      val = clamp(textureLookup(u_imgTex[0], fs_in.v_texCoord[i]), u_imgMinMax[i][0], u_imgMinMax[i][1]);
      break;
    }
    case 1: {
      val = clamp(textureLookup(u_imgTex[1], fs_in.v_texCoord[i]), u_imgMinMax[i][0], u_imgMinMax[i][1]);
      break;
    }
    }

    // Apply W/L:
    float norm = clamp(u_imgSlopeIntercept[i][0] * val + u_imgSlopeIntercept[i][1], 0.0, 1.0);

    // Foreground mask, based on whether texture coordinates are in range [0.0, 1.0]^3:
    bool mask = !(any(lessThan(fs_in.v_texCoord[i], MIN_IMAGE_TEXCOORD)) ||
                  any(greaterThan(fs_in.v_texCoord[i], MAX_IMAGE_TEXCOORD)));

    // Apply opacity, mask, and thresholds:
    float alpha = u_imgOpacity[i] * float(mask) * hardThreshold(val, u_imgThresholds[i]);
    overlapColor[i] = alpha * norm;
  }

  // Apply magenta/cyan option:
  overlapColor.b = float(u_magentaCyan) * max(overlapColor.r, overlapColor.g);

  // Turn on overlap color if either the reference or moving image is present:
  // (note that this operation effectively thresholds out 0.0 as background)
  overlapColor.a = float((overlapColor.r > 0.0) || (overlapColor.g > 0.0));
  overlapColor.rgb = overlapColor.a * overlapColor.rgb;

  o_color = overlapColor;
}
