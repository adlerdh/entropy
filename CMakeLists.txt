cmake_minimum_required(VERSION 3.24.0 FATAL_ERROR)

if(POLICY CMP0175)
  cmake_policy(SET CMP0175 OLD)
endif()

set(APP_NAME "Entropy")
set(APP_DESCRIPTION "Tool for interactively visualizing, comparing, segmenting, and annotating 3D medical images")
set(ORG_NAME "Penn Image Computing and Science Lab (PICSL), Department of Radiology, University of Pennsylvania")
set(COPYRIGHT_LINE "Copyright Daniel H. Adler and PICSL")
set(LICENSE_LINE "Apache License, Version 2.0")

set(VERSION_MAJOR 0)
set(VERSION_MINOR 8)
set(VERSION_FEATURE 0)
set(VERSION_PATCH 0)
set(VERSION_FULL "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_FEATURE}.${VERSION_PATCH}")

project(Entropy VERSION ${VERSION_FULL} LANGUAGES C CXX)

set(entropy_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(entropy_RES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources)
set(entropy_EXT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)

# CMake will generate a file compile_commands.json under the build directory.
# VSCode is able to parse the file and find the include path based on its content.
add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;MinSizeRel;RelWithDebInfo")
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fstack-protector-all -g")

message(STATUS "CMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}")

# Prevent compilation in-source
if(${CMAKE_BINARY_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
  message(FATAL_ERROR "Source and build directories are the same.")
endif()

# Set a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE)
  set(DEFAULT_BUILD_TYPE "RelWithDebInfo")
  message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
  set(CMAKE_BUILD_TYPE ${DEFAULT_BUILD_TYPE} CACHE STRING "Choose the type of build" FORCE)

  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()


option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

if(${BUILD_SHARED_LIBS})
  set(BUILD_STATIC_LIBS OFF)
else()
  set(BUILD_STATIC_LIBS ON)
endif()

message(STATUS "BUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}")
message(STATUS "BUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(UNIX)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
else()
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()


# External library versions:
set(argparse_VERSION "3.2")
set(boost_VERSION "1.87.0")
set(cmakerc_VERSION "2.0.1")
set(ghc_filesystem_VERSION "1.5.14")
set(glfw_VERSION "3.4")
set(glm_VERSION "1.0.1")
set(iconfont_VERSION "ef464d2")
set(imgui_VERSION "1.91.8")
set(implot_VERSION "0.16")
set(itk_VERSION "5.4.2")
set(nanovg_VERSION "f93799c")
set(nlohmann_json_VERSION "3.11.3")
set(spdlog_VERSION "1.15.1")
set(stduuid_VERSION "1.2.3")
set(tinyfsm_VERSION "1.15.1")

# External library directores:
set(EXTERNAL_DIR "${CMAKE_BINARY_DIR}/external")
set(argparse_PREFIX "${EXTERNAL_DIR}/argparse-${argparse_VERSION}")
set(boost_PREFIX "${EXTERNAL_DIR}/boost-${boost_VERSION}")
set(cmakerc_PREFIX "${EXTERNAL_DIR}/cmakerc-${cmakerc_VERSION}")
set(ghc_filesystem_PREFIX "${EXTERNAL_DIR}/ghc_filesystem-${ghc_filesystem_VERSION}")
set(glfw_PREFIX "${EXTERNAL_DIR}/glfw-${glfw_VERSION}")
set(glm_PREFIX "${EXTERNAL_DIR}/glm-${glm_VERSION}")
set(iconfont_PREFIX "${EXTERNAL_DIR}/iconfont-${iconfont_VERSION}")
set(imgui_PREFIX "${EXTERNAL_DIR}/imgui-${imgui_VERSION}")
set(implot_PREFIX "${EXTERNAL_DIR}/implot-${implot_VERSION}")
set(itk_PREFIX "${EXTERNAL_DIR}/itk-${itk_VERSION}")
set(nanovg_PREFIX "${EXTERNAL_DIR}/nanovg")
set(nlohmann_json_PREFIX "${EXTERNAL_DIR}/nlohmann_json-${nlohmann_json_VERSION}")
set(spdlog_PREFIX "${EXTERNAL_DIR}/spdlog-${spdlog_VERSION}")
set(stduuid_PREFIX "${EXTERNAL_DIR}/stduuid-${stduuid_VERSION}")
set(tinyfsm_PREFIX "${EXTERNAL_DIR}/tinyfsm-${tinyfsm_VERSION}")

option(Entropy_SUPERBUILD "Run the SuperBuild" ON)

if(Entropy_SUPERBUILD)
  include(SuperBuild)
  return()
endif()


# argparse: Header-only library for argument parsing using modern C++
find_package(argparse ${argparse_VERSION} REQUIRED
    HINTS "${argparse_PREFIX}/install")

if(argparse_FOUND)
  message(STATUS "Using argparse in ${argparse_DIR}")
  set(argparse_INCLUDE_DIR "${argparse_PREFIX}/install/include"
      CACHE STRING "argparse include directory" FORCE)
else()
  message(FATAL_ERROR "argparse not found")
endif()

# Boost C++ Libraries
set(Boost_INCLUDE_DIR "${boost_PREFIX}/src" CACHE STRING "Boost include directory" FORCE)

if(IS_DIRECTORY "${Boost_INCLUDE_DIR}")
  message(STATUS "Using Boost headers in ${Boost_INCLUDE_DIR}")
else()
  message(FATAL_ERROR "Boost headers not found: Please set Boost_INCLUDE_DIR")
endif()

# CMakeRC: Standalone CMake-based C++ resource compiler
include(${cmakerc_PREFIX}/src/CMakeRC.cmake)

# ghc::filesystem: Header-only, single-file std::filesystem compatible helper library,
# based on the C++17 and C++20 specs, but implemented for C++11, C++14, C++17 or C++20
# (Needed because std::filesystem is not fully implemented on macOS prior to 10.15)
find_package(ghc_filesystem ${ghc_filesystem_VERSION} REQUIRED
    HINTS "${ghc_filesystem_PREFIX}/install")

if(ghc_filesystem_FOUND)
  message(STATUS "Using ghc::filesystem in ${ghc_filesystem_DIR}")
else()
  message(FATAL_ERROR "ghc::filesystem not found")
endif()

# GLFW: Open Source, multi-platform library for OpenGL, OpenGL ES and Vulkan application development
find_package(glfw3 ${glfw_VERSION} REQUIRED HINTS "${glfw_PREFIX}/install")

if(glfw3_FOUND)
  message(STATUS "Using GLFW in ${glfw3_DIR}")
  set(glfw_INCLUDE_DIR "${glfw_PREFIX}/install/include"
      CACHE STRING "glfw include directory" FORCE)
else()
  message(FATAL_ERROR "GLFW not found")
endif()

# OpenGL Mathematics (GLM): C++ mathematics library for graphics software based
# on the OpenGL Shading Language (GLSL) specifications
find_package(glm ${glm_VERSION} REQUIRED CONFIG
    HINTS "${glm_PREFIX}/install")

if(glm_FOUND)
  message(STATUS "Using GLM in ${glm_DIR}")
else()
  message(FATAL_ERROR "GLM not found")
endif()

# IconFontCppHeaders: Headers for Font Awesome and Fork Awesome icons
set(iconfont_INCLUDE_DIR "${iconfont_PREFIX}/src"
    CACHE STRING "IconFontCppHeaders include directory" FORCE)

# Insight Toolkit (ITK)
find_package(ITK ${itk_VERSION} REQUIRED HINTS "${itk_PREFIX}/build")

if(ITK_FOUND)
  message(STATUS "Found ITK in ${ITK_DIR}")
  include(${ITK_USE_FILE})
else()
  message(FATAL_ERROR "ITK not found: Please set ITK_DIR")
endif()

# NanoVG: A small antialiased vector graphics rendering library for OpenGL
set(nanovg_SRC_DIR "${nanovg_PREFIX}/src/src")
add_library(nanovg "${nanovg_SRC_DIR}/nanovg.c")
target_include_directories(nanovg PRIVATE "${nanovg_SRC_DIR}")

# Niels Lohmann's JSON for Modern C++
find_package(nlohmann_json ${nlohmann_json_VERSION} REQUIRED
    HINTS "${nlohmann_json_PREFIX}/install")

if(nlohmann_json_FOUND)
  message(STATUS "Using nlohmann_json in ${nlohmann_json_DIR}")
else()
  message(FATAL_ERROR "nlohmann_json not found")
endif()

# spdlog: Fast C++ logging library
find_package(spdlog ${spdlog_VERSION} REQUIRED HINTS "${spdlog_PREFIX}/install")

if(spdlog_FOUND)
  message(STATUS "Using spdlog in ${spdlog_DIR}")
else()
  message(FATAL_ERROR "spdlog not found: Please set spdlog_DIR")
endif()

# stduuid: A C++17 cross-platform single-header library implementation for universally
# unique identifiers
find_package(stduuid REQUIRED HINTS "${stduuid_PREFIX}/install")

if(stduuid_FOUND)
  message(STATUS "Using stduuid in ${stduuid_DIR}")
else()
  message(FATAL_ERROR "stduuid not found")
endif()

# TinyFSM: A simple, header-only C++ finite state machine library
set(tinyfsm_INCLUDE_DIR "${tinyfsm_PREFIX}/src/include"
    CACHE STRING "TinyFSM include directory" FORCE)


#--------------------------------------------------------------------------------
# GLAD library: Multi-Language GL/GLES/EGL/GLX/WGL Loader-Generator based on the official specs
#--------------------------------------------------------------------------------
# Entropy includes the loaders for OpenGL 3.3, 4.1, and 4.6 Core profiles for C,
# though only the loaders for version 3.3 are used in Entropy in order to assure
# compatability across graphics hardware and platforms.

# This source code was generated by cloning the Git repository
# https://github.com/Dav1dde/glad.git and executing
# python3 -m glad --profile core --out-path glad --api gl=3.3 --generator c --spec gl
# (It can also be generated from the webservice at https://glad.dav1d.de/)

set(GLAD_GL_VERSION "3.3" CACHE STRING "OpenGL Core profile version loaded by GLAD")
set(GL_VERSION_VALUES "3.3;4.1;4.6")
set(GLAD_GL_DEBUG false CACHE BOOL "Use OpenGL debugging in GLAD")

set_property(CACHE GLAD_GL_VERSION PROPERTY STRINGS ${GL_VERSION_VALUES})

string(REPLACE "." ";" GL_VERSION_LIST ${GLAD_GL_VERSION})
list(GET GL_VERSION_LIST 0 GLAD_GL_VERSION_MAJOR)
list(GET GL_VERSION_LIST 1 GLAD_GL_VERSION_MINOR)

set(glad_BASE glad_gl_${GLAD_GL_VERSION}_core)

if(GLAD_GL_DEBUG)
  set(glad_BASE ${glad_BASE}_debug)
endif()

set(glad_DIR ${entropy_EXT_DIR}/glad/${glad_BASE})
set(glad_INCLUDE_DIR ${glad_DIR}/include)

add_library(glad ${glad_DIR}/src/glad.c)
target_include_directories(glad PRIVATE ${glad_INCLUDE_DIR})

#--------------------------------------------------------------------------------
# "Dear ImGui" library: Bloat-free graphical user interface library for C++.
# Outputs optimized vertex buffers to render anytime in 3D-pipeline enabled application.
# Fast, portable, renderer agnostic and self-contained (no external dependencies).
#--------------------------------------------------------------------------------
set(imgui_SRC_DIR ${imgui_PREFIX}/src)

list(APPEND imgui_SOURCES
  ${imgui_SRC_DIR}/imgui/imgui.cpp
  ${imgui_SRC_DIR}/imgui/imgui_demo.cpp
  ${imgui_SRC_DIR}/imgui/imgui_draw.cpp
  ${imgui_SRC_DIR}/imgui/imgui_tables.cpp
  ${imgui_SRC_DIR}/imgui/imgui_widgets.cpp
  ${imgui_SRC_DIR}/imgui/misc/cpp/imgui_stdlib.cpp
  ${imgui_SRC_DIR}/imgui/backends/imgui_impl_glfw.cpp # GLFW bindings for ImGui
  ${imgui_SRC_DIR}/imgui/backends/imgui_impl_opengl3.cpp # OpenGL 3 bindings for ImGui
)

add_library(imgui ${imgui_SOURCES})

target_compile_definitions(imgui PRIVATE IMGUI_USER_CONFIG="ui/entropy_imgui_config.h")

target_include_directories(imgui PRIVATE
  ${imgui_SRC_DIR}/imgui
  ${entropy_SRC_DIR} # needed for IMGUI_USER_CONFIG file
  ${glfw_INCLUDE_DIR}
)

# used in IMGUI_USER_CONFIG
target_link_libraries(imgui PRIVATE spdlog::spdlog)

target_compile_options(imgui PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
  -Werror -Wall -Wextra -Wpointer-arith -Winit-self -Wunreachable-code
  -Wshadow -pedantic-errors -g -ftrapv
  -Wno-error=deprecated-copy
  -Wno-error=empty-body
  -Wno-empty-body # suppressed due to empty IM_ASSERT
  >
  $<$<CXX_COMPILER_ID:MSVC>:
  /W4
  >
)

set_target_properties(imgui PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

#--------------------------------------------------------------------------------
# ImPlot: Immediate mode, GPU accelerated plotting library for Dear ImGui
#--------------------------------------------------------------------------------
set(implot_SRC_DIR ${implot_PREFIX}/src)

list(APPEND IMPLOT_SOURCES
  ${implot_SRC_DIR}/implot/implot_demo.cpp
  ${implot_SRC_DIR}/implot/implot_items.cpp
  ${implot_SRC_DIR}/implot/implot.cpp
)

add_library(implot ${IMPLOT_SOURCES})

target_include_directories(implot PRIVATE
  ${implot_SRC_DIR}/implot
  ${imgui_SRC_DIR}/imgui
  ${entropy_SRC_DIR} # needed for IMGUI_USER_CONFIG file
)

# used in IMGUI_USER_CONFIG
target_link_libraries(implot PRIVATE spdlog::spdlog)

target_compile_options(implot PRIVATE
   $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
    -Wall -Wextra -Wpointer-arith -Winit-self -Wunreachable-code -Wshadow -g -ftrapv>
   $<$<CXX_COMPILER_ID:MSVC>:
    /W4>
)

set_target_properties(implot PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

#--------------------------------------------------------------------------------
# GridCut (NOT included as Git submodule):
# Header-only C++ library of fast max-flow/min-cut solvers optimized for grid graphs
#--------------------------------------------------------------------------------
set(gridcut_INCLUDE_DIRS
  ${entropy_EXT_DIR}/gridcut/include
  ${entropy_EXT_DIR}/gridcut/examples/include
)

# Generate build stamp header after GLAD_GL_VERSION_MAJOR and GLAD_GL_VERSION_MINOR are defined
include(BuildStamp)
BuildStamp()

#--------------------------------------------------------------------------------
# Define Entropy executable target
#--------------------------------------------------------------------------------
set(entropy_SOURCES
  ${entropy_SRC_DIR}/main.cpp
  ${entropy_SRC_DIR}/EntropyApp.cpp

  ${entropy_SRC_DIR}/common/CoordinateFrame.cpp
  ${entropy_SRC_DIR}/common/DataHelper.cpp
  ${entropy_SRC_DIR}/common/DirectionMaps.cpp
  ${entropy_SRC_DIR}/common/InputParams.cpp
  ${entropy_SRC_DIR}/common/InputParser.cpp
  ${entropy_SRC_DIR}/common/MathFuncs.cpp
  ${entropy_SRC_DIR}/common/ParcellationLabelTable.cpp
  ${entropy_SRC_DIR}/common/Types.cpp
  ${entropy_SRC_DIR}/common/UuidUtility.cpp
  ${entropy_SRC_DIR}/common/Viewport.cpp

  ${entropy_SRC_DIR}/image/Image.cpp
  ${entropy_SRC_DIR}/image/ImageColorMap.cpp
  ${entropy_SRC_DIR}/image/ImageHeader.cpp
  ${entropy_SRC_DIR}/image/ImageIoInfo.cpp
  ${entropy_SRC_DIR}/image/ImageSettings.cpp
  ${entropy_SRC_DIR}/image/ImageTransformations.cpp
  ${entropy_SRC_DIR}/image/ImageUtility.cpp
  ${entropy_SRC_DIR}/image/SegUtil.cpp
  ${entropy_SRC_DIR}/image/SurfaceUtility.cpp

  ${entropy_SRC_DIR}/logic/app/CallbackHandler.cpp
  ${entropy_SRC_DIR}/logic/app/Data.cpp
  ${entropy_SRC_DIR}/logic/app/Logging.cpp
  ${entropy_SRC_DIR}/logic/app/Settings.cpp
  ${entropy_SRC_DIR}/logic/app/State.cpp

  ${entropy_SRC_DIR}/logic/annotation/Annotation.cpp
  ${entropy_SRC_DIR}/logic/annotation/BezierHelper.cpp
  ${entropy_SRC_DIR}/logic/annotation/LandmarkGroup.cpp
  ${entropy_SRC_DIR}/logic/annotation/SerializeAnnot.cpp

  ${entropy_SRC_DIR}/logic/camera/Camera.cpp
  ${entropy_SRC_DIR}/logic/camera/CameraHelpers.cpp
  ${entropy_SRC_DIR}/logic/camera/CameraTypes.cpp
  ${entropy_SRC_DIR}/logic/camera/MathUtility.cpp
  ${entropy_SRC_DIR}/logic/camera/OrthogonalProjection.cpp
  ${entropy_SRC_DIR}/logic/camera/PerspectiveProjection.cpp
  ${entropy_SRC_DIR}/logic/camera/Projection.cpp

  ${entropy_SRC_DIR}/logic/interaction/ViewHit.cpp
  ${entropy_SRC_DIR}/logic/interaction/events/ButtonState.cpp

  ${entropy_SRC_DIR}/logic/segmentation/GraphCuts.cpp
  ${entropy_SRC_DIR}/logic/segmentation/Poisson.cpp
  ${entropy_SRC_DIR}/logic/segmentation/SegHelpers.cpp

  ${entropy_SRC_DIR}/logic/serialization/ProjectSerialization.cpp

  ${entropy_SRC_DIR}/logic/states/AnnotationStateHelpers.cpp
  ${entropy_SRC_DIR}/logic/states/AnnotationStateMachine.cpp
  ${entropy_SRC_DIR}/logic/states/AnnotationStates.cpp

  # We were testing IPC with ITK-SNAP. This functionality is not currently hooked up to Entropy.
  # ${entropy_SRC_DIR}/logic/ipc/IPCHandler.cxx

  ${entropy_SRC_DIR}/rendering/ImageDrawing.cpp
  ${entropy_SRC_DIR}/rendering/Rendering.cpp
  ${entropy_SRC_DIR}/rendering/RenderData.cpp
  ${entropy_SRC_DIR}/rendering/TextureSetup.cpp
  ${entropy_SRC_DIR}/rendering/VectorDrawing.cpp

  ${entropy_SRC_DIR}/rendering/utility/containers/VertexAttributeInfo.cpp
  ${entropy_SRC_DIR}/rendering/utility/containers/VertexIndicesInfo.cpp
  ${entropy_SRC_DIR}/rendering/utility/containers/Uniforms.cpp
  ${entropy_SRC_DIR}/rendering/utility/gl/GLBufferObject.cpp
  ${entropy_SRC_DIR}/rendering/utility/gl/GLBufferTexture.cpp
  ${entropy_SRC_DIR}/rendering/utility/gl/GLErrorChecker.cpp
  ${entropy_SRC_DIR}/rendering/utility/gl/GLFrameBufferObject.cpp
  ${entropy_SRC_DIR}/rendering/utility/gl/GLShader.cpp
  ${entropy_SRC_DIR}/rendering/utility/gl/GLShaderProgram.cpp
  ${entropy_SRC_DIR}/rendering/utility/gl/GLTexture.cpp
  ${entropy_SRC_DIR}/rendering/utility/gl/GLVersionChecker.cpp
  ${entropy_SRC_DIR}/rendering/utility/gl/GLVertexArrayObject.cpp
  ${entropy_SRC_DIR}/rendering/utility/math/SliceIntersector.cpp

  ${entropy_SRC_DIR}/ui/GuiData.cpp
  ${entropy_SRC_DIR}/ui/Headers.cpp
  ${entropy_SRC_DIR}/ui/Helpers.cpp
  ${entropy_SRC_DIR}/ui/IsosurfaceHeader.cpp
  ${entropy_SRC_DIR}/ui/ImGuiCustomControls.cpp
  ${entropy_SRC_DIR}/ui/ImGuiWrapper.cpp
  ${entropy_SRC_DIR}/ui/MainMenuBar.cpp
  ${entropy_SRC_DIR}/ui/Popups.cpp
  ${entropy_SRC_DIR}/ui/Style.cpp
  ${entropy_SRC_DIR}/ui/Toolbars.cpp
  ${entropy_SRC_DIR}/ui/Widgets.cpp
  ${entropy_SRC_DIR}/ui/Windows.cpp

  ${entropy_SRC_DIR}/ui/imgui/imGuIZMO.quat/imGuIZMOquat.cpp
  ${entropy_SRC_DIR}/ui/imgui/imgui-knobs/imgui-knobs.cpp

  ${entropy_SRC_DIR}/windowing/ControlFrame.cpp
  ${entropy_SRC_DIR}/windowing/GlfwCallbacks.cpp
  ${entropy_SRC_DIR}/windowing/GlfwWrapper.cpp
  ${entropy_SRC_DIR}/windowing/Layout.cpp
  ${entropy_SRC_DIR}/windowing/View.cpp
  ${entropy_SRC_DIR}/windowing/ViewTypes.cpp
  ${entropy_SRC_DIR}/windowing/WindowData.cpp
)

set(entropy_SHADERS
  ${entropy_SRC_DIR}/rendering/shaders/Correlation.fs
  ${entropy_SRC_DIR}/rendering/shaders/Difference.fs
  ${entropy_SRC_DIR}/rendering/shaders/Edge.fs
  ${entropy_SRC_DIR}/rendering/shaders/Image.fs
  ${entropy_SRC_DIR}/rendering/shaders/ImageRgba.fs
  ${entropy_SRC_DIR}/rendering/shaders/Image.vs
  ${entropy_SRC_DIR}/rendering/shaders/Metric.vs
  ${entropy_SRC_DIR}/rendering/shaders/Overlay.fs
  ${entropy_SRC_DIR}/rendering/shaders/RaycastIsoSurface.fs
  ${entropy_SRC_DIR}/rendering/shaders/RaycastIsoSurface.vs
  ${entropy_SRC_DIR}/rendering/shaders/Simple.fs
  ${entropy_SRC_DIR}/rendering/shaders/Simple.vs
  ${entropy_SRC_DIR}/rendering/shaders/Xray.fs
)

set(entropy_SHADERS_OLD
  ${entropy_SRC_DIR}/rendering_old/shaders/BasicMesh.vert
  ${entropy_SRC_DIR}/rendering_old/shaders/BasicMesh.frag
  ${entropy_SRC_DIR}/rendering_old/shaders/BasicMeshPeel.frag
  ${entropy_SRC_DIR}/rendering_old/shaders/Debug.vert
  ${entropy_SRC_DIR}/rendering_old/shaders/Debug.frag
  ${entropy_SRC_DIR}/rendering_old/shaders/fbo_screen.vert
  ${entropy_SRC_DIR}/rendering_old/shaders/fbo_screen.frag
  ${entropy_SRC_DIR}/rendering_old/shaders/Flat.vert
  ${entropy_SRC_DIR}/rendering_old/shaders/Flat.frag
  ${entropy_SRC_DIR}/rendering_old/shaders/FlatPeel.frag
  ${entropy_SRC_DIR}/rendering_old/shaders/Mesh.vert
  ${entropy_SRC_DIR}/rendering_old/shaders/Mesh.frag
  ${entropy_SRC_DIR}/rendering_old/shaders/MeshPeel.frag
  ${entropy_SRC_DIR}/rendering_old/shaders/Polygonizer.vert
  ${entropy_SRC_DIR}/rendering_old/shaders/Polygonizer.geom
  ${entropy_SRC_DIR}/rendering_old/shaders/Simple.vert
  ${entropy_SRC_DIR}/rendering_old/shaders/Simple.frag
  ${entropy_SRC_DIR}/rendering_old/shaders/ddp/blend.frag
  ${entropy_SRC_DIR}/rendering_old/shaders/ddp/blend.vert
  ${entropy_SRC_DIR}/rendering_old/shaders/ddp/final.frag
  ${entropy_SRC_DIR}/rendering_old/shaders/ddp/InitializeDepths.frag
  ${entropy_SRC_DIR}/rendering_old/shaders/ddp/init.vert
)

file(GLOB entropy_COLORMAPS
  ${entropy_RES_DIR}/colormaps/ncl/*.csv
  ${entropy_RES_DIR}/colormaps/matplotlib/*.csv
  ${entropy_RES_DIR}/colormaps/peter_kovesi/*.csv
)

set(entropy_FONTS
  ${entropy_RES_DIR}/fonts/Cousine/Cousine-Regular.ttf
  ${entropy_RES_DIR}/fonts/ForkAwesome/forkawesome-webfont.ttf
  ${entropy_RES_DIR}/fonts/HelveticaNeue/HelveticaNeue-Light.ttf
  ${entropy_RES_DIR}/fonts/Inter/Inter-Light.ttf
  ${entropy_RES_DIR}/fonts/Roboto/Roboto-Light.ttf
  ${entropy_RES_DIR}/fonts/Roboto/Roboto-Regular.ttf
  ${entropy_RES_DIR}/fonts/Satoshi/Satoshi-Light.ttf
  ${entropy_RES_DIR}/fonts/SFMono/SFMono-Regular.ttf
  ${entropy_RES_DIR}/fonts/SFPro/sf-pro-text-regular.ttf
  ${entropy_RES_DIR}/fonts/SpaceGrotesk/SpaceGrotesk-Light.ttf
  ${entropy_RES_DIR}/fonts/Supreme/Supreme-Light.ttf
)


# if(UNIX AND NOT APPLE)
#   find_package(OpenGL REQUIRED)
#  add_definitions(${OPENGL_DEFINITIONS})
#  find_package(X11 REQUIRED)

  # Note: order is important for setting the libs
  # use pkg-config --libs $(pkg-config --print-requires --print-requires-private glfw3) in a terminal to confirm
  # set(LIBS ${GLFW3_LIBRARY} X11 Xrandr Xinerama Xi Xxf86vm Xcursor GL dl pthread ${ASSIMP_LIBRARY} freetype)
  # set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -ldl")

if(APPLE)
  include_directories(/System/Library/Frameworks)

  find_library(COCOA_LIBRARY Cocoa)
  find_library(OpenGL_LIBRARY OpenGL)
  find_library(IOKit_LIBRARY IOKit)
  find_library(CoreVideo_LIBRARY CoreVideo)

  mark_as_advanced(COCOA_LIBRARY OpenGL_LIBRARY)

  # set(APPLE_LIBS ${COCOA_LIBRARY} ${IOKit_LIBRARY} ${OpenGL_LIBRARY} ${CoreVideo_LIBRARY})
  # set(APPLE_LIBS ${APPLE_LIBS} ${GLFW3_LIBRARY} ${ASSIMP_LIBRARY})
  # set(LIBS ${LIBS} ${APPLE_LIBS})
endif()

set(entropy_EXE Entropy)

# Embed the resources using CMakeRC
cmrc_add_resource_library(${entropy_EXE}-colormaps ALIAS ${entropy_EXE}::colormaps ${entropy_COLORMAPS} NAMESPACE colormaps)
cmrc_add_resource_library(${entropy_EXE}-fonts ALIAS ${entropy_EXE}::fonts ${entropy_FONTS} NAMESPACE fonts)
cmrc_add_resource_library(${entropy_EXE}-shaders ALIAS ${entropy_EXE}::shaders ${entropy_SHADERS} NAMESPACE shaders)

add_executable(${entropy_EXE})

target_sources(${entropy_EXE} PRIVATE
  ${entropy_SOURCES}
  ${entropy_HEADERS}
)

target_link_libraries(${entropy_EXE} PRIVATE
  ${ITK_LIBRARIES}
  ghcFilesystem::ghc_filesystem
  glfw
  glad
  glm::glm
  imgui
  implot
  nanovg
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  stduuid
  ${entropy_EXE}::colormaps
  ${entropy_EXE}::fonts
  ${entropy_EXE}::shaders
)

target_include_directories(${entropy_EXE} PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR} # for BuildStamp.h
  ${entropy_SRC_DIR}
  ${entropy_RES_DIR}
  ${glad_INCLUDE_DIR}
  ${iconfont_INCLUDE_DIR}
  ${imgui_SRC_DIR}
  ${imgui_SRC_DIR}/imgui # due to ImPlot
  ${implot_SRC_DIR}
  ${ITK_INCLUDE_DIRS}
  ${tinyfsm_INCLUDE_DIR}
)

# These libraries are in 'system' includes in order to ignore their compiler warnings:
target_include_directories(${entropy_EXE} SYSTEM PRIVATE
  ${argparse_INCLUDE_DIR}
  ${Boost_INCLUDE_DIR}
  ${gridcut_INCLUDE_DIRS}
  ${nanovg_SRC_DIR}
)

target_compile_options(${entropy_EXE} PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
  -Werror
  -Wall -Wextra -Wpointer-arith -Winit-self -Wunreachable-code -Wshadow
  # -Wconversion -Wsign-conversion
  -Wno-error=array-bounds
  -Wno-error=empty-body
  -Wno-error=float-conversion
  -Wno-error=maybe-uninitialized
  -Wno-error=stringop-overflow
  -ftrapvx
  >
  $<$<CXX_COMPILER_ID:AppleClang>:
  -Werror -Wall -Wextra -Wpointer-arith -Winit-self -Wunreachable-code
  -Wno-error=array-bounds
  -Wshadow -ftrapv
  >
  $<$<CXX_COMPILER_ID:MSVC>:
  /W4
  >
)

set_target_properties(${entropy_EXE} PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)
