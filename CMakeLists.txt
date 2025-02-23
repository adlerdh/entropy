cmake_minimum_required(VERSION 3.24.0 FATAL_ERROR)
cmake_policy(SET CMP0175 OLD)

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

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(RES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources)
set(EXT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/externals)

# CMake will generate a file compile_commands.json under the build directory.
# VSCode is able to parse the file and find the include path based on its content.
add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(UNIX)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
else()
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

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


option(Entropy_SUPERBUILD "Run the SuperBuild" ON)

set(EXTERNAL_DIR "${CMAKE_CURRENT_BINARY_DIR}/external")

# External library versions:
set(argparse_VERSION "3.2")
set(boost_VERSION "1.87.0")
set(glfw_VERSION "3.4")
set(glm_VERSION "1.0.1")
set(itk_VERSION "5.4.2")
set(nlohmann_json_VERSION "3.11.3")
set(spdlog_VERSION "1.15.1")
set(tinyfsm_VERSION "1.15.1")

# External library directores:
set(argparse_PREFIX "${EXTERNAL_DIR}/argparse-${argparse_VERSION}")
set(Boost_PREFIX "${EXTERNAL_DIR}/boost-${boost_VERSION}")
set(glfw_PREFIX "${EXTERNAL_DIR}/glfw-${glfw_VERSION}")
set(glm_PREFIX "${EXTERNAL_DIR}/glm-${glm_VERSION}")
set(itk_PREFIX "${EXTERNAL_DIR}/itk-${itk_VERSION}")
set(nanovg_PREFIX "${EXTERNAL_DIR}/nanovg")
set(nlohmann_json_PREFIX "${EXTERNAL_DIR}/nlohmann_json-${nlohmann_json_VERSION}")
set(spdlog_PREFIX "${EXTERNAL_DIR}/spdlog-${spdlog_VERSION}")
set(tinyfsm_PREFIX "${EXTERNAL_DIR}/tinyfsm-${tinyfsm_VERSION}")

if(Entropy_SUPERBUILD)
  include(SuperBuild)
  return()
endif()

# The Insight Toolkit (ITK)
find_package(ITK ${itk_VERSION} REQUIRED HINTS "${itk_PREFIX}/build")

if(ITK_FOUND)
  message(STATUS "Found ITK in ${ITK_DIR}")
  include(${ITK_USE_FILE})
else()
  message(FATAL_ERROR "ITK not found: Please set ITK_DIR")
endif()

# Boost C++ Libraries
set(Boost_INCLUDE_DIR "${Boost_PREFIX}/src" CACHE STRING "Boost include directory" FORCE)

if(IS_DIRECTORY "${Boost_INCLUDE_DIR}")
  message(STATUS "Using Boost headers in ${Boost_INCLUDE_DIR}")
else()
  message(FATAL_ERROR "Boost headers not found: Please set Boost_INCLUDE_DIR")
endif()

# spdlog: Fast C++ logging library
find_package(spdlog ${spdlog_VERSION} REQUIRED HINTS "${spdlog_PREFIX}/install")

if(spdlog_FOUND)
  message(STATUS "Using spdlog in ${spdlog_DIR}")
else()
  message(FATAL_ERROR "spdlog not found: Please set spdlog_DIR")
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

# Niels Lohmann's JSON for Modern C++
find_package(nlohmann_json ${nlohmann_json_VERSION} REQUIRED HINTS "${nlohmann_json_PREFIX}/install")

if(nlohmann_json_FOUND)
  message(STATUS "Using nlohmann_json in ${nlohmann_json_DIR}")
else()
  message(FATAL_ERROR "nlohmann_json not found")
endif()

# OpenGL Mathematics (GLM): C++ mathematics library for graphics software based
# on the OpenGL Shading Language (GLSL) specifications
find_package(glm ${glm_VERSION} REQUIRED CONFIG REQUIRED HINTS "${glm_PREFIX}/install")

if(glm_FOUND)
  message(STATUS "Using GLM in ${glm_DIR}")
else()
  message(FATAL_ERROR "GLM not found")
endif()

