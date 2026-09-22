#include "common/Viewport.h"
#include "rendering/JointHistogramBatching.h"
#include "rendering/JointHistogramRenderer.h"
#include "rendering/JointHistogramShaderUniforms.h"
#include "rendering/gl/GLShader.h"
#include "rendering/gl/GLShaderProgram.h"

#include <catch2/catch_test_macros.hpp>
#include <cmrc/cmrc.hpp>
#include <glad/glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

CMRC_DECLARE(shaders);

namespace
{
std::string shaderSource(const std::string& name)
{
  const auto file = cmrc::shaders::get_filesystem().open("rendering/shaders/" + name);
  return {file.begin(), file.end()};
}
} // namespace

TEST_CASE("joint histogram shaders are embedded", "[rendering][histogram]")
{
  const auto shaders = cmrc::shaders::get_filesystem();
  CHECK(shaders.is_file("rendering/shaders/JointHistogramScatter.vs"));
  CHECK(shaders.is_file("rendering/shaders/JointHistogramScatter.fs"));
  CHECK(shaders.is_file("rendering/shaders/JointHistogramDisplay.fs"));
}

TEST_CASE("joint histogram plot stays square inside views of either aspect ratio", "[rendering][histogram]")
{
  const FrameBounds wide{{100.0f, 200.0f, 500.0f, 300.0f}};
  const auto widePlot = joint_histogram::plotForFrame(wide);
  CHECK(widePlot.left == 272.0f);
  CHECK(widePlot.top == 217.0f);
  CHECK(widePlot.size == 214.0f);

  const FrameBounds tall{{100.0f, 200.0f, 300.0f, 500.0f}};
  const auto tallPlot = joint_histogram::plotForFrame(tall);
  CHECK(tallPlot.left == 176.0f);
  CHECK(tallPlot.top == 321.0f);
  CHECK(tallPlot.size == 206.0f);
}

TEST_CASE("joint histogram batches interleave across and exactly partition the voxel domain", "[rendering][histogram]")
{
  constexpr std::uint64_t voxelCount = 23u;
  constexpr std::uint64_t maximumBatchSize = 5u;
  constexpr std::uint64_t batchCount = rendering::jointHistogramBatchCount(voxelCount, maximumBatchSize);
  STATIC_CHECK(batchCount == 5u);

  std::vector<int> visits(voxelCount, 0);
  for (std::uint64_t batchIndex = 0; batchIndex < batchCount; ++batchIndex) {
    const rendering::JointHistogramBatch batch =
      rendering::jointHistogramBatch(voxelCount, maximumBatchSize, batchIndex);
    CHECK(batch.offset == batchIndex);
    CHECK(batch.stride == batchCount);
    CHECK(batch.size <= maximumBatchSize);
    for (std::uint64_t localIndex = 0; localIndex < batch.size; ++localIndex) {
      const std::uint64_t voxelIndex = batch.offset + localIndex * batch.stride;
      REQUIRE(voxelIndex < voxelCount);
      ++visits[voxelIndex];
    }
  }
  for (const int visitCount : visits) {
    CHECK(visitCount == 1);
  }
}

TEST_CASE("instanced histogram batches visit every voxel across all slices", "[rendering][histogram]")
{
  constexpr std::uint64_t planeSize = 23, depth = 7, limit = 30;
  const auto batches = rendering::jointHistogramPlaneBatchCount(planeSize, depth, limit);
  std::vector<int> visits(planeSize * depth, 0);
  for (std::uint64_t b = 0; b < batches; ++b) {
    const auto batch = rendering::jointHistogramBatch(planeSize, limit / depth, b);
    CHECK(batch.size * depth <= limit);
    for (std::uint64_t z = 0; z < depth; ++z) {
      for (std::uint64_t i = 0; i < batch.size; ++i) {
        ++visits.at(z * planeSize + batch.offset + i * batch.stride);
      }
    }
  }
  for (int count : visits) {
    CHECK(count == 1);
  }
}

