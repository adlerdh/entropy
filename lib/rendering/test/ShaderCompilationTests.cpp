#include "rendering/ShaderPreprocessor.h"
#include "rendering/ShaderProgramSetup.h"
#include "rendering/ShaderSourceSetup.h"
#include "rendering/ShaderTextureDimension.h"
#include "rendering/TextureLayout.h"
#include "rendering/common/ShaderType.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace shader_setup = rendering::shader_setup;

namespace
{

#if defined(ENTROPY_GLSLANG_VALIDATOR)
struct ShaderStageSource
{
  std::string extension;
  std::string source;
};

std::string shellQuote(const std::filesystem::path& path)
{
#ifdef _WIN32
  std::string result{"\""};
  for (const char character : path.string()) {
    result += character == '"' ? "\\\"" : std::string(1, character);
  }
  result += '"';
#else
  std::string result{"'"};
  for (const char character : path.string()) {
    result += character == '\'' ? "'\"'\"'" : std::string(1, character);
  }
  result += '\'';
#endif
  return result;
}

class TemporaryShaderDirectory
{
public:
  TemporaryShaderDirectory()
  {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    m_path = std::filesystem::temp_directory_path() / ("entropy-glsl-" + std::to_string(suffix));
    std::filesystem::create_directories(m_path);
  }

  ~TemporaryShaderDirectory()
  {
    std::error_code error;
    std::filesystem::remove_all(m_path, error);
  }

  TemporaryShaderDirectory(const TemporaryShaderDirectory&) = delete;
  TemporaryShaderDirectory& operator=(const TemporaryShaderDirectory&) = delete;

  const std::filesystem::path& path() const noexcept
  {
    return m_path;
  }

private:
  std::filesystem::path m_path;
};

void validateProgram(
  const std::string_view name,
  const std::vector<ShaderStageSource>& stages,
  const std::size_t programIndex)
{
  TemporaryShaderDirectory directory;
  std::string command = shellQuote(ENTROPY_GLSLANG_VALIDATOR) + " -l";
  std::size_t stageIndex = 0;
  for (const auto& stage : stages) {
    const std::filesystem::path path = directory.path() / ("program-" + std::to_string(programIndex) + "-stage-" +
                                                           std::to_string(stageIndex++) + stage.extension);
    std::ofstream stream(path, std::ios::binary);
    REQUIRE(stream);
    stream << stage.source;
    stream.close();
    command += " " + shellQuote(path);
  }

  const std::filesystem::path logPath = directory.path() / "glslang.log";
  command += " > " + shellQuote(logPath) + " 2>&1";
  const int result = std::system(command.c_str());
  std::ifstream logStream(logPath, std::ios::binary);
  const std::string log{std::istreambuf_iterator<char>{logStream}, std::istreambuf_iterator<char>{}};
  INFO("GLSL program: " << name);
  INFO(log);
  CHECK(result == 0);
}

std::string shader(const std::string& relativePath)
{
  return shader_setup::loadEmbeddedShaderSource("rendering/shaders/" + relativePath);
}

std::string preprocess(const std::string& relativePath, const rendering::ShaderReplacements& replacements)
{
  return rendering::preprocessShaderSource(shader(relativePath), replacements);
}
#endif

} // namespace