# argparse: Header-only library for argument parsing using modern C++
find_package(argparse ${argparse_VERSION} REQUIRED HINTS "${argparse_PREFIX}/install")

if(argparse_FOUND)
  message(STATUS "Using argparse in ${argparse_DIR}")
  set(argparse_INCLUDE_DIR "${argparse_PREFIX}/install/include"
      CACHE STRING "argparse include directory" FORCE)
else()
  message(FATAL_ERROR "argparse not found")
endif()

# TinyFSM: A simple, header-only C++ finite state machine library
set(tinyfsm_INCLUDE_DIR "${tinyfsm_PREFIX}/src/include"
    CACHE STRING "TinyFSM include directory" FORCE)



#--------------------------------------------------------------------------------
# GLAD library:
# Multi-Language GL/GLES/EGL/GLX/WGL Loader-Generator based on the official specs
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

set(GLAD_BASE glad_gl_${GLAD_GL_VERSION}_core)

if(GLAD_GL_DEBUG)
  set(GLAD_BASE ${GLAD_BASE}_debug)
endif()

set(GLAD_DIR ${EXT_DIR}/glad/${GLAD_BASE})
set(GLAD_INCLUDE_DIR ${GLAD_DIR}/include)

add_library(glad ${GLAD_DIR}/src/glad.c)
target_include_directories(glad PRIVATE ${GLAD_INCLUDE_DIR})



#--------------------------------------------------------------------------------
# "Dear ImGui" library (included as Git submodule):
# A bloat-free graphical user interface library for C++. It outputs optimized
# vertex buffers that you can render anytime in your 3D-pipeline enabled
# application. It is fast, portable, renderer agnostic and self-contained
# (no external dependencies).
#--------------------------------------------------------------------------------
set(IMGUI_SRC_DIR ${EXT_DIR}/imgui)
set(IMGUI_INCLUDE_DIR ${EXT_DIR}/imgui)
set(IMGUI_BINDINGS_DIR ${SRC_DIR}/ui/imgui)

add_compile_definitions(IMGUI_USER_CONFIG="ui/entropy_imgui_config.h")

list(APPEND IMGUI_SOURCES
  ${IMGUI_SRC_DIR}/imgui.cpp
  ${IMGUI_SRC_DIR}/imgui_demo.cpp
  ${IMGUI_SRC_DIR}/imgui_draw.cpp
  ${IMGUI_SRC_DIR}/imgui_tables.cpp
  ${IMGUI_SRC_DIR}/imgui_widgets.cpp
  ${IMGUI_SRC_DIR}/misc/cpp/imgui_stdlib.cpp
  ${IMGUI_SRC_DIR}/backends/imgui_impl_glfw.cpp # GLFW bindings for ImGui
  ${IMGUI_SRC_DIR}/backends/imgui_impl_opengl3.cpp # OpenGL 3 bindings for ImGui
)

add_library(imgui ${IMGUI_SOURCES})

target_include_directories(imgui PRIVATE
  ${IMGUI_INCLUDE_DIR}
  ${SRC_DIR} # needed for IMGUI_USER_CONFIG file
  ${glfw_INCLUDE_DIR}
)

target_link_libraries(imgui PRIVATE
  spdlog::spdlog # used in IMGUI_USER_CONFIG
)

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
  CXX_EXTENSIONS ON)


#--------------------------------------------------------------------------------
# ImPlot (included as Git submodule):
# ImPlot is an immediate mode, GPU accelerated plotting library for Dear ImGui.
#--------------------------------------------------------------------------------

set(IMPLOT_INCLUDE_DIR ${EXT_DIR}/implot)

list(APPEND IMPLOT_SOURCES
  ${EXT_DIR}/implot/implot_demo.cpp
  ${EXT_DIR}/implot/implot_items.cpp
  ${EXT_DIR}/implot/implot.cpp)

add_library(implot ${IMPLOT_SOURCES})

target_include_directories(implot PRIVATE
  ${IMPLOT_INCLUDE_DIR}
  ${IMGUI_INCLUDE_DIR}
  ${SRC_DIR} # needed for IMGUI_USER_CONFIG file
)

