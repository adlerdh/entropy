#ifndef SHADER_NAMES_H
#define SHADER_NAMES_H

#include <cstddef>

struct BasicTxVertexUniforms
{
  static const char* const world_O_model;
  static const char* const camera_O_world;
  static const char* const clip_O_camera;
  static const char* const world_O_model_inv_trans;
};

struct BasicTxFragmentUniforms
{
  static const char* const cameraPos;
  static const char* const cameraDir;
  static const char* const cameraIsOrthographic;
};

struct MaterialFragmentUniforms
{
  static const char* const material_diffuse;
  static const char* const material_specular;
  static const char* const material_shininess;
};

struct SimpleLightFragmentUniforms
{
  static const char* const simpleLight_position;
  static const char* const simpleLight_direction;
  static const char* const simpleLight_ambient;
  static const char* const simpleLight_diffuse;
  static const char* const simpleLight_specular;
};

struct DepthPeelerFragmentUniforms
{
  static const char* const depthBlenderTex;
  static const char* const frontBlenderTex;
};

namespace FlatProgram
{
extern const char* const name;

struct vert : BasicTxVertexUniforms
{
  static const char* const color;
};

struct frag
{
  static const char* const objectId;
  static const char* const opacity;
};
} // namespace FlatProgram

namespace SimpleProgram
{
extern const char* const name;

struct vert : BasicTxVertexUniforms
{
  static const char* const color;
};

struct frag
{
  static const char* const tex2D;
  static const char* const objectId;
  static const char* const opacity;
};
} // namespace SimpleProgram

namespace PolygonizerProgram
{
extern const char* const name;

struct vert
{
  static const char* const tex_O_image;
};

struct geom
{
  static const char* const tex3D;
  static const char* const triTableTex;
  static const char* const isolevel;
  static const char* const vertDecals;
  static const char* const gradDeltas;
  static const char* const world_O_tex;
};
} // namespace PolygonizerProgram

namespace BasicMeshProgram
{
extern const char* const name;

struct vert : BasicTxVertexUniforms
{
  static const char* const worldClipPlanes[3];
};

struct frag : MaterialFragmentUniforms, SimpleLightFragmentUniforms, BasicTxFragmentUniforms
{
  static const char* const objectId;

  static const char* const masterOpacityMultiplier;
  static const char* const layerOpacities;

  static const char* const xrayMode;
  static const char* const xrayPower;
};
} // namespace BasicMeshProgram

namespace BasicMeshDualDepthPeelProgram
{
extern const char* const name;

struct vert : BasicMeshProgram::vert
{
};

struct frag : BasicMeshProgram::frag, DepthPeelerFragmentUniforms
{
};
} // namespace BasicMeshDualDepthPeelProgram

namespace MeshProgram
{
extern const char* const name;

struct vert : BasicTxVertexUniforms
{
  static const char* const worldClipPlanes[3];

  static const char* const imageTexCoords_O_world;
  static const char* const labelTexCoords_O_world;
};

struct frag : MaterialFragmentUniforms, SimpleLightFragmentUniforms, BasicTxFragmentUniforms
{
  static const char* const objectId;

  static const char* const masterOpacityMultiplier;
  static const char* const layerOpacities;
  static const char* const layerPermutation;

  static const char* const tex2D;
  static const char* const imageTex3D;
  static const char* const labelTex3D;
  static const char* const labelColormapTexture;
  static const char* const imageColorMapTexture;

  static const char* const slope;
  static const char* const intercept;

  static const char* const image2dThresholds;
  static const char* const thresholds;

  static const char* const cmapSlope;
  static const char* const cmapIntercept;

  static const char* const autoHidingMode;
  static const char* const image3DThresholdMode;
  static const char* const xrayMode;
  static const char* const xrayPower;

  //    static const char* const labelTexCoords_O_view;
};
} // namespace MeshProgram

namespace MeshDDPPeelProgram
{
extern const char* const name;

struct vert : MeshProgram::vert
{
};

struct frag : MeshProgram::frag, DepthPeelerFragmentUniforms
{
};
} // namespace MeshDDPPeelProgram

namespace FlatPeelProgram
{
extern const char* const name;

struct vert : FlatProgram::vert
{
};

struct frag : FlatProgram::frag, DepthPeelerFragmentUniforms
{
};
} // namespace FlatPeelProgram

namespace DDPInitProgram
{
extern const char* const name;

struct vert : BasicTxVertexUniforms
{
  static const char* const worldClipPlanes[3];
};

struct frag
{
  static const char* const opaqueDepthTex;
};
} // namespace DDPInitProgram

namespace DDPBlendProgram
{
extern const char* const name;

struct vert
{
};

struct frag
{
  static const char* const tempTexture;
};
} // namespace DDPBlendProgram

namespace DDPFinalProgram
{
extern const char* const name;

struct vert
{
};

struct frag
{
  static const char* const frontBlenderTexture;
  static const char* const backBlenderTexture;
};
} // namespace DDPFinalProgram

namespace DebugProgram
{
extern const char* const name;

struct vert
{
};

struct frag
{
  static const char* const debugTexture;
};
} // namespace DebugProgram

#endif // SHADER_NAMES_H