TEST_CASE("all assembled GLSL programs compile and link offline", "[rendering][shaders][glslang]")
{
#if !defined(ENTROPY_GLSLANG_VALIDATOR)
  SKIP("glslangValidator was not found when TestRendering was configured");
#else
  std::size_t programIndex = 0;
  const auto validate = [&programIndex](const std::string_view name, const std::vector<ShaderStageSource>& stages) {
    validateProgram(name, stages, programIndex++);
  };

  const auto setup = shader_setup::buildProgramSetup();
  for (const ShaderProgramType shaderType : setup.shaderTypes) {
    const auto& info = setup.shaderInfo.at(shaderType);
    for (const auto dimension : {rendering::TextureDimension::Texture3D, rendering::TextureDimension::Texture2D}) {
      const auto replacements = rendering::shaderReplacementsForTextureDimension(
        info.fsReplacements,
        dimension,
        setup.lookupReplacementSources);
      validate(
        to_string(shaderType),
        {{".vert", shader(info.vsFileName)}, {".frag", preprocess(info.fsFileName, replacements)}});
    }
  }

  validate("simple", {{".vert", shader("Simple.vs")}, {".frag", shader("Simple.fs")}});
  validate("ASCII cell mean", {{".vert", shader("AsciiPost.vs")}, {".frag", shader("AsciiCellMean.fs")}});
  validate("ASCII cell regions", {{".vert", shader("AsciiPost.vs")}, {".frag", shader("AsciiCellRegions.fs")}});
  validate(
    "ASCII luminance",
    {{".vert", shader("AsciiPost.vs")},
     {".frag", preprocess("AsciiPost.fs", {{"ASCII_COMPOSITE_FUNCTIONS", shader("functions/AsciiComposite.glsl")}})}});
  validate(
    "ASCII spatial",
    {{".vert", shader("AsciiPost.vs")},
     {".frag",
      preprocess("AsciiPostSpatial.fs", {{"ASCII_COMPOSITE_FUNCTIONS", shader("functions/AsciiComposite.glsl")}})}});
  validate("pixel edges", {{".vert", shader("AsciiPost.vs")}, {".frag", shader("PixelEdgePost.fs")}});

  const std::vector<ShaderStageSource> meshStages{{".vert", shader("mesh/Mesh.vs")}, {".frag", shader("mesh/Mesh.fs")}};
  validate("mesh", meshStages);
  validate(
    "mesh with edges",
    {{".vert", shader("mesh/MeshEdges.vs")},
     {".geom", shader("mesh/MeshEdges.gs")},
     {".frag", shader("mesh/Mesh.fs")}});
  validate("mesh shadow depth", {{".vert", shader("mesh/Mesh.vs")}, {".frag", shader("mesh/MeshShadowDepth.fs")}});
  validate(
    "mesh AO geometry",
    {{".vert", shader("mesh/MeshEdges.vs")},
     {".geom", shader("mesh/MeshEdges.gs")},
     {".frag", shader("mesh/AmbientOcclusionGeometry.fs")}});
  for (const char* fragment : {"AmbientOcclusionResolve.fs", "AmbientOcclusionFilter.fs"}) {
    validate(
      fragment,
      {{".vert", shader("mesh/FullScreenTriangle.vs")}, {".frag", shader("mesh/" + std::string(fragment))}});
  }

  const rendering::ShaderReplacements ddpReplacements{{"DDP_DEPTH_FUNCTIONS", shader("mesh/MeshDdpDepth.glsl")}};
  validate("mesh DDP init", {{".vert", shader("mesh/Mesh.vs")}, {".frag", shader("mesh/MeshDdpInit.fs")}});
  validate(
    "mesh DDP init with edges",
    {{".vert", shader("mesh/MeshEdges.vs")},
     {".geom", shader("mesh/MeshEdges.gs")},
     {".frag", shader("mesh/MeshDdpInit.fs")}});
  validate(
    "mesh DDP peel",
    {{".vert", shader("mesh/Mesh.vs")}, {".frag", preprocess("mesh/MeshDdpPeel.fs", ddpReplacements)}});
  validate(
    "mesh DDP peel with edges",
    {{".vert", shader("mesh/MeshEdges.vs")},
     {".geom", shader("mesh/MeshEdges.gs")},
     {".frag", preprocess("mesh/MeshDdpPeel.fs", ddpReplacements)}});
  for (const char* fragment : {"MeshDdpBackBlend.fs", "MeshDdpCompletion.fs", "MeshDdpResolve.fs"}) {
    validate(
      fragment,
      {{".vert", shader("mesh/FullScreenTriangle.vs")},
       {".frag", preprocess("mesh/" + std::string(fragment), ddpReplacements)}});
  }
  for (const char* fragment : {"MeshImagePlaneCompositeDdpInit.fs", "MeshImagePlaneCompositeDdpPeel.fs"}) {
    validate(
      fragment,
      {{".vert", shader("mesh/FullScreenTriangle.vs")},
       {".frag", preprocess("mesh/" + std::string(fragment), ddpReplacements)}});
  }
  validate(
    "analytic image-plane border",
    {{".vert", shader("mesh/FullScreenTriangle.vs")}, {".frag", shader("mesh/MeshImagePlaneBorder.fs")}});

  const rendering::ShaderReplacements raycastCommon{
    {"SAMPLE_TEX_COORD_FUNCTION", shader("functions/SampleTexCoord_Identity.glsl")},
    {"SAMPLE_IMAGE_VALUE_FUNCTION", shader("functions/SampleImageValue_Identity.glsl")},
    {"RAYCAST_JUMP_DISTANCE_FUNCTION", shader("functions/RaycastJumpDistance_Texture.glsl")}};
  validate(
    "raycast isosurface",
    {{".vert", shader("RaycastIso.vs")}, {".frag", preprocess("RaycastIso.fs", raycastCommon)}});
  validate(
    "warped raycast isosurface",
    {{".vert", shader("RaycastIso.vs")},
     {".frag",
      preprocess(
        "RaycastIso.fs",
        {{"SAMPLE_TEX_COORD_FUNCTION", shader("functions/SampleTexCoord_Deformation.glsl")},
         {"SAMPLE_IMAGE_VALUE_FUNCTION", shader("functions/SampleImageValue_Deformation.glsl")},
         {"RAYCAST_JUMP_DISTANCE_FUNCTION", shader("functions/RaycastJumpDistance_Disabled.glsl")}})}});

  const auto& imageInfo = setup.shaderInfo.at(ShaderProgramType::ImageGrayLinear);
  const auto imageDisplay = shader("mesh/MeshImagePlaneDisplay.glsl");
  for (const auto dimension : {rendering::TextureDimension::Texture3D, rendering::TextureDimension::Texture2D}) {
    auto replacements = rendering::shaderReplacementsForTextureDimension(
      imageInfo.fsReplacements,
      dimension,
      setup.lookupReplacementSources);
    replacements["UINT_TEXTURE_LOOKUP_FUNCTION"] = dimension == rendering::TextureDimension::Texture2D
                                                     ? shader("functions/UIntTextureLookup_ImagePlane_2D.glsl")
                                                     : shader("functions/UIntTextureLookup_Linear.glsl");
    replacements["IMAGE_PLANE_DISPLAY_FUNCTIONS"] = imageDisplay;
    replacements["DDP_DEPTH_FUNCTIONS"] = shader("mesh/MeshDdpDepth.glsl");
    validate(
      "opaque 3D image plane",
      {{".vert", shader("mesh/MeshImagePlane.vs")}, {".frag", preprocess(imageInfo.fsFileName, replacements)}});

    const auto& isocontourInfo = setup.shaderInfo.at(ShaderProgramType::IsoContourLinearFloating);
    validate(
      "opaque 3D isocontour plane",
      {{".vert", shader("mesh/MeshImagePlane.vs")},
       {".frag",
        preprocess(
          isocontourInfo.fsFileName,
          rendering::shaderReplacementsForTextureDimension(
            isocontourInfo.fsReplacements,
            dimension,
            setup.lookupReplacementSources))}});

    for (const char* fragment : {"MeshImagePlaneDdpInit.fs", "MeshImagePlaneDdpPeel.fs"}) {
      validate(
        fragment,
        {{".vert", shader("mesh/MeshImagePlane.vs")},
         {".frag", preprocess("mesh/" + std::string(fragment), replacements)}});
    }

    std::string compositeSource = shader("mesh/MeshImagePlaneDdpPeel.fs");
    const std::size_t versionLineEnd = compositeSource.find('\n');
    REQUIRE(versionLineEnd != std::string::npos);
    compositeSource.insert(versionLineEnd + 1u, "#define IMAGE_PLANE_COMPOSITE_PASS\n");
    validate(
      "image-plane stack composite",
      {{".vert", shader("mesh/MeshImagePlane.vs")},
       {".frag", rendering::preprocessShaderSource(compositeSource, replacements)}});
  }
#endif
}
