add_subdirectory(shaders)

add_library(EntropyRendering STATIC)
add_library(Entropy::Rendering ALIAS EntropyRendering)

# Keep the reusable library independent of Entropy's application and domain models. This scan is deliberately separate
# from the explicit source list so newly added implementation and test files are checked as soon as CMake configures.
file(GLOB_RECURSE entropy_RENDERING_BOUNDARY_FILES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
foreach(entropy_RENDERING_BOUNDARY_FILE IN LISTS entropy_RENDERING_BOUNDARY_FILES)
  file(READ "${entropy_RENDERING_BOUNDARY_FILE}" entropy_RENDERING_BOUNDARY_CONTENT)
  if(entropy_RENDERING_BOUNDARY_CONTENT MATCHES "#[ \t]*include[ \t]*[<\"](app|image|logic|ui|windowing)/")
    message(FATAL_ERROR
      "Rendering library boundary violation in ${entropy_RENDERING_BOUNDARY_FILE}: "
      "lib/rendering must not include Entropy application or domain headers")
  endif()
endforeach()
unset(entropy_RENDERING_BOUNDARY_CONTENT)
unset(entropy_RENDERING_BOUNDARY_FILE)
unset(entropy_RENDERING_BOUNDARY_FILES)

target_sources(EntropyRendering PRIVATE
  ascii/AsciiClipboard.cpp
  FramePacer.cpp
  geometry/PixelEdgeGeometry.cpp
  geometry/ScaleBarGeometry.cpp
  helpers/ImageDrawingHelpers.cpp
  helpers/LightboxOffsetLabelFormat.cpp
  helpers/PipelineHelpers.cpp
  helpers/TextureSetupHelpers.cpp
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
  utility/containers/Uniforms.cpp
  utility/containers/VertexAttributeInfo.cpp
  utility/containers/VertexIndicesInfo.cpp
  utility/gl/GLBufferObject.cpp
  utility/gl/GLBufferTexture.cpp
  utility/gl/GLErrorChecker.cpp
  utility/gl/GLFrameBufferObject.cpp
  utility/gl/GLShader.cpp
  utility/gl/GLShaderProgram.cpp
  utility/gl/GLTexture.cpp
  utility/gl/GLVertexArrayObject.cpp
  utility/gl/OpenGLContext.cpp
  utility/gl/OpenGLRenderState.cpp
  utility/gl/OpenGLStateGuard.cpp
  utility/math/SliceIntersector.cpp
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
