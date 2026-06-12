#include "rendering/ascii/AsciiRenderer.h"

#include "common/Exception.hpp"
#include "common/Expected.h"
#include "common/Types.h"

#include "logic/app/Data.h"
#include "common/Viewport.h"

#include "logic/camera/CameraHelpers.h"
#include "logic/camera/CameraTypes.h"

#include "rendering/ascii/AsciiAtlasBaker.h"
#include "rendering/RenderData.h"
#include "rendering/common/ShaderType.h"
#include "rendering/utility/containers/Uniforms.h"
#include "rendering/utility/gl/GLShader.h"
#include "rendering/utility/gl/GLTextureTypes.h"

#include "windowing/View.h"

#include <cmrc/cmrc.hpp>

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <unordered_map>
#include <vector>

CMRC_DECLARE(fonts);
CMRC_DECLARE(shaders);

// Built-in ASCII character sets (order doesn't matter — atlas sorts by fill density):
static const std::vector<std::string> sk_asciiCharsets = {
  " .:-=+*#%@",
  "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'. ",
  " 01"};

namespace
{

const glm::vec2 sk_zeroVec2{0.0f, 0.0f};
const glm::vec3 sk_zeroVec3{0.0f, 0.0f, 0.0f};
const glm::ivec2 sk_zeroIVec2{0, 0};

// Sampler index for the ASCII atlas texture unit (must match Rendering.cpp)
const Uniforms::SamplerIndexType sk_asciiAtlasSampler{2};

std::string applyReplacements(const std::string& src, const std::unordered_map<std::string, std::string>& replacements)
{
  std::string result = src;
  for (const auto& [placeholder, replacement] : replacements) {
    std::size_t pos = 0;
    while ((pos = result.find(placeholder, pos)) != std::string::npos) {
      result.replace(pos, placeholder.length(), replacement);
      pos += replacement.length();
    }
  }
  return result;
}

entropy_expected::expected<std::unique_ptr<GLShaderProgram>, std::string> buildAsciiShaderProgram(
  const std::string& programName,
  const std::string& vsName,
  const std::string& fsName,
  const std::unordered_map<std::string, std::string>& fsReplacements,
  const Uniforms& vsUniforms,
  const Uniforms& fsUniforms)
{
  static const std::string shaderPath("app/rendering/shaders/");

  spdlog::debug("Creating ASCII shader program '{}'", programName);

  const auto filesystem = cmrc::shaders::get_filesystem();
  std::string vsSource;
  std::string fsSource;

  try {
    const cmrc::file vsData = filesystem.open(shaderPath + vsName);
    const cmrc::file fsData = filesystem.open(shaderPath + fsName);
    vsSource = std::string(vsData.begin(), vsData.end());
    fsSource = std::string(fsData.begin(), fsData.end());
  }
  catch (const std::exception& e) {
    return entropy_expected::unexpected(
      std::format("Exception loading ASCII shader for program {}: {}", programName, e.what()));
  }

  fsSource = applyReplacements(fsSource, fsReplacements);

  GLShader vs(vsName, ShaderType::Vertex, vsSource.c_str());
  vs.setRegisteredUniforms(vsUniforms);

  GLShader fs(fsName, ShaderType::Fragment, fsSource.c_str());
  fs.setRegisteredUniforms(fsUniforms);

  auto program = std::make_unique<GLShaderProgram>(programName);

  if (!program->attachShader(vs)) {
    return entropy_expected::unexpected(std::format("Unable to compile ASCII vertex shader {}", vsName));
  }
  if (!program->attachShader(fs)) {
    return entropy_expected::unexpected(std::format("Unable to compile ASCII fragment shader {}", fsName));
  }
  if (!program->link()) {
    return entropy_expected::unexpected(std::format("Failed to link ASCII shader program {}", programName));
  }

  spdlog::debug("Linked ASCII shader program '{}'", programName);
  return program;
}

} // namespace

AsciiRenderer::AsciiRenderer(AppData& appData)
  : m_appData(appData)
  , m_asciiAtlas()
  , m_sceneFbo("SceneAsciiPostFbo")
  , m_sceneColorTex(std::nullopt)
  , m_sceneFboSize{0, 0}
  , m_asciiPostVao()
  , m_asciiCellMeanTex(std::nullopt)
  , m_asciiCellMeanFbo(std::nullopt)
  , m_asciiCellMeanTexSize{0, 0}
  , m_asciiLumLutTex(std::nullopt)
  , m_asciiLumLutCellPx{0.0f, 0.0f}
  , m_asciiCellRegionsTex(std::nullopt)
  , m_asciiCellRegionsTexB(std::nullopt)
  , m_glyphProfilesPackedA()
  , m_glyphProfilesPackedB()
  , m_asciiSpatialProfileCellPx{-1.0f, -1.0f}
  , m_glyphRankToIndex()
  , m_asciiSpatialDensityWindow{3.0f}
  , m_glyphRegionMax{}
  , m_lastUploadedExponent{-1.0f}
  , m_glyphProfilesNormalized()
{
}

void AsciiRenderer::init()
{
  buildAtlas();
  m_asciiPostVao.generate();
}