TEST_CASE("histogram scheduling bounds previews and cautiously adapts to GPU cost", "[rendering][histogram]")
{
  using namespace rendering;
  CHECK(jointHistogramSubmissionLimit(0.1, true) == kJointHistogramPreviewVoxels);
  CHECK(jointHistogramSubmissionLimit(0.1, false) == kJointHistogramRefinementVoxels);
  CHECK(jointHistogramSubmissionLimit(100.0, true) == 60'000u);
  CHECK(jointHistogramSubmissionLimit(100.0, false) == 60'000u);
  CHECK(jointHistogramSubmissionLimit(4'000'000.0, true) == 1u);
  CHECK(jointHistogramUpdateCost(4.0, 100.0) == 100.0);
  const double recovered = jointHistogramUpdateCost(100.0, 1.0);
  CHECK(recovered > 85.0);
  CHECK(recovered < 100.0);
  CHECK(jointHistogramUpdateCost(0.1, 0.0) == 0.1);
}

TEST_CASE("joint histogram shaders link and scatter in OpenGL 3.3", "[rendering][histogram][gl]")
{
#if defined(__APPLE__)
  if (std::getenv("ENTROPY_TEST_GL33") == nullptr) {
    SKIP("Set ENTROPY_TEST_GL33 to opt in to macOS OpenGL context tests");
  }
#endif
  if (glfwInit() != GLFW_TRUE) {
    SKIP("No OpenGL context is available on this test worker");
  }
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
  GLFWwindow* window = glfwCreateWindow(32, 32, "Joint histogram shader test", nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    SKIP("No OpenGL 3.3 context is available on this test worker");
  }
  glfwMakeContextCurrent(window);
  REQUIRE(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0);
  {
    const std::string scatterVertex = shaderSource("JointHistogramScatter.vs");
    const std::string scatterFragment = shaderSource("JointHistogramScatter.fs");
    for (int variant = 0; variant < 4; ++variant) {
      std::string source = scatterVertex;
      std::string defines;
      if ((variant & 1) != 0) {
        defines += "#define FIXED_IMAGE_2D\n";
      }
      if ((variant & 2) != 0) {
        defines += "#define MOVING_IMAGE_2D\n";
      }
      source.insert(source.find('\n') + 1, defines);
      GLShader vertex("joint histogram scatter vertex", ShaderType::Vertex, source.c_str());
      GLShader fragment("joint histogram scatter fragment", ShaderType::Fragment, scatterFragment.c_str());
      vertex.setRegisteredUniforms(rendering::jointHistogramScatterUniforms());
      REQUIRE(vertex.isCompiled());
      REQUIRE(fragment.isCompiled());
      GLShaderProgram program("joint histogram scatter");
      REQUIRE(program.attachShader(vertex));
      REQUIRE(program.attachShader(fragment));
      REQUIRE(program.link());
      if (variant == 0) {
        constexpr std::array fixedValues{0.1f, 0.4f, 0.7f, 0.9f};
        constexpr std::array movingValues{0.2f, 0.5f, 0.8f, 0.1f};
        constexpr std::array
          identity{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
        constexpr std::array zero{0.0f, 0.0f, 0.0f, 0.0f};
        GLuint images[3]{};
        glGenTextures(3, images);
        for (int i = 0; i < 3; ++i) {
          glActiveTexture(GL_TEXTURE0 + i);
          glBindTexture(GL_TEXTURE_3D, images[i]);
          glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
          glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
          const float* data = i == 0 ? fixedValues.data() : i == 1 ? movingValues.data() : zero.data();
          glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, i == 2 ? 1 : 2, i == 2 ? 1 : 2, 1, 0, GL_RED, GL_FLOAT, data);
        }
        for (int unit = 3; unit <= 8; ++unit) {
          glActiveTexture(GL_TEXTURE0 + unit);
          glBindTexture(GL_TEXTURE_3D, images[2]);
        }
        GLuint counts = 0;
        glGenTextures(1, &counts);
        glBindTexture(GL_TEXTURE_2D, counts);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 4, 4, 0, GL_RED, GL_FLOAT, nullptr);
        GLuint framebuffer = 0;
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, counts, 0);
        REQUIRE(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        glViewport(0, 0, 4, 4);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ONE, GL_ONE);
        glClearBufferfv(GL_COLOR, 0, zero.data());
        program.use();
        const auto loc = [&program](const char* name) {
          return glGetUniformLocation(program.handle(), name);
        };
        glUniform1i(loc("u_fixedImage"), 0);
        glUniform1i(loc("u_movingImage"), 1);
        glUniform2i(loc("u_fixedSizeXY"), 2, 2);
        glUniform1i(loc("u_fixedDepth"), 1);
        glUniform1i(loc("u_bins"), 4);
        glUniform1i(loc("u_batchOffset"), 0);
        glUniform1i(loc("u_batchStride"), 1);
        glUniformMatrix4fv(loc("u_world_T_fixedTexture"), 1, GL_FALSE, identity.data());
        for (int i = 0; i < 2; ++i) {
          const std::string index = "[" + std::to_string(i) + "]";
          glUniformMatrix4fv(loc(("u_tex_T_world" + index).c_str()), 1, GL_FALSE, identity.data());
          glUniformMatrix4fv(loc(("u_defTex_T_world" + index).c_str()), 1, GL_FALSE, identity.data());
          glUniform2f(loc(("u_normalized_T_texture" + index).c_str()), 1.0f, 0.0f);
          glUniform1f(loc(("u_defSlope_native_T_texture" + index).c_str()), 1.0f);
          glUniform1f(loc(("u_deformationStrength" + index).c_str()), 0.0f);
          glUniform1i(loc(("u_defInterleaved" + index).c_str()), 0);
          glUniform1i(loc(("u_warpEnabled" + index).c_str()), 0);
          for (int component = 0; component < 3; ++component) {
            const std::string sampler = "u_defTex" + std::to_string(i) + "[" + std::to_string(component) + "]";
            glUniform1i(loc(sampler.c_str()), 3 + 3 * i + component);
          }
        }
        GLuint vao = 0;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glEnable(GL_PROGRAM_POINT_SIZE);
        glDrawArrays(GL_POINTS, 0, 4);
        std::array<float, 16> histogram{};
        glReadPixels(0, 0, 4, 4, GL_RED, GL_FLOAT, histogram.data());
        CHECK(glGetError() == GL_NO_ERROR);
        CHECK(histogram[0] == 1.0f);
        CHECK(histogram[2 * 4 + 1] == 1.0f);
        CHECK(histogram[3 * 4 + 2] == 1.0f);
        CHECK(histogram[3] == 1.0f);

        if (std::getenv("ENTROPY_HISTOGRAM_BENCHMARK") != nullptr) {
          // Prior procedural scatter: no fixed cache, no background spreading, one million voxels per draw.
          // Submit without frame waits to measure a lower bound on its former completion time.
          constexpr std::size_t count = std::size_t{256} * 256u * 256u;
          std::vector<float> values(count);
          for (int slot = 0; slot < 2; ++slot) {
            for (std::size_t i = 0; i < count; ++i) {
              const int bin = i % 10 < 8 ? 0 : static_cast<int>((slot == 0 ? i : i / 4) % 4);
              values[i] = bin == 0 ? 0.0f : (static_cast<float>(bin) + 0.25f) / 4.0f;
            }
            glActiveTexture(GL_TEXTURE0 + slot);
            glBindTexture(GL_TEXTURE_3D, images[slot]);
            glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, 256, 256, 256, 0, GL_RED, GL_FLOAT, values.data());
          }
          glActiveTexture(GL_TEXTURE9);
          glBindTexture(GL_TEXTURE_2D, counts);
          glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 512, 512, 0, GL_RED, GL_FLOAT, nullptr);
          glViewport(0, 0, 512, 512);
          glClearBufferfv(GL_COLOR, 0, zero.data());
          glUniform2i(loc("u_fixedSizeXY"), 256, 256);
          glUniform1i(loc("u_fixedDepth"), 256);
          glUniform1i(loc("u_bins"), 512);
          const auto batches = rendering::jointHistogramBatchCount(count, rendering::kJointHistogramMaxVoxelsPerBatch);
          glFinish();
          const auto start = std::chrono::steady_clock::now();
          for (std::uint64_t b = 0; b < batches; ++b) {
            const auto batch = rendering::jointHistogramBatch(count, rendering::kJointHistogramMaxVoxelsPerBatch, b);
            glUniform1i(loc("u_batchOffset"), static_cast<GLint>(batch.offset));
            glUniform1i(loc("u_batchStride"), static_cast<GLint>(batch.stride));
            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(batch.size));
            glFinish();
          }
          const double elapsed =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
          std::cout << "Histogram previous scatter (frame waits excluded): " << elapsed << " ms\n";
          CHECK(glGetError() == GL_NO_ERROR);
        }
        glBindVertexArray(0);
        glDeleteVertexArrays(1, &vao);
        GLShaderProgram::stopUse();
        glDisable(GL_PROGRAM_POINT_SIZE);
        glDisable(GL_BLEND);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteTextures(1, &counts);
        glDeleteTextures(3, images);
      }
    }
    const std::string displayVertex = shaderSource("AsciiPost.vs");
    const std::string displayFragment = shaderSource("JointHistogramDisplay.fs");
    GLShader vertex("joint histogram display vertex", ShaderType::Vertex, displayVertex.c_str());
    GLShader fragment("joint histogram display fragment", ShaderType::Fragment, displayFragment.c_str());
    fragment.setRegisteredUniforms(rendering::jointHistogramDisplayUniforms());
    REQUIRE(vertex.isCompiled());
    REQUIRE(fragment.isCompiled());
    GLShaderProgram program("joint histogram display");
    REQUIRE(program.attachShader(vertex));
    REQUIRE(program.attachShader(fragment));
    CHECK(program.link());

    constexpr std::array fixedValues{0.1f, 0.4f, 0.7f, 0.9f};
    constexpr std::array movingValues{0.2f, 0.5f, 0.8f, 0.1f};
    constexpr std::array zero{0.0f, 0.0f, 0.0f, 0.0f};
    GLuint images[3]{};
    glGenTextures(3, images);
    for (int i = 0; i < 3; ++i) {
      glActiveTexture(GL_TEXTURE0 + i);
      glBindTexture(GL_TEXTURE_3D, images[i]);
      glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      const float* data = i == 0 ? fixedValues.data() : i == 1 ? movingValues.data() : zero.data();
      glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, i == 2 ? 1 : 2, i == 2 ? 1 : 2, 1, 0, GL_RED, GL_FLOAT, data);
    }
    for (int unit = 3; unit <= 8; ++unit) {
      glActiveTexture(GL_TEXTURE0 + unit);
      glBindTexture(GL_TEXTURE_3D, images[2]);
    }
    constexpr std::array colorMap{0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f};
    GLuint colorMapTexture = 0;
    glGenTextures(1, &colorMapTexture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_1D, colorMapTexture);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F, 2, 0, GL_RGBA, GL_FLOAT, colorMap.data());
    GLuint output = 0;
    glGenTextures(1, &output);
    glBindTexture(GL_TEXTURE_2D, output);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    GLuint outputFramebuffer = 0;
    glGenFramebuffers(1, &outputFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output, 0);
    REQUIRE(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glViewport(16, 24, 64, 64);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    rendering::RenderSettings::MetricParams metric;
    rendering::JointHistogramRenderer::Inputs inputs;
    inputs.fixedDimensions = {2u, 2u, 1u};
    inputs.metric = metric;
    inputs.bins = 4;
    inputs.sourceTextureIds[0] = images[0];
    inputs.sourceTextureIds[1] = images[1];
    rendering::JointHistogramRenderer renderer;
    Viewport viewport{8.0f, 12.0f, 32.0f, 32.0f};
    viewport.setDevicePixelRatio({2.0f, 2.0f});
    REQUIRE_NOTHROW(renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport));
    const auto readCounts = [&renderer, &inputs]() {
      const auto& texture = renderer.countsTexture();
      glActiveTexture(GL_TEXTURE9);
      glBindTexture(GL_TEXTURE_2D, texture.id());
      const int width = std::max(inputs.bins, 16);
      std::vector<float> atlas(static_cast<std::size_t>(width) * static_cast<std::size_t>(inputs.bins + 16));
      glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, atlas.data());
      std::vector<float> histogram(static_cast<std::size_t>(inputs.bins) * static_cast<std::size_t>(inputs.bins));
      for (int y = 0; y < inputs.bins; ++y) {
        const auto sourceOffset = static_cast<std::ptrdiff_t>(y) * width;
        const auto destinationOffset = static_cast<std::ptrdiff_t>(y) * inputs.bins;
        std::copy_n(atlas.begin() + sourceOffset, inputs.bins, histogram.begin() + destinationOffset);
      }
      return histogram;
    };
    const auto initialCounts = readCounts();
    CHECK(initialCounts[0] == 1.0f);
    CHECK(initialCounts[9] == 1.0f);
    CHECK(initialCounts[14] == 1.0f);
    CHECK(initialCounts[3] == 1.0f);
    CHECK(std::accumulate(initialCounts.begin(), initialCounts.end(), 0.0f) == 4.0f);
    std::array<unsigned char, 4> countedPixel{};
    glReadPixels(30, 52, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, countedPixel.data());
    CHECK(countedPixel[0] > 0u);
    std::array<unsigned char, 4> emptyPixel{};
    glReadPixels(38, 52, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, emptyPixel.data());
    CHECK(emptyPixel[0] == 0u);
    CHECK(emptyPixel[1] > 0u);
    inputs.metric.m_cmapContinuous = false;
    inputs.metric.m_cmapQuantizationLevels = 2;
    REQUIRE_NOTHROW(renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport));
    std::array<unsigned char, 4> quantizedPixel{};
    glReadPixels(30, 52, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, quantizedPixel.data());
    CHECK(quantizedPixel[0] == 0u);
    CHECK(quantizedPixel[1] > 0u);
    inputs.metric.m_cmapContinuous = true;
    inputs.logarithmicScale = false;
    REQUIRE_NOTHROW(renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport));
    std::array<unsigned char, 4> linearPixel{};
    glReadPixels(30, 52, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, linearPixel.data());
    CHECK(linearPixel[0] < countedPixel[0]);
    inputs.logarithmicScale = true;
    REQUIRE_NOTHROW(renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport));
    std::array<unsigned char, 4> outsidePixel{};
    glReadPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, outsidePixel.data());
    CHECK(outsidePixel[0] == 0u);
    CHECK(glGetError() == GL_NO_ERROR);

    // Interleaved images must use each image's active component, and changing only that component selection must
    // invalidate both the fixed-value cache and the accumulated histogram.
    std::array<float, 16> fixedRgba{};
    std::array<float, 16> movingRgba{};
    for (std::size_t i = 0; i < fixedValues.size(); ++i) {
      fixedRgba[4 * i + 1] = fixedValues[i];
      movingRgba[4 * i + 2] = movingValues[i];
    }
    GLuint rgbaImages[2]{};
    glGenTextures(2, rgbaImages);
    for (int i = 0; i < 2; ++i) {
      glActiveTexture(GL_TEXTURE0 + i);
      glBindTexture(GL_TEXTURE_3D, rgbaImages[i]);
      glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexImage3D(
        GL_TEXTURE_3D,
        0,
        GL_RGBA32F,
        2,
        2,
        1,
        0,
        GL_RGBA,
        GL_FLOAT,
        i == 0 ? fixedRgba.data() : movingRgba.data());
      inputs.sourceTextureIds[i] = rgbaImages[i];
      ++inputs.sourceTextureRevisions[i];
    }
    inputs.textureComponents = {0, 0};
    renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport);
    glFinish();
    auto componentCounts = readCounts();
    CHECK(componentCounts[0] == 4.0f);
    CHECK(std::accumulate(componentCounts.begin(), componentCounts.end(), 0.0f) == 4.0f);

    inputs.textureComponents = {1, 2};
    renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport);
    glFinish();
    componentCounts = readCounts();
    CHECK(componentCounts[0] == 1.0f);
    CHECK(componentCounts[9] == 1.0f);
    CHECK(componentCounts[14] == 1.0f);
    CHECK(componentCounts[3] == 1.0f);
    CHECK(std::accumulate(componentCounts.begin(), componentCounts.end(), 0.0f) == 4.0f);

    inputs.textureComponents = {0, 0};
    for (int i = 0; i < 2; ++i) {
      glActiveTexture(GL_TEXTURE0 + i);
      glBindTexture(GL_TEXTURE_3D, images[i]);
      inputs.sourceTextureIds[i] = images[i];
      ++inputs.sourceTextureRevisions[i];
    }
    glDeleteTextures(2, rgbaImages);
    renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport);
    glFinish();

    const float changedValue = 0.6f;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, images[0]);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 1, 1, 1, GL_RED, GL_FLOAT, &changedValue);
    REQUIRE_NOTHROW(renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport));
    std::array<unsigned char, 4> cachedBin{};
    glReadPixels(30, 52, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, cachedBin.data());
    CHECK(cachedBin[0] > 0u);
    ++inputs.sourceTextureRevisions[0];
    REQUIRE_NOTHROW(renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport));
    std::array<unsigned char, 4> oldBin{};
    glReadPixels(30, 52, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, oldBin.data());
    CHECK(oldBin[0] == 0u);
    std::array<unsigned char, 4> newBin{};
    glReadPixels(46, 52, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, newBin.data());
    CHECK(newBin[0] > 0u);

    // Background counting must exclude samples outside the moving image, in both shader paths.
    for (int i = 0; i < 2; ++i) {
      glActiveTexture(GL_TEXTURE0 + i);
      glBindTexture(GL_TEXTURE_3D, images[i]);
      glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 2, 2, 1, GL_RED, GL_FLOAT, zero.data());
      ++inputs.sourceTextureRevisions[i];
    }
    inputs.texture_T_world[1][3].x = 0.5f;
    glFinish();
    renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport);
    const auto fastBackground = readCounts();
    CHECK(fastBackground[0] == 2.0f);
    CHECK(std::accumulate(fastBackground.begin(), fastBackground.end(), 0.0f) == 2.0f);
    inputs.deformations[0].enabled = true; // Zero displacement, exercising the general shader.
    glFinish();
    renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport);
    CHECK(readCounts() == fastBackground);
    inputs.deformations[0].enabled = false;

    GLuint planarImages[2]{};
    glGenTextures(2, planarImages);
    for (int i = 0; i < 2; ++i) {
      glActiveTexture(GL_TEXTURE0 + i);
      glBindTexture(GL_TEXTURE_2D, planarImages[i]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 2, 2, 0, GL_RED, GL_FLOAT, zero.data());
    }
    for (int variant = 1; variant < 4; ++variant) {
      for (int i = 0; i < 2; ++i) {
        const bool planar = (variant & (1 << i)) != 0;
        inputs.layouts[i].dimension =
          planar ? rendering::TextureDimension::Texture2D : rendering::TextureDimension::Texture3D;
        inputs.sourceTextureIds[i] = planar ? planarImages[i] : images[i];
      }
      glFinish();
      renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport);
      CHECK(readCounts() == fastBackground);
    }
    glDeleteTextures(2, planarImages);
    CHECK(glGetError() == GL_NO_ERROR);

    // Compare complete multi-batch histograms against CPU counts, including many background pairs.
    const bool benchmark = std::getenv("ENTROPY_HISTOGRAM_BENCHMARK") != nullptr;
    inputs.bins = benchmark ? 512 : 4;
    inputs.fixedDimensions = benchmark ? glm::uvec3{256, 256, 256} : glm::uvec3{128, 127, 67};
    const std::size_t total =
      std::size_t{inputs.fixedDimensions.x} * inputs.fixedDimensions.y * inputs.fixedDimensions.z;
    std::vector<float> fixedVolume(total), movingVolume(total);
    std::vector<float> expectedCounts(
      static_cast<std::size_t>(inputs.bins) * static_cast<std::size_t>(inputs.bins),
      0.0f);
    for (std::size_t i = 0; i < total; ++i) {
      const int fixedBin = i % 10 < 8 ? 0 : static_cast<int>(i % 4);
      const int movingBin = i % 10 < 8 ? 0 : static_cast<int>((i / 4) % 4);
      fixedVolume[i] = fixedBin == 0 ? 0.0f : (static_cast<float>(fixedBin) + 0.25f) / 4.0f;
      movingVolume[i] = movingBin == 0 ? 0.0f : (static_cast<float>(movingBin) + 0.25f) / 4.0f;
      const int x = static_cast<int>(fixedVolume[i] * inputs.bins);
      const int y = static_cast<int>(movingVolume[i] * inputs.bins);
      ++expectedCounts[y * inputs.bins + x];
    }
    for (int i = 0; i < 2; ++i) {
      inputs.layouts[i] = {};
      inputs.sourceTextureIds[i] = images[i];
      ++inputs.sourceTextureRevisions[i];
      glActiveTexture(GL_TEXTURE0 + i);
      glBindTexture(GL_TEXTURE_3D, images[i]);
      glTexImage3D(
        GL_TEXTURE_3D,
        0,
        GL_R32F,
        inputs.fixedDimensions.x,
        inputs.fixedDimensions.y,
        inputs.fixedDimensions.z,
        0,
        GL_RED,
        GL_FLOAT,
        i == 0 ? fixedVolume.data() : movingVolume.data());
    }
    inputs.texture_T_world[1] = glm::mat4{1.0f};
    for (int pass = 0; pass < 3; ++pass) {
      inputs.deformations[0].enabled = pass == 1; // General path, then return to the cached path.
      glFinish();
      const auto start = std::chrono::steady_clock::now();
      int frames = 0;
      bool needsRefinement = false;
      do {
        needsRefinement = renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport);
        glFinish(); // Deterministic completion in the test; the application never waits on these queries.
        ++frames;
        REQUIRE(frames < 5000);
      } while (needsRefinement);
      CHECK(renderer.isComplete());
      CHECK_FALSE(renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport));
      const double elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
      CHECK(readCounts() == expectedCounts);
      CHECK(glGetError() == GL_NO_ERROR);
      if (benchmark) {
        std::cout << "Histogram "
                  << (pass == 0   ? "cold cache"
                      : pass == 1 ? "general shader"
                                  : "warm cache")
                  << ": " << elapsed << " ms, " << frames << " submissions, " << total << " voxels\n";
      }
    }

    // Simulate continuous dragging without CPU/GPU synchronization between frames. In particular,
    // obsolete transforms and bin-count changes must not leave mixed or permanently incomplete counts.
    for (const bool generalPath : {false, true}) {
      inputs.deformations[0].enabled = generalPath;
      const auto dragStart = std::chrono::steady_clock::now();
      for (int frame = 0; frame < 64; ++frame) {
        inputs.texture_T_world[1][3].x = static_cast<float>(frame % 9) / 16.0f;
        inputs.bins = frame % 2 == 0 ? 8 : 4;
        renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport);
        glFlush(); // Submit work, but do not wait for it as glFinish/readback would.
      }
      inputs.texture_T_world[1][3].x = 0.5f;
      inputs.bins = 8;
      const auto settleStart = std::chrono::steady_clock::now();
      bool needsRefinement = false;
      do {
        needsRefinement = renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport);
        glFlush();
        // No input events: the renderer's redraw request alone drives the next frame at the idle refinement rate.
        if (needsRefinement) {
          std::this_thread::sleep_for(std::chrono::milliseconds{16});
        }
        REQUIRE(std::chrono::steady_clock::now() - settleStart < std::chrono::seconds{10});
      } while (needsRefinement);
      CHECK(renderer.isComplete());

      std::vector<float> expectedTranslated(
        static_cast<std::size_t>(inputs.bins) * static_cast<std::size_t>(inputs.bins),
        0.0f);
      const std::size_t shift = inputs.fixedDimensions.x / 2;
      for (std::size_t i = 0; i < total; ++i) {
        if (i % inputs.fixedDimensions.x >= shift) {
          continue;
        }
        const int x = static_cast<int>(fixedVolume[i] * inputs.bins);
        const int y = static_cast<int>(movingVolume[i + shift] * inputs.bins);
        ++expectedTranslated[y * inputs.bins + x];
      }
      CHECK(readCounts() == expectedTranslated);
      CHECK(glGetError() == GL_NO_ERROR);
      if (benchmark) {
        const auto end = std::chrono::steady_clock::now();
        std::cout << "Histogram dragging (" << (generalPath ? "general" : "cached")
                  << "): " << std::chrono::duration<double, std::milli>(settleStart - dragStart).count()
                  << " ms for 64 updates, " << std::chrono::duration<double, std::milli>(end - settleStart).count()
                  << " ms to settle\n";
      }
    }

    // Non-renderable requests must not cause perpetual redraws, even when their inputs have changed.
    inputs.texture_T_world[1][3].x = 0.0f;
    CHECK_FALSE(renderer.render(inputs, {5.0f, 4.0f, 0.0f}, joint_histogram::Navigation{}, viewport));
    inputs.fixedDimensions = glm::uvec3{0u};
    CHECK_FALSE(renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport));

    inputs.fixedDimensions = glm::uvec3{1u};
    inputs.textureComponents[0] = 4;
    CHECK_FALSE(renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport));
    inputs.textureComponents[0] = 0;
    inputs.texture_T_world[0][0][0] = std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport));
    inputs.texture_T_world[0] = glm::mat4{1.0f};
    inputs.sourceTextureIds[0] = 0u;
    CHECK_FALSE(renderer.render(inputs, {5.0f, 4.0f, 16.0f}, joint_histogram::Navigation{}, viewport));

    GLTexture trackedTexture(tex::Target::Texture2D);
    trackedTexture.generate();
    trackedTexture.setSize({1u, 1u, 1u});
    CHECK(trackedTexture.contentRevision() == 0u);
    float trackedValue = 0.5f;
    trackedTexture.setData(
      0,
      tex::SizedInternalFormat::R32F,
      tex::BufferPixelFormat::Red,
      tex::BufferPixelDataType::Float32,
      &trackedValue);
    CHECK(trackedTexture.contentRevision() == 1u);
    trackedTexture.setSubData(
      0,
      {0u, 0u, 0u},
      {1u, 1u, 1u},
      tex::BufferPixelFormat::Red,
      tex::BufferPixelDataType::Float32,
      &trackedValue);
    CHECK(trackedTexture.contentRevision() == 2u);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &outputFramebuffer);
    glDeleteTextures(1, &output);
    glDeleteTextures(1, &colorMapTexture);
    glDeleteTextures(3, images);
  }
  glfwMakeContextCurrent(nullptr);
  glfwDestroyWindow(window);
  glfwTerminate();
}
