#version 330 core

in VS_OUT
{
  vec3 v_texCoord[2];
  vec3 v_worldPos;
}
fs_in;

layout(location = 0) out vec4 o_color; // output RGBA color (premultiplied alpha RGBA)

// Texture samplers:
uniform $$IMAGE_SAMPLER_TYPE$$ u_imgTex[2]; // images (scalar, red channel only)

// Image adjustment uniforms:
uniform vec2 u_imgSlopeIntercept[2]; // map texture to normalized intensity [0, 1], plus window/leveling
uniform vec2 u_imgMinMax[2];         // min/max image values (texture intenstiy units)
uniform vec2 u_imgThresholds[2];     // lower/upper image thresholds (texture intensity units)
uniform float u_imgOpacity[2];       // image opacity
uniform bool u_magentaCyan;          // flag to use magenta/cyan/white comparison colors

uniform mat4 img1Tex_T_img0Tex;
uniform vec3 u_tex0SamplingDirX;
uniform vec3 u_tex0SamplingDirY;
uniform vec3 u_texSamplingDirZ;

$$HELPER_FUNCTIONS$$

/// float textureLookup(sampler3D texture, vec3 texCoord);
$$TEXTURE_LOOKUP_FUNCTION$$

/// vec3 metricTexCoord(int imageIndex, vec2 patchOffset, int slabOffset);
$$METRIC_SAMPLING_FUNCTIONS$$

void main()
{
  vec4 overlapColor = vec4(0.0, 0.0, 0.0, 0.0);

  for (int i = 0; i < 2; ++i) {
    vec3 texCoord = metricTexCoord(i, vec2(0.0), 0);
    float val;
    switch (i) {
      case 0: {
        val = clamp(textureLookup(u_imgTex[0], texCoord, 0), u_imgMinMax[i][0], u_imgMinMax[i][1]);
        break;
      }
      case 1: {
        val = clamp(textureLookup(u_imgTex[1], texCoord, 1), u_imgMinMax[i][0], u_imgMinMax[i][1]);
        break;
      }
    }

    float norm = clamp(u_imgSlopeIntercept[i][0] * val + u_imgSlopeIntercept[i][1], 0.0, 1.0);
    float mask = float(isInsideTexture(texCoord));
    float alpha = u_imgOpacity[i] * mask * hardThreshold(val, u_imgThresholds[i]);
    overlapColor[i] = alpha * norm;
  }

  overlapColor.b = float(u_magentaCyan) * max(overlapColor.r, overlapColor.g);

  // Turn on overlap color if either the reference or moving image is present:
  // (note that this operation effectively thresholds out 0.0 as background)
  overlapColor.a = float((overlapColor.r > 0.0) || (overlapColor.g > 0.0));
  overlapColor.rgb = overlapColor.a * overlapColor.rgb;

  o_color = overlapColor;
}