void AsciiRenderer::registerShaderPrograms(
  std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>>& programs)
{
  // Load shared composite functions (rgb2hsv, hsv2rgb, sampleGlyphCoverage, asciiComposite)
  static const std::string shaderPath("app/rendering/shaders/");
  std::string compositeFunctions;
  try {
    const auto fs = cmrc::shaders::get_filesystem();
    const cmrc::file f = fs.open(shaderPath + "functions/AsciiComposite.glsl");
    compositeFunctions = std::string(f.begin(), f.end());
  }
  catch (const std::exception& e) {
    spdlog::warn("Could not load AsciiComposite.glsl: {}", e.what());
  }

  const std::unordered_map<std::string, std::string> compositePlaceholder{
    {"$$ASCII_COMPOSITE_FUNCTIONS$$", compositeFunctions}};

  Uniforms fsAsciiCellMeanUniforms;
  fsAsciiCellMeanUniforms.insertUniform("u_sceneTex", UniformType::Sampler, Uniforms::SamplerIndexType{3});
  fsAsciiCellMeanUniforms.insertUniform("u_viewSizePx", UniformType::Vec2, sk_zeroVec2);
  fsAsciiCellMeanUniforms.insertUniform("u_cellSizePx", UniformType::Vec2, sk_zeroVec2);

  Uniforms fsAsciiPostUniforms;
  fsAsciiPostUniforms.insertUniform("u_cellMeanTex", UniformType::Sampler, Uniforms::SamplerIndexType{4});
  fsAsciiPostUniforms.insertUniform("u_asciiAtlas", UniformType::Sampler, sk_asciiAtlasSampler);
  fsAsciiPostUniforms.insertUniform("u_viewSizePx", UniformType::Vec2, sk_zeroVec2);
  fsAsciiPostUniforms.insertUniform("u_asciiCellSizePx", UniformType::Vec2, sk_zeroVec2);
  fsAsciiPostUniforms.insertUniform("u_asciiGlyphCount", UniformType::Int, 0);
  fsAsciiPostUniforms.insertUniform("u_asciiFgColor", UniformType::Vec3, glm::vec3{1.f});
  fsAsciiPostUniforms.insertUniform("u_asciiBgColor", UniformType::Vec3, sk_zeroVec3);
  fsAsciiPostUniforms.insertUniform("u_asciiBgAlpha", UniformType::Float, 1.f);
  fsAsciiPostUniforms.insertUniform("u_asciiUseColormap", UniformType::Bool, false);
  fsAsciiPostUniforms.insertUniform("u_asciiSlotSizePx", UniformType::Vec2, sk_zeroVec2);
  fsAsciiPostUniforms.insertUniform("u_asciiSdfPadding", UniformType::Float, 0.f);
  fsAsciiPostUniforms.insertUniform("u_asciiPixDistScale", UniformType::Float, 0.f);

  Uniforms fsAsciiCellRegionsUniforms;
  fsAsciiCellRegionsUniforms.insertUniform("u_sceneTex", UniformType::Sampler, Uniforms::SamplerIndexType{3});
  fsAsciiCellRegionsUniforms.insertUniform("u_viewSizePx", UniformType::Vec2, sk_zeroVec2);
  fsAsciiCellRegionsUniforms.insertUniform("u_cellSizePx", UniformType::Vec2, sk_zeroVec2);
  fsAsciiCellRegionsUniforms.insertUniform("u_cellSizePxInt", UniformType::IVec2, sk_zeroIVec2);

  Uniforms fsAsciiPostSpatialUniforms;
  fsAsciiPostSpatialUniforms.insertUniform("u_cellRegionsTex", UniformType::Sampler, Uniforms::SamplerIndexType{5});
  fsAsciiPostSpatialUniforms.insertUniform("u_cellMeanTex", UniformType::Sampler, Uniforms::SamplerIndexType{4});
  fsAsciiPostSpatialUniforms.insertUniform("u_asciiAtlas", UniformType::Sampler, sk_asciiAtlasSampler);
  fsAsciiPostSpatialUniforms.insertUniform("u_asciiLumLut", UniformType::Sampler, Uniforms::SamplerIndexType{6});
  fsAsciiPostSpatialUniforms.insertUniform("u_viewSizePx", UniformType::Vec2, sk_zeroVec2);
  fsAsciiPostSpatialUniforms.insertUniform("u_asciiCellSizePx", UniformType::Vec2, sk_zeroVec2);
  fsAsciiPostSpatialUniforms.insertUniform("u_asciiGlyphCount", UniformType::Int, 0);
  fsAsciiPostSpatialUniforms.insertUniform("u_asciiFgColor", UniformType::Vec3, glm::vec3{1.f});
  fsAsciiPostSpatialUniforms.insertUniform("u_asciiBgColor", UniformType::Vec3, sk_zeroVec3);
  fsAsciiPostSpatialUniforms.insertUniform("u_asciiBgAlpha", UniformType::Float, 1.f);
  fsAsciiPostSpatialUniforms.insertUniform("u_asciiUseColormap", UniformType::Bool, false);
  fsAsciiPostSpatialUniforms.insertUniform("u_asciiSlotSizePx", UniformType::Vec2, sk_zeroVec2);
  fsAsciiPostSpatialUniforms.insertUniform("u_asciiSdfPadding", UniformType::Float, 0.f);
  fsAsciiPostSpatialUniforms.insertUniform("u_asciiPixDistScale", UniformType::Float, 0.f);

  struct AsciiShaderDesc
  {
    ShaderProgramType type;
    const char* vsFile;
    const char* fsFile;
    const std::unordered_map<std::string, std::string>* fsReplacements;
    Uniforms vsUniforms;
    Uniforms fsUniforms;
  };

  static const std::unordered_map<std::string, std::string> sk_noReplacements;

  const std::array<AsciiShaderDesc, 4> descs = {{
    {ShaderProgramType::AsciiCellMean,
     "AsciiPost.vs",
     "AsciiCellMean.fs",
     &sk_noReplacements,
     Uniforms{},
     fsAsciiCellMeanUniforms},
    {ShaderProgramType::AsciiPost,
     "AsciiPost.vs",
     "AsciiPost.fs",
     &compositePlaceholder,
     Uniforms{},
     fsAsciiPostUniforms},
    {ShaderProgramType::AsciiCellRegions,
     "AsciiPost.vs",
     "AsciiCellRegions.fs",
     &sk_noReplacements,
     Uniforms{},
     fsAsciiCellRegionsUniforms},
    {ShaderProgramType::AsciiPostSpatial,
     "AsciiPost.vs",
     "AsciiPostSpatial.fs",
     &compositePlaceholder,
     Uniforms{},
     fsAsciiPostSpatialUniforms},
  }};

  for (const auto& desc : descs) {
    auto prog = buildAsciiShaderProgram(
      to_string(desc.type),
      desc.vsFile,
      desc.fsFile,
      *desc.fsReplacements,
      desc.vsUniforms,
      desc.fsUniforms);

    if (prog) {
      programs.emplace(desc.type, std::move(*prog));
    }
    else {
      spdlog::error(prog.error());
      throw_debug(std::format("Failed to create ASCII shader program {}", to_string(desc.type)))
    }
  }
}

