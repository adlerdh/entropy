#pragma once

#include "common/Types.h"
#include "logic/interaction/JointHistogramInteraction.h"
#include "rendering/RenderSettings.h"
#include "rendering/TextureLayout.h"
#include "rendering/gl/GLFrameBufferObject.h"
#include "rendering/gl/GLShaderProgram.h"
#include "rendering/gl/GLTexture.h"
#include "rendering/gl/GLVertexArrayObject.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

struct NVGcontext;
class Viewport;

namespace rendering
{

class JointHistogramRenderer final
{
public:
  struct Deformation
  {
    glm::mat4 texture_T_world{1.0f};
    float native_T_texture = 1.0f;
    float strength = 0.0f;
    bool enabled = false;
    bool interleaved = false;
  };

  struct Inputs
  {
    glm::uvec3 fixedDimensions{0u};
    glm::mat4 world_T_fixedTexture{1.0f};
    std::array<glm::mat4, 2> texture_T_world{glm::mat4{1.0f}, glm::mat4{1.0f}};
    std::array<glm::vec2, 2> normalized_T_texture{glm::vec2{1.0f, 0.0f}, glm::vec2{1.0f, 0.0f}};
    std::array<PlanarTextureLayout, 2> layouts{};
    std::array<Deformation, 2> deformations{};
    std::array<GLuint, 8> sourceTextureIds{};
    std::array<std::uint64_t, 8> sourceTextureRevisions{};
    std::array<int, 2> textureComponents{};
    std::array<bool, 2> linearInterpolation{true, true};
    RenderSettings::MetricParams metric{};
    bool logarithmicScale = true;
    int bins = 512;
  };

  JointHistogramRenderer();
  ~JointHistogramRenderer();
  JointHistogramRenderer(const JointHistogramRenderer&) = delete;
  JointHistogramRenderer& operator=(const JointHistogramRenderer&) = delete;

  /** Counts occupy the lower-left bins-by-bins square; the remaining rows are scratch storage. */
  const GLTexture& countsTexture() const
  {
    return m_counts;
  }
  bool isComplete() const;

  /**
   * Image textures must be bound to units 0 and 1; colormap to 2; deformation textures to 3–8.
   * Returns true when another frame is needed to finish accumulation, including while the GPU is busy.
   * Invalid/non-visible plots return false so they do not keep the event loop awake.
   */
  bool render(
    const Inputs& inputs,
    const joint_histogram::Plot& plot,
    const joint_histogram::Navigation& navigation,
    const Viewport& windowViewport);

  /** Draw axes in NanoVG's top-left coordinate system after the image pass. */
  static void drawAxes(
    NVGcontext* nvg,
    const joint_histogram::Plot& plot,
    const Viewport& windowViewport,
    const std::array<std::string, 2>& imageNames,
    const std::array<std::pair<double, double>, 2>& ranges,
    const joint_histogram::Navigation& navigation,
    int majorTicks,
    int minorTicks);

private:
  void initialize();
  void ensureTexture(int bins);
  bool prepareFixedSamples(const Inputs& inputs, std::uint64_t voxelCount);
  void resolveBackground(int bins);

  // XY positions are reused for every slice; fixed intensities are cached independently of registration transforms.
  GLuint m_positions = 0;
  GLuint m_fixedValues = 0;
  GLuint m_fixedValuesTexture = 0;
  std::optional<Inputs> m_fixedSampleInputs;
  std::optional<Inputs> m_failedFixedSampleInputs;
  std::array<std::unique_ptr<GLShaderProgram>, 2> m_fixedSamplePrograms;
  std::array<std::unique_ptr<GLShaderProgram>, 2> m_fastScatterPrograms;
  std::unique_ptr<GLShaderProgram> m_reduceProgram;
  std::array<GLuint, 2> m_reductionTextures{};
  GLuint m_reductionFramebuffer = 0;

  struct Timing
  {
    GLuint query = 0;
    std::uint64_t voxels = 0;
    bool pending = false;
  };
  // Never queue another accumulation behind unfinished work for an obsolete transform.
  std::array<Timing, 1> m_timings{};
  double m_nanosecondsPerVoxel = 4.0;

  std::array<std::unique_ptr<GLShaderProgram>, 4> m_scatterPrograms;
  std::unique_ptr<GLShaderProgram> m_displayProgram;
  GLFrameBufferObject m_framebuffer;
  GLTexture m_counts;
  GLVertexArrayObject m_vao;
  int m_allocatedBins = 0;
  std::optional<Inputs> m_cachedInputs;
  bool m_inputsPending = false;
  std::uint64_t m_batchVoxelLimit = 0;
  std::uint64_t m_completedBatches = 0;
  std::uint64_t m_processedVoxels = 0;
};

} // namespace rendering
