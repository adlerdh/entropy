add_subdirectory(shaders)

add_library(EntropyRendering STATIC)
add_library(Entropy::Rendering ALIAS EntropyRendering)

target_sources(EntropyRendering PRIVATE
  ascii/AsciiClipboard.cpp
  FramePacer.cpp
  geometry/PixelEdgeGeometry.cpp
  geometry/ScaleBarGeometry.cpp
  helpers/ImageDrawingHelpers.cpp
  helpers/LightboxOffsetLabelFormat.cpp
  helpers/PipelineHelpers.cpp
  helpers/TextureSetupHelpers.cpp
  ImageShaderCapabilities.cpp
  metrics/LocalLinearResidualMetric.cpp
  metrics/LocalNccMetric.cpp
  RenderDerivedData.cpp
  RenderResources.cpp
  RenderSettings.cpp
  ShaderInfoSetup.cpp
  ShaderPreprocessor.cpp
  ShaderProgramSetup.cpp
  ShaderSourceSetup.cpp
  ShaderTextureDimension.cpp
  ShaderUniformSetup.cpp
  common/ShaderType.cpp
  gl/Uniforms.cpp
  gl/VertexAttributeInfo.cpp
  gl/VertexIndicesInfo.cpp
  gl/GLBufferObject.cpp
  gl/GLBufferTexture.cpp
  gl/GLErrorChecker.cpp
  gl/GLFrameBufferObject.cpp
  gl/GLShader.cpp
  gl/GLShaderProgram.cpp
  gl/GLTexture.cpp
  gl/GLVertexArrayObject.cpp
  gl/OpenGLContext.cpp
  gl/OpenGLRenderState.cpp
  gl/OpenGLStateGuard.cpp
  helpers/SliceIntersector.cpp
  physics/XrayAttenuation.cpp
)

add_subdirectory(mesh)

target_include_directories(EntropyRendering
  PUBLIC
    "${entropy_LIB_DIR}"
    "${glad_INCLUDE_DIR}"
)

target_link_libraries(EntropyRendering
  PUBLIC
    Entropy::Common
    glad
    glm::glm
    stduuid
  PRIVATE
    entropy::shaders
    entropy_warnings
)

entropy_enable_coverage_for_target(EntropyRendering)

set_target_properties(EntropyRendering PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