bool AsciiRenderer::enabled() const
{
  const auto& R = m_appData.renderData();
  return R.m_asciiEnabled && (m_asciiAtlas.textureId() != 0);
}

void AsciiRenderer::maybeRebuildAtlas()
{
  RenderData& R = m_appData.renderData();
  if (R.m_asciiAtlasNeedsRebuild) {
    R.m_asciiAtlasNeedsRebuild = false;
    buildAtlas();
  }
}

void AsciiRenderer::buildAtlas()
{
  const RenderData& R = m_appData.renderData();
  const int charsetIdx = std::clamp(R.m_asciiCharsetIndex, 0, static_cast<int>(sk_asciiCharsets.size()) - 1);
  const std::string& charset = sk_asciiCharsets[charsetIdx];

  try {
    const glm::ivec2 cellPx{16, 32};

    const unsigned char* ttfData = nullptr;
    int ttfBytes = 0;
    std::vector<unsigned char> fontBuf;

    try {
      auto fontFs = cmrc::fonts::get_filesystem();
      const cmrc::file f = fontFs.open("res/fonts/Cousine/Cousine-Regular.ttf");
      fontBuf.assign(f.begin(), f.end());
      ttfData = fontBuf.data();
      ttfBytes = static_cast<int>(fontBuf.size());
    }
    catch (const std::exception& fe) {
      spdlog::warn("Could not load Cousine font for ASCII atlas: {}", fe.what());
    }

    if (!m_asciiAtlas.build(ttfData, ttfBytes, charset, cellPx)) {
      spdlog::error("Failed to build ASCII atlas");
    }
    else {
      m_asciiLumLutCellPx = glm::vec2{0.0f};
      m_asciiLumLutTex.reset();
      m_asciiCellRegionsTex.reset();
      m_asciiCellRegionsTexB.reset();
      m_asciiSpatialProfileCellPx = glm::vec2{-1.0f, -1.0f};
    }
  }
  catch (const std::exception& e) {
    spdlog::error("Exception building ASCII atlas: {}", e.what());
  }
}