target_link_libraries(implot PRIVATE
  spdlog::spdlog # used in IMGUI_USER_CONFIG
)

target_compile_options(implot PRIVATE
   $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
    -Wall -Wextra -Wpointer-arith -Winit-self -Wunreachable-code -Wshadow -g -ftrapv>
   $<$<CXX_COMPILER_ID:MSVC>:
    /W4>)

set_target_properties(implot PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS ON)


#--------------------------------------------------------------------------------
# NanoVG: A small antialiased vector graphics rendering library for OpenGL.
# It has lean API modeled after HTML5 canvas API. It is aimed to be a practical
# and fun toolset for building scalable user interfaces and visualizations.
#--------------------------------------------------------------------------------
set(nanovg_SRC_DIR "${nanovg_PREFIX}/src/src")
add_library(nanovg "${nanovg_SRC_DIR}/nanovg.c")
target_include_directories(nanovg PRIVATE "${nanovg_SRC_DIR}")

#--------------------------------------------------------------------------------
# GridCut (NOT included as Git submodule):
# Header-only C++ library of fast max-flow/min-cut solvers optimized for grid graphs
#--------------------------------------------------------------------------------
set(GRIDCUT_INCLUDE_DIRS
  ${EXT_DIR}/gridcut/include
  ${EXT_DIR}/gridcut/examples/include)

#--------------------------------------------------------------------------------
# IconFontCppHeaders (included as Git submodule in ${EXT_DIR}/IconFontCppHeaders):
# C, C++ headers and C# classes for icon fonts Font Awesome, Fork Awesome,
# Google Material Design icons, Kenney game icons and Fontaudio.
#--------------------------------------------------------------------------------

#--------------------------------------------------------------------------------
# stduuid (included as Git submodule):
# A C++17 cross-platform single-header library implementation for universally
# unique identifiers
#--------------------------------------------------------------------------------
set(UUID_DIR ${EXT_DIR}/stduuid)
set(UUID_INCLUDE_DIR ${EXT_DIR}/stduuid/include)

#--------------------------------------------------------------------------------
# ghc::filesystem (included as Git submodule):
# A header-only single-file std::filesystem compatible helper library, based on the
# C++17 and C++20 specs, but implemented for C++11, C++14, C++17 or C++20 (tightly
# following the C++17 standard with very few documented exceptions).
#
# This library is needed because std::filesystem is not fully implemented on macOS
# prior to version 10.15.
#--------------------------------------------------------------------------------
set(GHC_FILESYSTEM_BUILD_TESTING OFF)
set(GHC_FILESYSTEM_BUILD_EXAMPLES OFF)
set(GHC_FILESYSTEM_WITH_INSTALL OFF)
set(GHC_FILESYSTEM ${EXT_DIR}/filesystem/include)

add_subdirectory(${EXT_DIR}/filesystem)

#--------------------------------------------------------------------------------
# CMakeRC (included as Git submodule):
# Standalone CMake-Based C++ Resource Compiler
#--------------------------------------------------------------------------------
include(${EXT_DIR}/cmrc/CMakeRC.cmake)

#--------------------------------------------------------------------------------
# Entropy's sources
#--------------------------------------------------------------------------------

# Generate build stamp header after GLAD_GL_VERSION_MAJOR and
# GLAD_GL_VERSION_MINOR are defined
include(BuildStamp)
BuildStamp()

