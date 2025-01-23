#include "rendering_old/ShaderNames.h"

const char* const BasicTxVertexUniforms::world_O_model = "world_O_model";
const char* const BasicTxVertexUniforms::camera_O_world = "camera_O_world";
const char* const BasicTxVertexUniforms::clip_O_camera = "clip_O_camera";
const char* const BasicTxVertexUniforms::world_O_model_inv_trans = "world_O_model_inv_trans";

const char* const BasicTxFragmentUniforms::cameraDir = "cameraDir";
const char* const BasicTxFragmentUniforms::cameraPos = "cameraPos";
const char* const BasicTxFragmentUniforms::cameraIsOrthographic = "cameraIsOrthographic";

const char* const MaterialFragmentUniforms::material_diffuse = "material.diffuse";
const char* const MaterialFragmentUniforms::material_specular = "material.specular";
const char* const MaterialFragmentUniforms::material_shininess = "material.shininess";

const char* const SimpleLightFragmentUniforms::simpleLight_position = "simpleLight.position";
const char* const SimpleLightFragmentUniforms::simpleLight_direction = "simpleLight.direction";
const char* const SimpleLightFragmentUniforms::simpleLight_ambient = "simpleLight.ambient";
const char* const SimpleLightFragmentUniforms::simpleLight_diffuse = "simpleLight.diffuse";
const char* const SimpleLightFragmentUniforms::simpleLight_specular = "simpleLight.specular";

const char* const DepthPeelerFragmentUniforms::depthBlenderTex = "depthBlenderTex";
const char* const DepthPeelerFragmentUniforms::frontBlenderTex = "frontBlenderTex";

namespace FlatProgram
{
const char* const name = "flat";

const char* const vert::color = "color";

const char* const frag::objectId = "objectId";
const char* const frag::opacity = "opacity";
} // namespace FlatProgram

namespace SimpleProgram
{
const char* const name = "simple";

const char* const vert::color = "color";

const char* const frag::tex2D = "ttt";
const char* const frag::objectId = "objectId";
const char* const frag::opacity = "opacity";
} // namespace SimpleProgram

namespace PolygonizerProgram
{
const char* const name = "polygonizer";

const char* const vert::tex_O_image = "tex_O_image";

const char* const geom::tex3D = "tex3D";
const char* const geom::triTableTex = "triTableTex";
const char* const geom::isolevel = "isolevel";
const char* const geom::vertDecals = "vertDecals";
const char* const geom::gradDeltas = "gradDeltas";
const char* const geom::world_O_tex = "world_O_tex";
} // namespace PolygonizerProgram

namespace BasicMeshProgram
{

const char* const name = "basic_mesh";

const char* const vert::worldClipPlanes[3]
  = {"worldClipPlanes[0]", "worldClipPlanes[1]", "worldClipPlanes[2]"};

const char* const frag::objectId = "objectId";
const char* const frag::masterOpacityMultiplier = "masterOpacityMultiplier";
const char* const frag::layerOpacities = "layerOpacities";

const char* const frag::xrayMode = "xrayMode";
const char* const frag::xrayPower = "xrayPower";

} // namespace BasicMeshProgram

namespace BasicMeshDualDepthPeelProgram
{

const char* const name = "basic_mesh_ddp";

} // namespace BasicMeshDualDepthPeelProgram

namespace MeshProgram
{
const char* const name = "mesh";

const char* const vert::worldClipPlanes[3]
  = {"worldClipPlanes[0]", "worldClipPlanes[1]", "worldClipPlanes[2]"};

const char* const vert::imageTexCoords_O_world = "imageTexCoords_O_world";
const char* const vert::labelTexCoords_O_world = "labelTexCoords_O_world";

const char* const frag::objectId = "objectId";
const char* const frag::masterOpacityMultiplier = "masterOpacityMultiplier";
const char* const frag::layerOpacities = "layerOpacities";
const char* const frag::layerPermutation = "layerPermutation";

const char* const frag::tex2D = "tex2D";
const char* const frag::imageTex3D = "imageTex3D";
const char* const frag::labelTex3D = "labelTex3D";
const char* const frag::labelColormapTexture = "labelColormapTexture";
const char* const frag::imageColorMapTexture = "imageColorMapTexture";

const char* const frag::slope = "slope";
const char* const frag::intercept = "intercept";

const char* const frag::image2dThresholds = "image2dThresholds";
const char* const frag::thresholds = "thresholds";

const char* const frag::cmapSlope = "cmapSlope";
const char* const frag::cmapIntercept = "cmapIntercept";

const char* const frag::autoHidingMode = "autoHidingMode";
const char* const frag::image3DThresholdMode = "image3DThresholdMode";
const char* const frag::xrayMode = "xrayMode";
const char* const frag::xrayPower = "xrayPower";

//const char* const frag::labelTexCoords_O_view = "labelTexCoords_O_view";
} // namespace MeshProgram

namespace MeshDDPPeelProgram
{
const char* const name = "meshDDPPeel";
} // namespace MeshDDPPeelProgram

namespace FlatPeelProgram
{
const char* const name = "flatDDPPeel";
} // namespace FlatPeelProgram

namespace DDPInitProgram
{
const char* const name = "meshDDPInit";

const char* const vert::worldClipPlanes[3]
  = {"worldClipPlanes[0]", "worldClipPlanes[1]", "worldClipPlanes[2]"};

const char* const frag::opaqueDepthTex = "opaqueDepthTex";
} // namespace DDPInitProgram

namespace DDPBlendProgram
{
const char* const name = "ddpBlend";

const char* const frag::tempTexture = "tempTexture";
} // namespace DDPBlendProgram

namespace DDPFinalProgram
{
const char* const name = "ddpFinal";

const char* const frag::frontBlenderTexture = "frontBlenderTex";
const char* const frag::backBlenderTexture = "backBlenderTex";
} // namespace DDPFinalProgram

namespace DebugProgram
{
const char* const name = "debugProgram";

const char* const frag::debugTexture = "debugTexture";
} // namespace DebugProgram