void AsciiRenderer::ensureSceneFboSize(glm::ivec2 deviceSize)
{
  if (deviceSize == m_sceneFboSize) {
    return;
  }
  m_sceneFboSize = deviceSize;

  using namespace tex;
  const glm::uvec3 texSize{static_cast<uint32_t>(deviceSize.x), static_cast<uint32_t>(deviceSize.y), 1u};

  if (!m_sceneColorTex) {
    m_sceneColorTex.emplace(tex::Target::Texture2D);
    m_sceneColorTex->generate();
    m_sceneColorTex->setMinificationFilter(tex::MinificationFilter::Linear);
    m_sceneColorTex->setMagnificationFilter(tex::MagnificationFilter::Nearest);
    m_sceneColorTex->setWrapMode(tex::WrapMode::ClampToEdge);
  }

  m_sceneColorTex->setSize(texSize);
  m_sceneColorTex->bind(std::nullopt);
  m_sceneColorTex
    ->setData(0, SizedInternalFormat::RGBA8_UNorm, BufferPixelFormat::RGBA, BufferPixelDataType::UInt8, nullptr);
  m_sceneColorTex->unbind();

  if (m_sceneFbo.id() == 0) {
    m_sceneFbo.generate();
  }
  m_sceneFbo.bind(fbo::TargetType::DrawAndRead);
  m_sceneFbo.attach2DTexture(fbo::TargetType::Draw, fbo::AttachmentType::Color, *m_sceneColorTex, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void AsciiRenderer::ensureAsciiCellFbo(glm::ivec2 viewSizeDevPx, glm::vec2 cellSizePxDev)
{
  const glm::ivec2 cellMeanSize{
    static_cast<int>(std::ceil(static_cast<float>(viewSizeDevPx.x) / cellSizePxDev.x)),
    static_cast<int>(std::ceil(static_cast<float>(viewSizeDevPx.y) / cellSizePxDev.y))};

  if (cellMeanSize == m_asciiCellMeanTexSize && m_asciiCellMeanTex && m_asciiCellMeanFbo) {
    return;
  }
  m_asciiCellMeanTexSize = cellMeanSize;

  using namespace tex;
  const glm::uvec3 texSize{static_cast<uint32_t>(cellMeanSize.x), static_cast<uint32_t>(cellMeanSize.y), 1u};

  if (!m_asciiCellMeanTex) {
    m_asciiCellMeanTex.emplace(tex::Target::Texture2D);
    m_asciiCellMeanTex->generate();
    m_asciiCellMeanTex->setMinificationFilter(tex::MinificationFilter::Nearest);
    m_asciiCellMeanTex->setMagnificationFilter(tex::MagnificationFilter::Nearest);
    m_asciiCellMeanTex->setWrapMode(tex::WrapMode::ClampToEdge);
  }

  m_asciiCellMeanTex->setSize(texSize);
  m_asciiCellMeanTex->bind(std::nullopt);
  m_asciiCellMeanTex
    ->setData(0, SizedInternalFormat::RGBA16F, BufferPixelFormat::RGBA, BufferPixelDataType::Float32, nullptr);
  m_asciiCellMeanTex->unbind();

  if (!m_asciiCellMeanFbo) {
    m_asciiCellMeanFbo.emplace("AsciiCellMeanFbo");
    m_asciiCellMeanFbo->generate();
  }
  m_asciiCellMeanFbo->bind(fbo::TargetType::DrawAndRead);
  m_asciiCellMeanFbo->attach2DTexture(fbo::TargetType::Draw, fbo::AttachmentType::Color, *m_asciiCellMeanTex, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void AsciiRenderer::render(
  std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>>& shaderPrograms,
  DrawViewFn drawImages,
  DrawViewFn drawImageBorders,
  DrawViewFn drawLandmarks,
  DrawViewFn drawAnnotations)
{
  const auto& R = m_appData.renderData();
  const bool renderLandmarksOnTop = R.m_globalLandmarkParams.renderOnTopOfAllImagePlanes;
  const bool renderAnnotationsOnTop = R.m_globalAnnotationParams.renderOnTopOfAllImagePlanes;

  const Viewport& windowVP = m_appData.windowData().viewport();
  const glm::vec4 deviceVP = windowVP.getDeviceAsVec4();
  const glm::vec4 logicalVP = windowVP.getAsVec4();
  const glm::ivec2 deviceSize{static_cast<int>(deviceVP[2]), static_cast<int>(deviceVP[3])};
  const glm::vec2 viewSizePx{static_cast<float>(deviceVP[2]), static_cast<float>(deviceVP[3])};
  const glm::vec2 dpr{
    (logicalVP[2] > 0.0f) ? (deviceVP[2] / logicalVP[2]) : 1.0f,
    (logicalVP[3] > 0.0f) ? (deviceVP[3] / logicalVP[3]) : 1.0f};

  ensureSceneFboSize(deviceSize);

  // Lock cell width to atlas slot aspect ratio
  {
    const glm::ivec2 slotPx = m_asciiAtlas.slotSize();
    if (slotPx.y > 0) {
      m_appData.renderData().m_asciiCellSizePx.x =
        m_appData.renderData().m_asciiCellSizePx.y * static_cast<float>(slotPx.x) / static_cast<float>(slotPx.y);
    }
  }

  const glm::vec2 cellPxDev = m_appData.renderData().m_asciiCellSizePx * dpr;
  ensureAsciiCellFbo(deviceSize, cellPxDev);

  // Rebuild coverage LUT if cell size changed
  if (cellPxDev != m_asciiLumLutCellPx && m_asciiAtlas.glyphCount() > 0) {
    m_asciiLumLutCellPx = cellPxDev;
    const int N = m_asciiAtlas.glyphCount();
    const auto coverage = m_asciiAtlas.computeRenderedCoverage(cellPxDev);

    std::vector<uint8_t> lut(256);
    if (N <= 1) {
      std::fill(lut.begin(), lut.end(), static_cast<uint8_t>(0));
    }
    else {
      const auto intLut = buildLumLut(coverage);
      for (int bin = 0; bin < 256; ++bin) {
        lut[static_cast<size_t>(bin)] = static_cast<uint8_t>(
          std::round(static_cast<float>(intLut[static_cast<size_t>(bin)]) / static_cast<float>(N - 1) * 255.0f));
      }
    }

    if (!m_asciiLumLutTex) {
      using namespace tex;
      m_asciiLumLutTex.emplace(tex::Target::Texture2D);
      m_asciiLumLutTex->generate();
      m_asciiLumLutTex->setMinificationFilter(tex::MinificationFilter::Nearest);
      m_asciiLumLutTex->setMagnificationFilter(tex::MagnificationFilter::Nearest);
      m_asciiLumLutTex->setWrapMode(tex::WrapMode::ClampToEdge);
      m_asciiLumLutTex->setSize(glm::uvec3{256u, 1u, 1u});
      m_asciiLumLutTex->bind(std::nullopt);
      m_asciiLumLutTex->setData(
        0,
        tex::SizedInternalFormat::R8_UNorm,
        tex::BufferPixelFormat::Red,
        tex::BufferPixelDataType::UInt8,
        nullptr);
      m_asciiLumLutTex->unbind();
    }

    glBindTexture(GL_TEXTURE_2D, m_asciiLumLutTex->id());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RED, GL_UNSIGNED_BYTE, lut.data());
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  // Rebuild spatial profiles if cell size changed
  if (
    R.m_asciiSpatialMode && (cellPxDev != m_asciiSpatialProfileCellPx || m_glyphProfilesPackedA.empty()) &&
    m_asciiAtlas.glyphCount() > 0)
  {
    m_asciiSpatialProfileCellPx = cellPxDev;
    auto profiles = m_asciiAtlas.computeRenderedSpatialProfiles(cellPxDev);
    const int N = m_asciiAtlas.glyphCount();
    const int safeN = std::min(N, AsciiAtlas::kMaxGlyphs);

    m_glyphRegionMax = computePerRegionMax(profiles);
    normalizeGlyphProfilesInPlace(profiles, m_glyphRegionMax);
    m_glyphProfilesNormalized = profiles;
    shapeGlyphProfilesInPlace(profiles, R.m_asciiSpatialExponent);
    m_lastUploadedExponent = R.m_asciiSpatialExponent;

    m_glyphProfilesPackedA.assign(static_cast<size_t>(AsciiAtlas::kMaxGlyphs), glm::vec4{1.0e9f});
    m_glyphProfilesPackedB.assign(static_cast<size_t>(AsciiAtlas::kMaxGlyphs), glm::vec4{1.0e9f});
    for (int g = 0; g < safeN; ++g) {
      const auto& p = profiles[static_cast<size_t>(g)].regionFill;
      m_glyphProfilesPackedA[static_cast<size_t>(g)] = {p[0], p[1], p[2], p[3]};
      m_glyphProfilesPackedB[static_cast<size_t>(g)] = {p[4], p[5], 0.0f, 0.0f};
    }

    const auto coverage = m_asciiAtlas.computeRenderedCoverage(cellPxDev);
    const auto rankOrder = buildCoverageRankOrder(coverage);

    m_glyphRankToIndex.assign(static_cast<size_t>(AsciiAtlas::kMaxGlyphs), 0);
    for (int r = 0; r < safeN; ++r)
      m_glyphRankToIndex[static_cast<size_t>(r)] = rankOrder[static_cast<size_t>(r)];
  }

  // Allocate/resize cell-regions textures for spatial mode
  if (R.m_asciiSpatialMode) {
    const glm::uvec3 regTexSize{
      static_cast<unsigned>(m_asciiCellMeanTexSize.x),
      static_cast<unsigned>(m_asciiCellMeanTexSize.y),
      1u};

    const bool needsReallocA = !m_asciiCellRegionsTex || (m_asciiCellRegionsTex->size() != regTexSize);
    if (needsReallocA) {
      if (!m_asciiCellRegionsTex) {
        m_asciiCellRegionsTex.emplace(tex::Target::Texture2D);
        m_asciiCellRegionsTex->generate();
        m_asciiCellRegionsTex->setMinificationFilter(tex::MinificationFilter::Nearest);
        m_asciiCellRegionsTex->setMagnificationFilter(tex::MagnificationFilter::Nearest);
        m_asciiCellRegionsTex->setWrapMode(tex::WrapMode::ClampToEdge);
      }
      m_asciiCellRegionsTex->setSize(regTexSize);
      m_asciiCellRegionsTex->bind(std::nullopt);
      m_asciiCellRegionsTex->setData(
        0,
        tex::SizedInternalFormat::RGBA16F,
        tex::BufferPixelFormat::RGBA,
        tex::BufferPixelDataType::Float32,
        nullptr);
      m_asciiCellRegionsTex->unbind();
    }

    const bool needsReallocB = !m_asciiCellRegionsTexB || (m_asciiCellRegionsTexB->size() != regTexSize);
    if (needsReallocB) {
      if (!m_asciiCellRegionsTexB) {
        m_asciiCellRegionsTexB.emplace(tex::Target::Texture2D);
        m_asciiCellRegionsTexB->generate();
        m_asciiCellRegionsTexB->setMinificationFilter(tex::MinificationFilter::Nearest);
        m_asciiCellRegionsTexB->setMagnificationFilter(tex::MagnificationFilter::Nearest);
        m_asciiCellRegionsTexB->setWrapMode(tex::WrapMode::ClampToEdge);
      }
      m_asciiCellRegionsTexB->setSize(regTexSize);
      m_asciiCellRegionsTexB->bind(std::nullopt);
      m_asciiCellRegionsTexB->setData(
        0,
        tex::SizedInternalFormat::RG16F,
        tex::BufferPixelFormat::RG,
        tex::BufferPixelDataType::Float32,
        nullptr);
      m_asciiCellRegionsTexB->unbind();
    }
  }

  GLShaderProgram& asciiCellMeanProg = *shaderPrograms.at(ShaderProgramType::AsciiCellMean);
  GLShaderProgram& asciiCellRegionsProg = *shaderPrograms.at(ShaderProgramType::AsciiCellRegions);
  GLShaderProgram& asciiPostProg = *shaderPrograms.at(ShaderProgramType::AsciiPost);
  GLShaderProgram& asciiPostSpatialProg = *shaderPrograms.at(ShaderProgramType::AsciiPostSpatial);

  // Pre-compute per-view data
  struct ViewData
  {
    View* view;
    glm::vec3 worldXhairsOffset;
    FrameBounds miewportViewBounds;
    GLint sx, sy;
    GLsizei sw, sh;
  };

  std::vector<ViewData> viewDataList;
  for (const auto& [viewUid, view] : m_appData.windowData().currentLayout().views()) {
    if (!view) {
      continue;
    }

    const glm::vec3 worldXhairsOffset =
      view->updateImageSlice(m_appData, m_appData.state().worldCrosshairs().worldOrigin());

    const auto miewportViewBounds =
      helper::computeMiewportFrameBounds(view->windowClipViewport(), m_appData.windowData().viewport().getAsVec4());

    const glm::vec4 clip = view->windowClipViewport();
    const GLint sx = static_cast<GLint>((clip[0] * 0.5f + 0.5f) * deviceVP[2]);
    const GLint sy = static_cast<GLint>((clip[1] * 0.5f + 0.5f) * deviceVP[3]);
    const GLsizei sw = static_cast<GLsizei>(clip[2] * 0.5f * deviceVP[2]);
    const GLsizei sh = static_cast<GLsizei>(clip[3] * 0.5f * deviceVP[3]);

    viewDataList.push_back(ViewData{view.get(), worldXhairsOffset, miewportViewBounds, sx, sy, sw, sh});
  }

  // PASS 1: Render all views into scene FBO
  m_sceneFbo.bind(fbo::TargetType::Draw);
  glViewport(
    static_cast<GLint>(deviceVP[0]),
    static_cast<GLint>(deviceVP[1]),
    static_cast<GLsizei>(deviceVP[2]),
    static_cast<GLsizei>(deviceVP[3]));
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  for (const auto& vd : viewDataList) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(vd.sx, vd.sy, vd.sw, vd.sh);
    drawImages(*vd.view, vd.miewportViewBounds, vd.worldXhairsOffset);
    glDisable(GL_SCISSOR_TEST);
  }

  // PASS 1.5: Downsample scene into cell data
  glBindFramebuffer(GL_FRAMEBUFFER, m_asciiCellMeanFbo->id());
  glViewport(0, 0, m_asciiCellMeanTexSize.x, m_asciiCellMeanTexSize.y);
  glDisable(GL_BLEND);
  glDisable(GL_STENCIL_TEST);

  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, m_sceneColorTex->id());

  m_asciiCellMeanFbo->attach2DTexture(fbo::TargetType::Draw, fbo::AttachmentType::Color, *m_asciiCellMeanTex, 0);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  asciiCellMeanProg.use();
  asciiCellMeanProg.setSamplerUniform("u_sceneTex", 3);
  asciiCellMeanProg.setUniform("u_viewSizePx", viewSizePx);
  asciiCellMeanProg.setUniform("u_cellSizePx", cellPxDev);
  m_asciiPostVao.bind();
  glDrawArrays(GL_TRIANGLES, 0, 3);
  m_asciiPostVao.release();
  asciiCellMeanProg.stopUse();

  if (R.m_asciiSpatialMode && m_asciiCellRegionsTex && m_asciiCellRegionsTexB) {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_asciiCellRegionsTex->id(), 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_asciiCellRegionsTexB->id(), 0);
    const GLenum drawBufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, drawBufs);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    asciiCellRegionsProg.use();
    asciiCellRegionsProg.setSamplerUniform("u_sceneTex", 3);
    asciiCellRegionsProg.setUniform("u_viewSizePx", viewSizePx);
    asciiCellRegionsProg.setUniform("u_cellSizePx", cellPxDev);
    asciiCellRegionsProg.setUniform("u_cellSizePxInt", glm::ivec2(glm::round(cellPxDev)));
    m_asciiPostVao.bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);
    m_asciiPostVao.release();
    asciiCellRegionsProg.stopUse();

    const GLenum singleBuf = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &singleBuf);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, 0, 0);
  }
  glEnable(GL_BLEND);
  glEnable(GL_STENCIL_TEST);

  // PASS 2: Composite to default FB
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(
    static_cast<GLint>(deviceVP[0]),
    static_cast<GLint>(deviceVP[1]),
    static_cast<GLsizei>(deviceVP[2]),
    static_cast<GLsizei>(deviceVP[3]));

  if (
    R.m_asciiSpatialMode && m_asciiCellRegionsTex && m_asciiCellRegionsTexB && !m_glyphProfilesPackedA.empty() &&
    m_asciiLumLutTex)
  {
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_asciiCellMeanTex->id());
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, m_asciiCellRegionsTex->id());
    glActiveTexture(GL_TEXTURE0 + sk_asciiAtlasSampler.index);
    glBindTexture(GL_TEXTURE_2D, m_asciiAtlas.textureId());
    if (m_asciiLumLutTex) {
      glActiveTexture(GL_TEXTURE6);
      glBindTexture(GL_TEXTURE_2D, m_asciiLumLutTex->id());
    }
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, m_asciiCellRegionsTexB->id());

    asciiPostSpatialProg.use();
    asciiPostSpatialProg.setSamplerUniform("u_cellMeanTex", 4);
    asciiPostSpatialProg.setSamplerUniform("u_cellRegionsTex", 5);
    asciiPostSpatialProg.setSamplerUniform("u_asciiAtlas", sk_asciiAtlasSampler.index);
    if (m_asciiLumLutTex) {
      asciiPostSpatialProg.setSamplerUniform("u_asciiLumLut", 6);
    }
    asciiPostSpatialProg.setSamplerUniform("u_cellRegionsTexB", 7);
    asciiPostSpatialProg.setUniform("u_viewSizePx", viewSizePx);
    asciiPostSpatialProg.setUniform("u_asciiCellSizePx", cellPxDev);
    const int safeN = std::min(m_asciiAtlas.glyphCount(), AsciiAtlas::kMaxGlyphs);
    asciiPostSpatialProg.setUniform("u_asciiGlyphCount", safeN);
    asciiPostSpatialProg.setUniform("u_asciiFgColor", R.m_asciiFgColor);
    asciiPostSpatialProg.setUniform("u_asciiBgColor", R.m_asciiBgColor);
    asciiPostSpatialProg.setUniform("u_asciiBgAlpha", R.m_asciiBgAlpha);
    asciiPostSpatialProg.setUniform("u_asciiUseColormap", R.m_asciiUseColormap);
    asciiPostSpatialProg.setUniform("u_asciiSpatialDensityWindow", m_asciiSpatialDensityWindow);
    {
      const glm::ivec2 slotPx = m_asciiAtlas.slotSize();
      asciiPostSpatialProg.setUniform(
        "u_asciiSlotSizePx",
        glm::vec2{static_cast<float>(slotPx.x), static_cast<float>(slotPx.y)});
    }
    asciiPostSpatialProg.setUniform("u_asciiSdfPadding", static_cast<float>(AsciiAtlas::kPadding));
    asciiPostSpatialProg.setUniform("u_asciiPixDistScale", AsciiAtlas::kPixDistScale);
    {
      const GLint locA = asciiPostSpatialProg.getUniformLocation("u_glyphProfilesA");
      if (locA >= 0) {
        glUniform4fv(locA, AsciiAtlas::kMaxGlyphs, glm::value_ptr(m_glyphProfilesPackedA[0]));
      }
      const GLint locB = asciiPostSpatialProg.getUniformLocation("u_glyphProfilesB");
      if (locB >= 0) {
        glUniform4fv(locB, AsciiAtlas::kMaxGlyphs, glm::value_ptr(m_glyphProfilesPackedB[0]));
      }
    }
    {
      const GLint rankToIdxLoc = asciiPostSpatialProg.getUniformLocation("u_glyphRankToIndex");
      if (rankToIdxLoc >= 0) glUniform1iv(rankToIdxLoc, AsciiAtlas::kMaxGlyphs, m_glyphRankToIndex.data());
    }
    glUniform4f(
      glGetUniformLocation(asciiPostSpatialProg.handle(), "u_regionMaxA"),
      m_glyphRegionMax[0],
      m_glyphRegionMax[1],
      m_glyphRegionMax[2],
      m_glyphRegionMax[3]);
    glUniform2f(
      glGetUniformLocation(asciiPostSpatialProg.handle(), "u_regionMaxB"),
      m_glyphRegionMax[4],
      m_glyphRegionMax[5]);
    glUniform1f(
      glGetUniformLocation(asciiPostSpatialProg.handle(), "u_asciiSpatialExponent"),
      R.m_asciiSpatialExponent);

    if (R.m_asciiSpatialExponent != m_lastUploadedExponent && !m_glyphProfilesNormalized.empty()) {
      auto shaped = m_glyphProfilesNormalized;
      shapeGlyphProfilesInPlace(shaped, R.m_asciiSpatialExponent);
      const int safeNExp = std::min(m_asciiAtlas.glyphCount(), AsciiAtlas::kMaxGlyphs);
      m_glyphProfilesPackedA.assign(static_cast<size_t>(AsciiAtlas::kMaxGlyphs), glm::vec4{1.0e9f});
      m_glyphProfilesPackedB.assign(static_cast<size_t>(AsciiAtlas::kMaxGlyphs), glm::vec4{1.0e9f});
      for (int g = 0; g < safeNExp; ++g) {
        const auto& p = shaped[static_cast<size_t>(g)].regionFill;
        m_glyphProfilesPackedA[static_cast<size_t>(g)] = {p[0], p[1], p[2], p[3]};
        m_glyphProfilesPackedB[static_cast<size_t>(g)] = {p[4], p[5], 0.0f, 0.0f};
      }
      const GLint locA = asciiPostSpatialProg.getUniformLocation("u_glyphProfilesA");
      if (locA >= 0) glUniform4fv(locA, AsciiAtlas::kMaxGlyphs, glm::value_ptr(m_glyphProfilesPackedA[0]));
      const GLint locB = asciiPostSpatialProg.getUniformLocation("u_glyphProfilesB");
      if (locB >= 0) glUniform4fv(locB, AsciiAtlas::kMaxGlyphs, glm::value_ptr(m_glyphProfilesPackedB[0]));
      m_lastUploadedExponent = R.m_asciiSpatialExponent;
    }

    for (const auto& vd : viewDataList) {
      glEnable(GL_SCISSOR_TEST);
      glScissor(vd.sx, vd.sy, vd.sw, vd.sh);
      m_asciiPostVao.bind();
      glDrawArrays(GL_TRIANGLES, 0, 3);
      m_asciiPostVao.release();
      glDisable(GL_SCISSOR_TEST);
    }

    asciiPostSpatialProg.stopUse();

    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (m_asciiLumLutTex) {
      glActiveTexture(GL_TEXTURE6);
      glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0 + sk_asciiAtlasSampler.index);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
  }
  else {
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_asciiCellMeanTex->id());
    glActiveTexture(GL_TEXTURE0 + sk_asciiAtlasSampler.index);
    glBindTexture(GL_TEXTURE_2D, m_asciiAtlas.textureId());
    if (m_asciiLumLutTex) {
      glActiveTexture(GL_TEXTURE5);
      glBindTexture(GL_TEXTURE_2D, m_asciiLumLutTex->id());
    }

    asciiPostProg.use();
    asciiPostProg.setSamplerUniform("u_cellMeanTex", 4);
    asciiPostProg.setSamplerUniform("u_asciiAtlas", sk_asciiAtlasSampler.index);
    if (m_asciiLumLutTex) {
      asciiPostProg.setSamplerUniform("u_asciiLumLut", 5);
    }
    asciiPostProg.setUniform("u_viewSizePx", viewSizePx);
    asciiPostProg.setUniform("u_asciiCellSizePx", cellPxDev);
    asciiPostProg.setUniform("u_asciiGlyphCount", m_asciiAtlas.glyphCount());
    asciiPostProg.setUniform("u_asciiFgColor", R.m_asciiFgColor);
    asciiPostProg.setUniform("u_asciiBgColor", R.m_asciiBgColor);
    asciiPostProg.setUniform("u_asciiBgAlpha", R.m_asciiBgAlpha);
    asciiPostProg.setUniform("u_asciiUseColormap", R.m_asciiUseColormap);
    {
      const glm::ivec2 slotPx = m_asciiAtlas.slotSize();
      asciiPostProg.setUniform(
        "u_asciiSlotSizePx",
        glm::vec2{static_cast<float>(slotPx.x), static_cast<float>(slotPx.y)});
    }
    asciiPostProg.setUniform("u_asciiSdfPadding", static_cast<float>(AsciiAtlas::kPadding));
    asciiPostProg.setUniform("u_asciiPixDistScale", AsciiAtlas::kPixDistScale);

    for (const auto& vd : viewDataList) {
      glEnable(GL_SCISSOR_TEST);
      glScissor(vd.sx, vd.sy, vd.sw, vd.sh);
      m_asciiPostVao.bind();
      glDrawArrays(GL_TRIANGLES, 0, 3);
      m_asciiPostVao.release();
      glDisable(GL_SCISSOR_TEST);
    }

    asciiPostProg.stopUse();

    if (m_asciiLumLutTex) {
      glActiveTexture(GL_TEXTURE5);
      glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0 + sk_asciiAtlasSampler.index);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
  }

  // Overlays on top of ASCII composite
  for (const auto& vd : viewDataList) {
    if (ViewRenderMode::VolumeRender != vd.view->renderMode()) {
      drawImageBorders(*vd.view, vd.miewportViewBounds, vd.worldXhairsOffset);
      if (renderLandmarksOnTop) {
        drawLandmarks(*vd.view, vd.miewportViewBounds, vd.worldXhairsOffset);
      }
      if (renderAnnotationsOnTop) {
        drawAnnotations(*vd.view, vd.miewportViewBounds, vd.worldXhairsOffset);
      }
    }
  }
}