set(ENTROPY_SOURCES
  ${SRC_DIR}/main.cpp
  ${SRC_DIR}/EntropyApp.cpp

  ${SRC_DIR}/common/CoordinateFrame.cpp
  ${SRC_DIR}/common/DataHelper.cpp
  ${SRC_DIR}/common/DirectionMaps.cpp
  ${SRC_DIR}/common/InputParams.cpp
  ${SRC_DIR}/common/InputParser.cpp
  ${SRC_DIR}/common/MathFuncs.cpp
  ${SRC_DIR}/common/ParcellationLabelTable.cpp
  ${SRC_DIR}/common/Types.cpp
  ${SRC_DIR}/common/UuidUtility.cpp
  ${SRC_DIR}/common/Viewport.cpp

  ${SRC_DIR}/image/Image.cpp
  ${SRC_DIR}/image/ImageColorMap.cpp
  ${SRC_DIR}/image/ImageHeader.cpp
  ${SRC_DIR}/image/ImageIoInfo.cpp
  ${SRC_DIR}/image/ImageSettings.cpp
  ${SRC_DIR}/image/ImageTransformations.cpp
  ${SRC_DIR}/image/ImageUtility.cpp
  ${SRC_DIR}/image/SegUtil.cpp
  ${SRC_DIR}/image/SurfaceUtility.cpp

  ${SRC_DIR}/logic/app/CallbackHandler.cpp
  ${SRC_DIR}/logic/app/Data.cpp
  ${SRC_DIR}/logic/app/Logging.cpp
  ${SRC_DIR}/logic/app/Settings.cpp
  ${SRC_DIR}/logic/app/State.cpp

  ${SRC_DIR}/logic/annotation/Annotation.cpp
  ${SRC_DIR}/logic/annotation/BezierHelper.cpp
  ${SRC_DIR}/logic/annotation/LandmarkGroup.cpp
  ${SRC_DIR}/logic/annotation/SerializeAnnot.cpp

  ${SRC_DIR}/logic/camera/Camera.cpp
  ${SRC_DIR}/logic/camera/CameraHelpers.cpp
  ${SRC_DIR}/logic/camera/CameraTypes.cpp
  ${SRC_DIR}/logic/camera/MathUtility.cpp
  ${SRC_DIR}/logic/camera/OrthogonalProjection.cpp
  ${SRC_DIR}/logic/camera/PerspectiveProjection.cpp
  ${SRC_DIR}/logic/camera/Projection.cpp

  ${SRC_DIR}/logic/interaction/ViewHit.cpp
  ${SRC_DIR}/logic/interaction/events/ButtonState.cpp

  ${SRC_DIR}/logic/segmentation/GraphCuts.cpp
  ${SRC_DIR}/logic/segmentation/Poisson.cpp
  ${SRC_DIR}/logic/segmentation/SegHelpers.cpp

  ${SRC_DIR}/logic/serialization/ProjectSerialization.cpp

  ${SRC_DIR}/logic/states/AnnotationStateHelpers.cpp
  ${SRC_DIR}/logic/states/AnnotationStateMachine.cpp
  ${SRC_DIR}/logic/states/AnnotationStates.cpp

  # We were testing IPC with ITK-SNAP. This functionality is not currently hooked up to Entropy.
  # ${SRC_DIR}/logic/ipc/IPCHandler.cxx

  ${SRC_DIR}/rendering/ImageDrawing.cpp
  ${SRC_DIR}/rendering/Rendering.cpp
  ${SRC_DIR}/rendering/RenderData.cpp
  ${SRC_DIR}/rendering/TextureSetup.cpp
  ${SRC_DIR}/rendering/VectorDrawing.cpp

  ${SRC_DIR}/rendering/utility/containers/VertexAttributeInfo.cpp
  ${SRC_DIR}/rendering/utility/containers/VertexIndicesInfo.cpp
  ${SRC_DIR}/rendering/utility/containers/Uniforms.cpp
  ${SRC_DIR}/rendering/utility/gl/GLBufferObject.cpp
  ${SRC_DIR}/rendering/utility/gl/GLBufferTexture.cpp
  ${SRC_DIR}/rendering/utility/gl/GLErrorChecker.cpp
  ${SRC_DIR}/rendering/utility/gl/GLFrameBufferObject.cpp
  ${SRC_DIR}/rendering/utility/gl/GLShader.cpp
  ${SRC_DIR}/rendering/utility/gl/GLShaderProgram.cpp
  ${SRC_DIR}/rendering/utility/gl/GLTexture.cpp
  ${SRC_DIR}/rendering/utility/gl/GLVersionChecker.cpp
  ${SRC_DIR}/rendering/utility/gl/GLVertexArrayObject.cpp
  ${SRC_DIR}/rendering/utility/math/SliceIntersector.cpp

  ${SRC_DIR}/ui/GuiData.cpp
  ${SRC_DIR}/ui/Headers.cpp
  ${SRC_DIR}/ui/Helpers.cpp
  ${SRC_DIR}/ui/IsosurfaceHeader.cpp
  ${SRC_DIR}/ui/ImGuiCustomControls.cpp
  ${SRC_DIR}/ui/ImGuiWrapper.cpp
  ${SRC_DIR}/ui/MainMenuBar.cpp
  ${SRC_DIR}/ui/Popups.cpp
  ${SRC_DIR}/ui/Style.cpp
  ${SRC_DIR}/ui/Toolbars.cpp
  ${SRC_DIR}/ui/Widgets.cpp
  ${SRC_DIR}/ui/Windows.cpp

  ${SRC_DIR}/ui/imgui/imGuIZMO.quat/imGuIZMOquat.cpp
  ${SRC_DIR}/ui/imgui/imgui-knobs/imgui-knobs.cpp

  ${SRC_DIR}/windowing/ControlFrame.cpp
  ${SRC_DIR}/windowing/GlfwCallbacks.cpp
  ${SRC_DIR}/windowing/GlfwWrapper.cpp
  ${SRC_DIR}/windowing/Layout.cpp
  ${SRC_DIR}/windowing/View.cpp
  ${SRC_DIR}/windowing/ViewTypes.cpp
  ${SRC_DIR}/windowing/WindowData.cpp
)

