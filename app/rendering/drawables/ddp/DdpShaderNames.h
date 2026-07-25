#pragma once

namespace DDPBlendProgram
{
inline constexpr const char* name = "ddpBlend";

struct frag
{
  static constexpr const char* tempTexture = "tempTexture";
};
} // namespace DDPBlendProgram

namespace DDPFinalProgram
{
inline constexpr const char* name = "ddpFinal";

struct frag
{
  static constexpr const char* frontBlenderTexture = "frontBlenderTex";
  static constexpr const char* backBlenderTexture = "backBlenderTex";
};
} // namespace DDPFinalProgram

namespace DebugProgram
{
inline constexpr const char* name = "debugProgram";

struct frag
{
  static constexpr const char* debugTexture = "debugTexture";
};
} // namespace DebugProgram