set(SHADERS
  ${SRC_DIR}/rendering/shaders/Correlation.fs
  ${SRC_DIR}/rendering/shaders/Difference.fs
  ${SRC_DIR}/rendering/shaders/Edge.fs
  ${SRC_DIR}/rendering/shaders/Image.fs
  ${SRC_DIR}/rendering/shaders/ImageRgba.fs
  ${SRC_DIR}/rendering/shaders/Image.vs
  ${SRC_DIR}/rendering/shaders/Metric.vs
  ${SRC_DIR}/rendering/shaders/Overlay.fs
  ${SRC_DIR}/rendering/shaders/RaycastIsoSurface.fs
  ${SRC_DIR}/rendering/shaders/RaycastIsoSurface.vs
  ${SRC_DIR}/rendering/shaders/Simple.fs
  ${SRC_DIR}/rendering/shaders/Simple.vs
  ${SRC_DIR}/rendering/shaders/Xray.fs)

set(SHADERS_OLD
  ${SRC_DIR}/rendering_old/shaders/BasicMesh.vert
  ${SRC_DIR}/rendering_old/shaders/BasicMesh.frag
  ${SRC_DIR}/rendering_old/shaders/BasicMeshPeel.frag
  ${SRC_DIR}/rendering_old/shaders/Debug.vert
  ${SRC_DIR}/rendering_old/shaders/Debug.frag
  ${SRC_DIR}/rendering_old/shaders/fbo_screen.vert
  ${SRC_DIR}/rendering_old/shaders/fbo_screen.frag
  ${SRC_DIR}/rendering_old/shaders/Flat.vert
  ${SRC_DIR}/rendering_old/shaders/Flat.frag
  ${SRC_DIR}/rendering_old/shaders/FlatPeel.frag
  ${SRC_DIR}/rendering_old/shaders/Mesh.vert
  ${SRC_DIR}/rendering_old/shaders/Mesh.frag
  ${SRC_DIR}/rendering_old/shaders/MeshPeel.frag
  ${SRC_DIR}/rendering_old/shaders/Polygonizer.vert
  ${SRC_DIR}/rendering_old/shaders/Polygonizer.geom
  ${SRC_DIR}/rendering_old/shaders/Simple.vert
  ${SRC_DIR}/rendering_old/shaders/Simple.frag
  ${SRC_DIR}/rendering_old/shaders/ddp/blend.frag
  ${SRC_DIR}/rendering_old/shaders/ddp/blend.vert
  ${SRC_DIR}/rendering_old/shaders/ddp/final.frag
  ${SRC_DIR}/rendering_old/shaders/ddp/InitializeDepths.frag
  ${SRC_DIR}/rendering_old/shaders/ddp/init.vert)

file(GLOB COLORMAPS
  ${RES_DIR}/colormaps/ncl/*.csv
  ${RES_DIR}/colormaps/matplotlib/*.csv
  ${RES_DIR}/colormaps/peter_kovesi/*.csv)

set(FONTS
  ${RES_DIR}/fonts/Cousine/Cousine-Regular.ttf
  ${RES_DIR}/fonts/ForkAwesome/forkawesome-webfont.ttf
  ${RES_DIR}/fonts/HelveticaNeue/HelveticaNeue-Light.ttf
  ${RES_DIR}/fonts/Inter/Inter-Light.ttf
  ${RES_DIR}/fonts/Roboto/Roboto-Light.ttf
  ${RES_DIR}/fonts/Roboto/Roboto-Regular.ttf
  ${RES_DIR}/fonts/Satoshi/Satoshi-Light.ttf
  ${RES_DIR}/fonts/SFMono/SFMono-Regular.ttf
  ${RES_DIR}/fonts/SFPro/sf-pro-text-regular.ttf
  ${RES_DIR}/fonts/SpaceGrotesk/SpaceGrotesk-Light.ttf
  ${RES_DIR}/fonts/Supreme/Supreme-Light.ttf)


#--------------------------------------------------------------------------------
# Build types and compile options
#--------------------------------------------------------------------------------

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


#--------------------------------------------------------------------------------
# Define main executable target
#--------------------------------------------------------------------------------
set(EXEC_NAME Entropy)

# Embed the resources using CMakeRC
cmrc_add_resource_library(${EXEC_NAME}-colormaps ALIAS ${EXEC_NAME}::colormaps ${COLORMAPS} NAMESPACE colormaps)
cmrc_add_resource_library(${EXEC_NAME}-fonts ALIAS ${EXEC_NAME}::fonts ${FONTS} NAMESPACE fonts)
#cmrc_add_resource_library(${EXEC_NAME}-shaders ALIAS ${EXEC_NAME}::shaders ${SHADERS} ${SHADERS_OLD} NAMESPACE shaders)
cmrc_add_resource_library(${EXEC_NAME}-shaders ALIAS ${EXEC_NAME}::shaders ${SHADERS} NAMESPACE shaders)

add_executable(${EXEC_NAME})

target_sources(${EXEC_NAME} PRIVATE
  ${ENTROPY_SOURCES}
  ${ENTROPY_HEADERS}
)

target_link_libraries(${EXEC_NAME} PRIVATE
  ${ITK_LIBRARIES}
  ghc_filesystem
  glfw
  glad
  glm::glm
  imgui
  implot
  nanovg
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  ${EXEC_NAME}::colormaps
  ${EXEC_NAME}::fonts
  ${EXEC_NAME}::shaders
)

target_include_directories(${EXEC_NAME} PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR} # for defines.h
  ${SRC_DIR}
  ${EXT_DIR}
  ${RES_DIR}
  ${GHC_FILESYSTEM}
  ${GLAD_INCLUDE_DIR}
  ${IMGUI_BINDINGS_DIR}
  ${IMGUI_INCLUDE_DIR}
  ${IMGUI_KNOBS_DIR}
  ${IMPLOT_INCLUDE_DIR}
  ${ITK_INCLUDE_DIRS}
  ${tinyfsm_INCLUDE_DIR}
  ${UUID_DIR}
  ${UUID_INCLUDE_DIR}
)

# These libraries are in 'system' includes in order to ignore their compiler warnings:
target_include_directories(${EXEC_NAME} SYSTEM PRIVATE
  ${argparse_INCLUDE_DIR}
  ${Boost_INCLUDE_DIR}
  ${GRIDCUT_INCLUDE_DIRS}
  ${nanovg_SRC_DIR}
)

target_compile_options(${EXEC_NAME} PRIVATE
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

set_target_properties(${EXEC_NAME} PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)
