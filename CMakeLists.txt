cmake_minimum_required(VERSION 3.28.0 FATAL_ERROR)

if(POLICY CMP0175)
  cmake_policy(SET CMP0175 OLD)
endif()

set(APP_NAME "Entropy")
set(APP_DESCRIPTION "Visualize, compare, segment, annotate, and co-register medical images")
set(APP_FILE_DESCRIPTION "Entropy Medical Image Viewer")
set(PICSL_NAME "Penn Image Computing and Science Lab (PICSL), University of Pennsylvania")
set(PUBLISHER_NAME "Daniel H. Adler")
set(AUTHOR_CREDIT_LINE "Primary author and maintainer: Daniel H. Adler, Ph.D.")
set(COPYRIGHT_LINE "Copyright 2021-2026 Daniel H. Adler, Ph.D. and the ${PICSL_NAME}")
set(LICENSE_LINE "Apache License, Version 2.0")

set(VERSION_MAJOR 0)
set(VERSION_MINOR 9)
set(VERSION_FEATURE 8)
set(VERSION_PATCH 4)
set(VERSION_FULL "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_FEATURE}.${VERSION_PATCH}")

project(Entropy VERSION ${VERSION_FULL} LANGUAGES C CXX)

if(APPLE)
  enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(Entropy_USE_CCACHE "Use ccache as the compiler launcher when available" ON)
option(Entropy_ENABLE_IWYU "Run Include What You Use during C++ compilation" OFF)
set(Entropy_IWYU_OPTIONS
  "-Xiwyu;--no_fwd_decls;-Xiwyu;--quoted_includes_first;-Xiwyu;--max_line_length=120;-Xiwyu;--error=0"
  CACHE STRING "Additional options passed to Include What You Use")
option(Entropy_ENABLE_TRACE_LOGGING "Compile trace-level logging calls into Entropy" OFF)

if(Entropy_USE_CCACHE)
  find_program(CCACHE_PROGRAM ccache)
  if(CCACHE_PROGRAM)
    message(STATUS "Using ccache compiler launcher: ${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE FILEPATH "C compiler launcher" FORCE)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE FILEPATH "CXX compiler launcher" FORCE)
    if(APPLE)
      set(CMAKE_OBJCXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE FILEPATH "OBJCXX compiler launcher" FORCE)
    endif()
  else()
    message(STATUS "ccache was requested but was not found")
  endif()
else()
  unset(CMAKE_C_COMPILER_LAUNCHER CACHE)
  unset(CMAKE_CXX_COMPILER_LAUNCHER CACHE)
  if(APPLE)
    unset(CMAKE_OBJCXX_COMPILER_LAUNCHER CACHE)
  endif()
endif()

if(Entropy_ENABLE_IWYU)
  find_program(Entropy_IWYU_EXECUTABLE NAMES include-what-you-use iwyu)
  if(NOT Entropy_IWYU_EXECUTABLE)
    message(FATAL_ERROR "Entropy_ENABLE_IWYU=ON but include-what-you-use was not found")
  endif()

  message(STATUS "Using Include What You Use: ${Entropy_IWYU_EXECUTABLE}")
  set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${Entropy_IWYU_EXECUTABLE};${Entropy_IWYU_OPTIONS}"
      CACHE STRING "Include What You Use command" FORCE)
endif()

function(entropy_disable_iwyu_for_targets)
  if(NOT Entropy_ENABLE_IWYU)
    return()
  endif()

  foreach(target IN LISTS ARGN)
    if(TARGET "${target}")
      set_target_properties("${target}" PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "")
    endif()
  endforeach()
endfunction()

set(entropy_APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/app")
set(entropy_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib")
set(entropy_UI_DIR "${entropy_LIB_DIR}/ui")
set(entropy_RES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res")
set(entropy_EXT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")
set(entropy_FIREANTS_BRIDGE_DIR "${entropy_LIB_DIR}/registration/fireants_bridge")

# CMake will generate a file compile_commands.json under the build directory.
# VSCode is able to parse the file and find the include path based on its content.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(CMAKE_VERBOSE_MAKEFILE "Show full native build commands" OFF)
set(Entropy_SUPERBUILD_PARALLEL "" CACHE STRING "Parallel level for ExternalProject builds; empty uses the native build tool default")
set(Entropy_SUPERBUILD_CONFIG "Release" CACHE STRING "Build config for multi-config SuperBuild generators")
set(entropy_LINUX_PACKAGE_PLATFORM_LABEL_DEFAULT "Linux")
if(UNIX AND NOT APPLE AND EXISTS "/etc/os-release")
  file(STRINGS "/etc/os-release" entropy_OS_RELEASE_LINES REGEX "^(ID|VERSION_ID)=")
  foreach(entropy_OS_RELEASE_LINE IN LISTS entropy_OS_RELEASE_LINES)
    if(entropy_OS_RELEASE_LINE MATCHES "^ID=\"?([A-Za-z0-9._-]+)\"?$")
      set(entropy_LINUX_OS_ID "${CMAKE_MATCH_1}")
    elseif(entropy_OS_RELEASE_LINE MATCHES "^VERSION_ID=\"?([A-Za-z0-9._-]+)\"?$")
      set(entropy_LINUX_OS_VERSION_ID "${CMAKE_MATCH_1}")
    endif()
  endforeach()

  if(entropy_LINUX_OS_ID)
    if(entropy_LINUX_OS_ID STREQUAL "ubuntu")
      set(entropy_LINUX_OS_LABEL "Ubuntu")
    elseif(entropy_LINUX_OS_ID STREQUAL "fedora")
      set(entropy_LINUX_OS_LABEL "Fedora")
    elseif(entropy_LINUX_OS_ID STREQUAL "debian")
      set(entropy_LINUX_OS_LABEL "Debian")
    else()
      string(SUBSTRING "${entropy_LINUX_OS_ID}" 0 1 entropy_LINUX_OS_ID_FIRST)
      string(SUBSTRING "${entropy_LINUX_OS_ID}" 1 -1 entropy_LINUX_OS_ID_REST)
      string(TOUPPER "${entropy_LINUX_OS_ID_FIRST}" entropy_LINUX_OS_ID_FIRST)
      set(entropy_LINUX_OS_LABEL "${entropy_LINUX_OS_ID_FIRST}${entropy_LINUX_OS_ID_REST}")
    endif()

    if(entropy_LINUX_OS_VERSION_ID)
      set(entropy_LINUX_PACKAGE_PLATFORM_LABEL_DEFAULT "${entropy_LINUX_OS_LABEL}-${entropy_LINUX_OS_VERSION_ID}")
    else()
      set(entropy_LINUX_PACKAGE_PLATFORM_LABEL_DEFAULT "${entropy_LINUX_OS_LABEL}")
    endif()
  endif()
endif()
set(Entropy_LINUX_PACKAGE_PLATFORM_LABEL "${entropy_LINUX_PACKAGE_PLATFORM_LABEL_DEFAULT}" CACHE STRING "Platform label used in generated Linux package filenames")
set(Entropy_LINUX_CPACK_GENERATORS "DEB;TGZ" CACHE STRING "CPack generators used for Linux packages")
set_property(CACHE Entropy_SUPERBUILD_CONFIG PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(GNUInstallDirs)
include(Coverage)

get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(_isMultiConfig)
  set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel"
      CACHE STRING "Configs" FORCE)
endif()

if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "Embedded" CACHE STRING
    "Use object-embedded debug information for Ninja/MSVC builds to avoid parallel PDB writer contention"
    FORCE)
endif()

if(MSVC)
  foreach(_entropy_msvc_flag_var IN ITEMS
      CMAKE_C_FLAGS_DEBUG
      CMAKE_CXX_FLAGS_DEBUG
      CMAKE_C_FLAGS_RELWITHDEBINFO
      CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    if(DEFINED ${_entropy_msvc_flag_var} AND NOT "${${_entropy_msvc_flag_var}}" MATCHES "(^| )/FS($| )")
      string(APPEND ${_entropy_msvc_flag_var} " /FS")
    endif()
  endforeach()
endif()

add_library(entropy_warnings INTERFACE)
target_compile_options(entropy_warnings INTERFACE
  $<$<AND:$<CONFIG:Debug>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>>:
    -fstack-protector-all
  >
)

add_library(entropy_logging_level INTERFACE)
target_compile_definitions(entropy_logging_level INTERFACE
  $<$<OR:$<CONFIG:Debug>,$<BOOL:${Entropy_ENABLE_TRACE_LOGGING}>>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE>
  $<$<NOT:$<OR:$<CONFIG:Debug>,$<BOOL:${Entropy_ENABLE_TRACE_LOGGING}>>>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG>
  $<$<CONFIG:Debug>:ENTROPY_DEFAULT_LOG_LEVEL=SPDLOG_LEVEL_DEBUG>
  $<$<NOT:$<CONFIG:Debug>>:ENTROPY_DEFAULT_LOG_LEVEL=SPDLOG_LEVEL_INFO>
)

message(STATUS "CMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "Entropy_ENABLE_TRACE_LOGGING=${Entropy_ENABLE_TRACE_LOGGING}")

entropy_configure_coverage()

# Prevent compilation in-source
if(CMAKE_BINARY_DIR STREQUAL PROJECT_SOURCE_DIR)
  message(FATAL_ERROR "Source and build directories are the same.")
endif()

# Set a default build type if none was specified (single-config generators only)
if(NOT _isMultiConfig)
  if(NOT CMAKE_BUILD_TYPE)
    set(DEFAULT_BUILD_TYPE "RelWithDebInfo")
    message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build" FORCE)

    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
  endif()
endif()

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
set(entropy_STATIC_BUNDLED_DEPENDENCIES_DEFAULT OFF)
if(UNIX)
  set(entropy_STATIC_BUNDLED_DEPENDENCIES_DEFAULT ON)
endif()
option(Entropy_STATIC_BUNDLED_DEPENDENCIES
  "Build bundled third-party dependencies as static libraries where practical; Qt and system libraries stay dynamic"
  ${entropy_STATIC_BUNDLED_DEPENDENCIES_DEFAULT})

if(BUILD_SHARED_LIBS)
  set(BUILD_STATIC_LIBS OFF)
else()
  set(BUILD_STATIC_LIBS ON)
endif()

message(STATUS "BUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}")
message(STATUS "BUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}")
message(STATUS "Entropy_STATIC_BUNDLED_DEPENDENCIES=${Entropy_STATIC_BUNDLED_DEPENDENCIES}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

function(entropy_copy_runtime_dlls target)
  if(WIN32)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND}
        -D "ENTROPY_RUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:${target}>,,>"
        -D "ENTROPY_RUNTIME_OUTPUT_DIR=$<TARGET_FILE_DIR:${target}>"
        -P "${CMAKE_SOURCE_DIR}/cmake/CopyRuntimeDlls.cmake"
    )
  endif()
endfunction()


# External library versions:
set(catch2_VERSION "3.8.1")
set(cli11_VERSION "2.6.2")
set(cmakerc_VERSION "2.0.1")
set(curl_VERSION "8.21.0")
set(glfw_VERSION "3.4")
set(glm_VERSION "1.0.3")
set(iconfont_VERSION "210b5a399a64270674560d633638952d1e8d804d")
set(imgui_VERSION "1.92.8-docking")
set(implot_VERSION "0.17")
set(itk_VERSION "5.4.3")
set(nanovg_VERSION "ce3bf745eb2d2dbc14a50bf2446783f691ac4353")
set(nativefiledialog_VERSION "1.3.0")
set(nlohmann_json_VERSION "3.12.0")
set(qtbase_VERSION "6.8.1")
set(spdlog_VERSION "1.17.0")
set(stduuid_VERSION "1.2.3")
set(tinyfsm_VERSION "1.15.1")

include(ItkComponents)

# External library directories:
set(EXTERNAL_DIR "${CMAKE_BINARY_DIR}/external")
set(catch2_PREFIX "${EXTERNAL_DIR}/catch2-${catch2_VERSION}")
set(cli11_PREFIX "${EXTERNAL_DIR}/cli11-${cli11_VERSION}")
set(cmakerc_PREFIX "${EXTERNAL_DIR}/cmakerc-${cmakerc_VERSION}")
set(curl_PREFIX "${EXTERNAL_DIR}/curl-${curl_VERSION}")
set(glfw_PREFIX "${EXTERNAL_DIR}/glfw-${glfw_VERSION}")
set(glm_PREFIX "${EXTERNAL_DIR}/glm-${glm_VERSION}")
set(iconfont_PREFIX "${EXTERNAL_DIR}/iconfont-${iconfont_VERSION}")
set(imgui_PREFIX "${EXTERNAL_DIR}/imgui-${imgui_VERSION}")
set(implot_PREFIX "${EXTERNAL_DIR}/implot-${implot_VERSION}")
set(itk_PREFIX "${EXTERNAL_DIR}/itk-${itk_VERSION}")
set(nanovg_PREFIX "${EXTERNAL_DIR}/nanovg-${nanovg_VERSION}")
set(nativefiledialog_PREFIX "${EXTERNAL_DIR}/nativefiledialog-${nativefiledialog_VERSION}")
set(nlohmann_json_PREFIX "${EXTERNAL_DIR}/nlohmann_json-${nlohmann_json_VERSION}")
set(qtbase_PREFIX "${EXTERNAL_DIR}/qtbase-${qtbase_VERSION}")
set(spdlog_PREFIX "${EXTERNAL_DIR}/spdlog-${spdlog_VERSION}")
set(stduuid_PREFIX "${EXTERNAL_DIR}/stduuid-${stduuid_VERSION}")
set(tinyfsm_PREFIX "${EXTERNAL_DIR}/tinyfsm-${tinyfsm_VERSION}")

option(Entropy_SUPERBUILD "Run the SuperBuild" ON)

include(CTest)

if(Entropy_SUPERBUILD)
  get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  include(SuperBuild)
  return()
endif()


if(BUILD_TESTING)
  find_package(Catch2 ${catch2_VERSION} REQUIRED HINTS "${catch2_PREFIX}/install")
  message(STATUS "Using Catch2 in ${Catch2_DIR}")
  include(Catch)
endif()

if(APPLE)
  set(Entropy_MACOSX_BUNDLE_IDENTIFIER "io.github.adlerdh.entropy" CACHE STRING "macOS bundle identifier")

  if(CMAKE_OSX_DEPLOYMENT_TARGET)
    set(entropy_DEFAULT_MACOSX_BUNDLE_MINIMUM_SYSTEM_VERSION "${CMAKE_OSX_DEPLOYMENT_TARGET}")
  else()
    set(entropy_DEFAULT_MACOSX_BUNDLE_MINIMUM_SYSTEM_VERSION "13.0")
  endif()

  set(Entropy_MACOSX_BUNDLE_MINIMUM_SYSTEM_VERSION "${entropy_DEFAULT_MACOSX_BUNDLE_MINIMUM_SYSTEM_VERSION}"
    CACHE STRING "Minimum macOS version recorded in Info.plist")
  set(Entropy_MACOSX_BUNDLE_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_FEATURE}"
    CACHE STRING "macOS build version recorded in CFBundleVersion")
  set(Entropy_MACOS_CODESIGN_IDENTITY "-" CACHE STRING
    "macOS code signing identity used by build/install/CPack; '-' creates an ad-hoc signature, and an empty value skips signing")
endif()

# CLI11: Header-only command line parser
find_package(CLI11 ${cli11_VERSION} REQUIRED
  HINTS "${cli11_PREFIX}/install")

message(STATUS "Using CLI11 in ${CLI11_DIR}")

# CMakeRC: Standalone CMake-based C++ resource compiler
include("${cmakerc_PREFIX}/src/CMakeRC.cmake")

# libcurl: portable client-side URL transfer library used for update checks.
# macOS uses the SDK libcurl; Windows/Linux use the SuperBuild-pinned CMake package.
if(APPLE)
  execute_process(
    COMMAND xcrun --sdk macosx --show-sdk-path
    OUTPUT_VARIABLE entropy_MACOS_SDK_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  set(CURL_INCLUDE_DIR "${entropy_MACOS_SDK_PATH}/usr/include")
  find_library(CURL_LIBRARY curl
    PATHS "${entropy_MACOS_SDK_PATH}/usr/lib"
    NO_DEFAULT_PATH)
  set(CURL_INCLUDE_DIR "${CURL_INCLUDE_DIR}" CACHE PATH "macOS SDK curl include directory" FORCE)
  set(CURL_LIBRARY "${CURL_LIBRARY}" CACHE FILEPATH "macOS SDK curl library" FORCE)
  set(CURL_LIBRARY_RELEASE "${CURL_LIBRARY}" CACHE FILEPATH "macOS SDK curl release library" FORCE)
  set(CURL_LIBRARY_DEBUG "CURL_LIBRARY_DEBUG-NOTFOUND" CACHE FILEPATH "macOS SDK curl debug library" FORCE)
  find_package(CURL REQUIRED MODULE COMPONENTS HTTPS)
else()
  find_package(CURL ${curl_VERSION} REQUIRED CONFIG COMPONENTS HTTPS
    HINTS "${curl_PREFIX}/install"
    NO_DEFAULT_PATH)
endif()

if(CURL_DIR AND NOT CURL_DIR MATCHES "-NOTFOUND$")
  message(STATUS "Using CURL in ${CURL_DIR}")
else()
  message(STATUS "Using CURL ${CURL_VERSION_STRING}")
endif()

# GLFW: Open Source, multi-platform library for OpenGL, OpenGL ES and Vulkan application development
find_package(glfw3 ${glfw_VERSION} REQUIRED HINTS "${glfw_PREFIX}/install")

message(STATUS "Using GLFW in ${glfw3_DIR}")
set(glfw_INCLUDE_DIR "${glfw_PREFIX}/install/include"
  CACHE PATH "glfw include directory")

# OpenGL Mathematics (GLM): C++ mathematics library for graphics software based
# on the OpenGL Shading Language (GLSL) specifications
find_package(glm ${glm_VERSION} REQUIRED CONFIG
  HINTS "${glm_PREFIX}/install")

message(STATUS "Using GLM in ${glm_DIR}")

# IconFontCppHeaders: Headers for Font Awesome and Fork Awesome icons
set(iconfont_INCLUDE_DIR "${iconfont_PREFIX}/src"
  CACHE PATH "IconFontCppHeaders include directory")

# NanoVG: A small antialiased vector graphics rendering library for OpenGL
set(nanovg_SRC_DIR "${nanovg_PREFIX}/src/src")
add_library(nanovg STATIC "${nanovg_SRC_DIR}/nanovg.c")
target_include_directories(nanovg PRIVATE "${nanovg_SRC_DIR}")
entropy_disable_iwyu_for_targets(nanovg)

# Native File Dialog Extended
find_package(nfd REQUIRED
  HINTS "${nativefiledialog_PREFIX}/install")

message(STATUS "Using Native File Dialog Extended in ${nfd_DIR}")

# Niels Lohmann's JSON for Modern C++
find_package(nlohmann_json ${nlohmann_json_VERSION} REQUIRED
  HINTS "${nlohmann_json_PREFIX}/install")

message(STATUS "Using nlohmann_json in ${nlohmann_json_DIR}")

# Qt Core is used for QSharedMemory compatibility with ITK-SNAP IPC.
find_package(Qt6 ${qtbase_VERSION} REQUIRED COMPONENTS Core
  HINTS "${qtbase_PREFIX}/install/lib/cmake"
  NO_DEFAULT_PATH)

message(STATUS "Using Qt6Core in ${Qt6Core_DIR}")

# spdlog: Fast C++ logging library
find_package(spdlog ${spdlog_VERSION} REQUIRED HINTS "${spdlog_PREFIX}/install")

message(STATUS "Using spdlog in ${spdlog_DIR}")

# stduuid: A C++17 cross-platform single-header library implementation for universally
# unique identifiers
find_package(stduuid REQUIRED HINTS "${stduuid_PREFIX}/install")

message(STATUS "Using stduuid in ${stduuid_DIR}")

# TinyFSM: A simple, header-only C++ finite state machine library
set(tinyfsm_INCLUDE_DIR "${tinyfsm_PREFIX}/src/include"
  CACHE PATH "TinyFSM include directory")


# std::expected: C++23 vocabulary type for operations that return either a value or an error.
include(CheckCXXSourceCompiles)

check_cxx_source_compiles([=[
  #include <expected>
  #ifndef __cpp_lib_expected
  #error "std::expected not available"
  #endif
  int main() {
    std::expected<int,int> e(1);
    return *e;
  }
]=] ENTROPY_HAVE_STD_EXPECTED)

if(ENTROPY_HAVE_STD_EXPECTED)
  message(STATUS "Using std::expected")
else()
  message(FATAL_ERROR "std::expected is required. Use a C++23 standard library that provides <expected>.")
endif()



#--------------------------------------------------------------------------------
# GLAD library: Multi-Language GL/GLES/EGL/GLX/WGL Loader-Generator based on the official specs
#--------------------------------------------------------------------------------
# Entropy includes the loaders for OpenGL 3.3, 4.1, and 4.6 Core profiles for C,
# though only the loaders for version 3.3 are used in Entropy in order to assure
# compatibility across graphics hardware and platforms.

# This source code was generated by cloning the Git repository
# https://github.com/Dav1dde/glad.git and executing
# python3 -m glad --profile core --out-path glad --api gl=3.3 --generator c --spec gl
# (It can also be generated from the webservice at https://glad.dav1d.de/)

set(Entropy_GLAD_GL_VERSION "3.3" CACHE STRING "OpenGL Core profile version loaded by GLAD")
set(entropy_GLAD_GL_VERSION_VALUES "3.3;4.1;4.6")
set(Entropy_GLAD_GL_DEBUG false CACHE BOOL "Use OpenGL debugging in GLAD")

set_property(CACHE Entropy_GLAD_GL_VERSION PROPERTY STRINGS ${entropy_GLAD_GL_VERSION_VALUES})

string(REPLACE "." ";" entropy_GLAD_GL_VERSION_LIST ${Entropy_GLAD_GL_VERSION})
list(GET entropy_GLAD_GL_VERSION_LIST 0 entropy_GLAD_GL_VERSION_MAJOR)
list(GET entropy_GLAD_GL_VERSION_LIST 1 entropy_GLAD_GL_VERSION_MINOR)

set(glad_BASE glad_gl_${Entropy_GLAD_GL_VERSION}_core)

if(Entropy_GLAD_GL_DEBUG)
  set(glad_BASE ${glad_BASE}_debug)
endif()

set(glad_DIR "${entropy_EXT_DIR}/glad/${glad_BASE}")
set(glad_INCLUDE_DIR "${glad_DIR}/include")

add_library(glad STATIC "${glad_DIR}/src/glad.c")
target_include_directories(glad PRIVATE "${glad_INCLUDE_DIR}")
entropy_disable_iwyu_for_targets(glad)

#--------------------------------------------------------------------------------
# "Dear ImGui" library: Bloat-free graphical user interface library for C++.
# Outputs optimized vertex buffers to render anytime in 3D-pipeline enabled application.
# Fast, portable, renderer agnostic and self-contained (no external dependencies).
#--------------------------------------------------------------------------------
set(imgui_SRC_DIR "${imgui_PREFIX}/src")

list(APPEND imgui_SOURCES
  "${imgui_SRC_DIR}/imgui/imgui.cpp"
  "${imgui_SRC_DIR}/imgui/imgui_demo.cpp"
  "${imgui_SRC_DIR}/imgui/imgui_draw.cpp"
  "${imgui_SRC_DIR}/imgui/imgui_tables.cpp"
  "${imgui_SRC_DIR}/imgui/imgui_widgets.cpp"
  "${imgui_SRC_DIR}/imgui/misc/cpp/imgui_stdlib.cpp"
  "${imgui_SRC_DIR}/imgui/backends/imgui_impl_glfw.cpp" # GLFW bindings for ImGui
  "${imgui_SRC_DIR}/imgui/backends/imgui_impl_opengl3.cpp" # OpenGL 3 bindings for ImGui
)

add_library(imgui STATIC ${imgui_SOURCES})

target_compile_definitions(imgui PRIVATE IMGUI_USER_CONFIG="ui/entropy_imgui_config.h")

target_include_directories(imgui PRIVATE
  "${imgui_SRC_DIR}/imgui"
  "${entropy_LIB_DIR}" # needed for IMGUI_USER_CONFIG file
  "${glfw_INCLUDE_DIR}"
)

# used in IMGUI_USER_CONFIG
target_link_libraries(imgui PRIVATE
  glfw
  spdlog::spdlog
)

target_compile_options(imgui PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
    -Werror -Wall -Wextra -Wpointer-arith -Winit-self -Wunreachable-code
    -Wshadow -pedantic-errors -ftrapv
    -Wno-error=deprecated-copy
    -Wno-error=empty-body
    -Wno-empty-body # suppressed due to empty IM_ASSERT
  >
  $<$<CXX_COMPILER_ID:MSVC>:
    /W4
    /wd4390
  >
)

set_target_properties(imgui PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)
entropy_disable_iwyu_for_targets(imgui)

#--------------------------------------------------------------------------------
# ImPlot: Immediate mode, GPU accelerated plotting library for Dear ImGui
#--------------------------------------------------------------------------------
set(implot_SRC_DIR "${implot_PREFIX}/src")

list(APPEND IMPLOT_SOURCES
  "${implot_SRC_DIR}/implot/implot_demo.cpp"
  "${implot_SRC_DIR}/implot/implot_items.cpp"
  "${implot_SRC_DIR}/implot/implot.cpp"
)

add_library(implot STATIC ${IMPLOT_SOURCES})

target_compile_definitions(implot PRIVATE IMGUI_USER_CONFIG="ui/entropy_imgui_config.h")

target_include_directories(implot PRIVATE
  "${implot_SRC_DIR}/implot"
  "${imgui_SRC_DIR}/imgui"
  "${entropy_LIB_DIR}" # needed for IMGUI_USER_CONFIG file
)

# used in IMGUI_USER_CONFIG
target_link_libraries(implot PRIVATE
  glfw
  imgui
  spdlog::spdlog
)

target_compile_options(implot PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
    -Wall -Wextra -Wpointer-arith -Winit-self -Wunreachable-code -Wshadow -ftrapv
  >
  $<$<CXX_COMPILER_ID:GNU>:
    -Wno-format-truncation -Wno-deprecated-enum-enum-conversion
  >
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
    -Wno-deprecated-enum-enum-conversion
  >
  $<$<CXX_COMPILER_ID:MSVC>:
    /W4
    /wd4390
    /wd5054
  >
)

set_target_properties(implot PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)
entropy_disable_iwyu_for_targets(implot)

# Generate build stamp header after entropy_GLAD_GL_VERSION_MAJOR and entropy_GLAD_GL_VERSION_MINOR are defined
include(BuildStamp)
BuildStamp()

add_subdirectory("${entropy_LIB_DIR}/common")
add_subdirectory("${entropy_LIB_DIR}/image")
add_subdirectory("${entropy_LIB_DIR}/viewer")
add_subdirectory("${entropy_LIB_DIR}/layout")
add_subdirectory("${entropy_LIB_DIR}/registration")

#--------------------------------------------------------------------------------
# Entropy executable
#--------------------------------------------------------------------------------
set(entropy_SOURCES
  "${entropy_APP_DIR}/main.cpp"
  "${entropy_APP_DIR}/EntropyApp.cpp"
  "${entropy_APP_DIR}/EntropyAppCallbacks.cpp"
  "${entropy_APP_DIR}/EntropyAppLifecycle.cpp"
  "${entropy_APP_DIR}/EntropyAppProject.cpp"
  "${entropy_APP_DIR}/EntropyAppRegistration.cpp"

  "${entropy_APP_DIR}/logic/app/CallbackHandler.cpp"
  "${entropy_APP_DIR}/logic/app/Data.cpp"
  "${entropy_APP_DIR}/logic/app/DeformationWarp.cpp"
  "${entropy_APP_DIR}/logic/app/DataHelper.cpp"
  "${entropy_APP_DIR}/logic/app/ImageScaleInteraction.cpp"
  "${entropy_APP_DIR}/logic/app/ImageSelectionPolicy.cpp"
  "${entropy_APP_DIR}/logic/app/Logging.cpp"
  "${entropy_APP_DIR}/logic/app/LoadingStatusItems.cpp"
  "${entropy_APP_DIR}/logic/app/ParcellationLabelTable.cpp"
  "${entropy_APP_DIR}/logic/app/ProjectSnapshotComparison.cpp"
  "${entropy_APP_DIR}/logic/app/ProjectSnapshotSettings.cpp"
  "${entropy_APP_DIR}/logic/app/Settings.cpp"
  "${entropy_APP_DIR}/logic/app/StackTrace.cpp"
  "${entropy_APP_DIR}/logic/app/State.cpp"
  "${entropy_APP_DIR}/logic/app/UserPreferences.cpp"
  "${entropy_APP_DIR}/logic/app/UserPreferencesRenderData.cpp"
  "${entropy_APP_DIR}/logic/app/WindowTitleStatus.cpp"

  "${entropy_APP_DIR}/logic/annotation/Annotation.cpp"
  "${entropy_APP_DIR}/logic/annotation/BezierHelper.cpp"
  "${entropy_APP_DIR}/logic/annotation/LandmarkGroup.cpp"
  "${entropy_APP_DIR}/logic/annotation/SerializeAnnot.cpp"

  "${entropy_APP_DIR}/logic/camera/Camera.cpp"
  "${entropy_APP_DIR}/logic/camera/Camera3DControls.cpp"
  "${entropy_APP_DIR}/logic/camera/CameraFrustumSlice.cpp"
  "${entropy_APP_DIR}/logic/camera/Camera3DInteraction.cpp"
  "${entropy_APP_DIR}/logic/camera/CameraHelpers.cpp"
  "${entropy_APP_DIR}/logic/camera/CameraTypes.cpp"
  "${entropy_APP_DIR}/logic/camera/MathUtility.cpp"
  "${entropy_APP_DIR}/logic/camera/OrthogonalProjection.cpp"
  "${entropy_APP_DIR}/logic/camera/PerspectiveProjection.cpp"
  "${entropy_APP_DIR}/logic/camera/Projection.cpp"
  "${entropy_APP_DIR}/logic/camera/RaycastIsoSurfacePicker.cpp"

  "${entropy_APP_DIR}/logic/interaction/ViewHit.cpp"
  "${entropy_APP_DIR}/logic/interaction/events/ButtonState.cpp"

  "${entropy_APP_DIR}/logic/segmentation/AnnotationSegmentation.cpp"
  "${entropy_APP_DIR}/logic/segmentation/Poisson.cpp"
  "${entropy_APP_DIR}/logic/segmentation/SegHelpers.cpp"

  "${entropy_APP_DIR}/logic/serialization/ProjectSerialization.cpp"

  "${entropy_APP_DIR}/logic/states/annotation/AnnotationStateHelpers.cpp"
  "${entropy_APP_DIR}/logic/states/annotation/AnnotationStateMachine.cpp"
  "${entropy_APP_DIR}/logic/states/annotation/AnnotationStates.cpp"

  "${entropy_APP_DIR}/logic/DistanceMap.cpp"
  "${entropy_APP_DIR}/logic/SurfaceUtility.cpp"
  "${entropy_APP_DIR}/logic/sync/EntropyInstanceSync.cpp"
  "${entropy_APP_DIR}/logic/sync/EntropyInstanceSyncProtocol.cpp"
  "${entropy_APP_DIR}/logic/sync/ItkSnapSyncProtocol.cpp"
  "${entropy_APP_DIR}/logic/sync/ItkSnapSync.cpp"

  "${entropy_APP_DIR}/rendering/ascii/AsciiAtlas.cpp"
  "${entropy_APP_DIR}/rendering/ascii/AsciiAtlasBaker.cpp"
  "${entropy_APP_DIR}/rendering/ascii/AsciiClipboard.cpp"
  "${entropy_APP_DIR}/rendering/ascii/AsciiRenderer.cpp"
  "${entropy_APP_DIR}/rendering/ImageDrawing.cpp"
  "${entropy_APP_DIR}/rendering/geometry/PixelEdgeGeometry.cpp"
  "${entropy_APP_DIR}/rendering/geometry/ScaleBarGeometry.cpp"
  "${entropy_APP_DIR}/rendering/helpers/ImageDrawingHelpers.cpp"
  "${entropy_APP_DIR}/rendering/helpers/LightboxOffsetLabelFormat.cpp"
  "${entropy_APP_DIR}/rendering/helpers/PipelineHelpers.cpp"
  "${entropy_APP_DIR}/rendering/helpers/TextureSetupHelpers.cpp"
  "${entropy_APP_DIR}/rendering/helpers/VectorDrawingHelpers.cpp"
  "${entropy_APP_DIR}/rendering/metrics/LocalLinearResidualMetric.cpp"
  "${entropy_APP_DIR}/rendering/metrics/LocalNccMetric.cpp"
  "${entropy_APP_DIR}/rendering/physics/XrayAttenuation.cpp"
  "${entropy_APP_DIR}/rendering/PixelEdgeRenderer.cpp"
  "${entropy_APP_DIR}/rendering/BrushPreview.cpp"
  "${entropy_APP_DIR}/rendering/ColorImagePass.cpp"
  "${entropy_APP_DIR}/rendering/DeformationTextureBinding.cpp"
  "${entropy_APP_DIR}/rendering/GrayImagePass.cpp"
  "${entropy_APP_DIR}/rendering/ImagePasses.cpp"
  "${entropy_APP_DIR}/rendering/ImageViewOverlays.cpp"
  "${entropy_APP_DIR}/rendering/ImageTextureBinding.cpp"
  "${entropy_APP_DIR}/rendering/ImageUniforms.cpp"
  "${entropy_APP_DIR}/rendering/IsoContourPass.cpp"
  "${entropy_APP_DIR}/rendering/MetricPasses.cpp"
  "${entropy_APP_DIR}/rendering/MetricUniforms.cpp"
  "${entropy_APP_DIR}/rendering/Rendering.cpp"
  "${entropy_APP_DIR}/rendering/Raycast.cpp"
  "${entropy_APP_DIR}/rendering/RaycastIsoData.cpp"
  "${entropy_APP_DIR}/rendering/RaycastSelection.cpp"
  "${entropy_APP_DIR}/rendering/RaycastShaderPrograms.cpp"
  "${entropy_APP_DIR}/rendering/RaycastUniforms.cpp"
  "${entropy_APP_DIR}/rendering/SegmentationPass.cpp"
  "${entropy_APP_DIR}/rendering/ShaderInfoSetup.cpp"
  "${entropy_APP_DIR}/rendering/ShaderProgramSetup.cpp"
  "${entropy_APP_DIR}/rendering/ShaderPrograms.cpp"
  "${entropy_APP_DIR}/rendering/ShaderSourceSetup.cpp"
  "${entropy_APP_DIR}/rendering/ShaderUniformSetup.cpp"
  "${entropy_APP_DIR}/rendering/SegmentationTextureBinding.cpp"
  "${entropy_APP_DIR}/rendering/TextureBinding.cpp"
  "${entropy_APP_DIR}/rendering/TextureParameterUpdates.cpp"
  "${entropy_APP_DIR}/rendering/TextureUpdates.cpp"
  "${entropy_APP_DIR}/rendering/VectorImagePass.cpp"
  "${entropy_APP_DIR}/rendering/VectorFieldOverlays.cpp"
  "${entropy_APP_DIR}/rendering/ViewOverlays.cpp"
  "${entropy_APP_DIR}/rendering/RenderData.cpp"
  "${entropy_APP_DIR}/rendering/vector/FrustumOverlayDrawing.cpp"
  "${entropy_APP_DIR}/rendering/vector/LightboxOverlayDrawing.cpp"
  "${entropy_APP_DIR}/rendering/vector/ScaleBarDrawing.cpp"
  "${entropy_APP_DIR}/rendering/vector/VectorFieldOverlayDrawing.cpp"
  "${entropy_APP_DIR}/rendering/vector/ViewOverlayDrawing.cpp"
  "${entropy_APP_DIR}/rendering/TextureSetup.cpp"
  "${entropy_APP_DIR}/rendering/vector/VectorDrawing.cpp"

  "${entropy_APP_DIR}/rendering/common/ShaderType.cpp"

  "${entropy_APP_DIR}/rendering/utility/containers/VertexAttributeInfo.cpp"
  "${entropy_APP_DIR}/rendering/utility/containers/VertexIndicesInfo.cpp"
  "${entropy_APP_DIR}/rendering/utility/containers/Uniforms.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLBufferObject.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLBufferTexture.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLErrorChecker.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLFrameBufferObject.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLShader.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLShaderProgram.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLTexture.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLVersionChecker.cpp"
  "${entropy_APP_DIR}/rendering/utility/gl/GLVertexArrayObject.cpp"
  "${entropy_APP_DIR}/rendering/utility/math/SliceIntersector.cpp"

  "${entropy_APP_DIR}/windowing/ControlFrame.cpp"
  "${entropy_APP_DIR}/windowing/GlfwCallbacks.cpp"
  "${entropy_APP_DIR}/windowing/GlfwWrapper.cpp"
  "${entropy_APP_DIR}/windowing/Layout.cpp"
  "${entropy_APP_DIR}/windowing/LayoutSerialization.cpp"
  "${entropy_APP_DIR}/windowing/View.cpp"
  "${entropy_APP_DIR}/windowing/WindowData.cpp"

  "${entropy_EXT_DIR}/safeclib/strerrorlen_s.cpp"
)

set(entropy_ABOUT_ICON_SOURCE
  "${entropy_RES_DIR}/icons/Linux/hicolor/128x128/apps/io.github.adlerdh.entropy.png")
set(entropy_ABOUT_ICON_RESOURCE_PATH "res/icons/Linux/hicolor/128x128/apps/io.github.adlerdh.entropy.png")

if(Entropy_ENABLE_IWYU)
  set_source_files_properties(
    "${entropy_EXT_DIR}/safeclib/strerrorlen_s.cpp"
    PROPERTIES SKIP_LINTING ON
  )
endif()

if(APPLE)
  set(entropy_MACOS_APP_ICON "${entropy_RES_DIR}/icons/macOS/Entropy.icon")
  set(entropy_MACOS_APP_ICON_NAME "Entropy")

  list(APPEND entropy_SOURCES
    "${entropy_APP_DIR}/logic/app/AppPathsMac.mm"
  )
endif()

if(NOT APPLE)
  list(APPEND entropy_SOURCES
    "${entropy_APP_DIR}/logic/app/AppPaths.cpp"
  )
endif()

if(WIN32)
  set(entropy_WINDOWS_APP_ICON "${entropy_RES_DIR}/icons/Windows/Entropy.ico")
  set(entropy_WINDOWS_VERSION_RC "${CMAKE_CURRENT_BINARY_DIR}/EntropyWindowsVersion.rc")
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EntropyWindowsVersion.rc.in"
    "${entropy_WINDOWS_VERSION_RC}"
    @ONLY
  )

  list(APPEND entropy_SOURCES
    "${entropy_WINDOWS_VERSION_RC}"
  )
endif()

if(MSVC)
  set_source_files_properties(
    "${entropy_EXT_DIR}/imGuIZMO.quat/imGuIZMOquat.cpp"
    PROPERTIES COMPILE_OPTIONS "/wd4305"
  )
  set_source_files_properties(
    "${entropy_EXT_DIR}/imgui-knobs/imgui-knobs.cpp"
    PROPERTIES COMPILE_OPTIONS "/wd4244"
  )
endif()

set(entropy_SHADERS
  "${entropy_APP_DIR}/rendering/shaders/AsciiPost.vs"
  "${entropy_APP_DIR}/rendering/shaders/AsciiPost.fs"
  "${entropy_APP_DIR}/rendering/shaders/AsciiCellMean.fs"
  "${entropy_APP_DIR}/rendering/shaders/AsciiCellRegions.fs"
  "${entropy_APP_DIR}/rendering/shaders/AsciiPostSpatial.fs"
  "${entropy_APP_DIR}/rendering/shaders/Difference.fs"
  "${entropy_APP_DIR}/rendering/shaders/LocalNcc.fs"
  "${entropy_APP_DIR}/rendering/shaders/LocalLinearResidual.fs"
  "${entropy_APP_DIR}/rendering/shaders/Edge.fs"
  "${entropy_APP_DIR}/rendering/shaders/Image.vs"
  "${entropy_APP_DIR}/rendering/shaders/ImageWarped.vs"
  "${entropy_APP_DIR}/rendering/shaders/ImageGrey.fs"
  "${entropy_APP_DIR}/rendering/shaders/ImageColor.fs"
  "${entropy_APP_DIR}/rendering/shaders/VectorDirectionColor.fs"
  "${entropy_APP_DIR}/rendering/shaders/VectorSignedNormalProjection.fs"
  "${entropy_APP_DIR}/rendering/shaders/VectorPlanarProjectionColor.fs"
  "${entropy_APP_DIR}/rendering/shaders/VectorWarpedGrid.fs"
  "${entropy_APP_DIR}/rendering/shaders/IsoContour.fs"
  "${entropy_APP_DIR}/rendering/shaders/Metric.vs"
  "${entropy_APP_DIR}/rendering/shaders/MetricWarped.vs"
  "${entropy_APP_DIR}/rendering/shaders/Overlay.fs"
  "${entropy_APP_DIR}/rendering/shaders/PixelEdgePost.fs"
  "${entropy_APP_DIR}/rendering/shaders/RaycastIso.fs"
  "${entropy_APP_DIR}/rendering/shaders/RaycastIso.vs"
  "${entropy_APP_DIR}/rendering/shaders/Seg.fs"
  "${entropy_APP_DIR}/rendering/shaders/Seg.vs"
  "${entropy_APP_DIR}/rendering/shaders/SegWarped.vs"
  "${entropy_APP_DIR}/rendering/shaders/Simple.fs"
  "${entropy_APP_DIR}/rendering/shaders/Simple.vs"
  "${entropy_APP_DIR}/rendering/shaders/Xray.fs"
  "${entropy_APP_DIR}/rendering/shaders/functions/AsciiComposite.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/ColorHelpers.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/DoRender.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/ComputeEdge_FreiChen.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/ComputeEdge_Sobel.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/Helpers.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/IntensityProjection.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/MetricSampling_Deformation.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/MetricSampling_Identity.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/SegInteriorAlpha_NoOutline.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/SegInteriorAlpha_WithOutline.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/SegValue_Linear.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/SegValue_Nearest.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/SampleTexCoord_Deformation.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/SampleTexCoord_Identity.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/TextureLookup_Cubic.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/TextureLookup_Cubic_2D.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/TextureLookup_Linear.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/TextureLookup_Linear_2D.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/TextureLookup_FloatingPoint_Linear.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/TextureLookup_FloatingPoint_Linear_2D.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/UIntTextureLookup_Linear.glsl"
  "${entropy_APP_DIR}/rendering/shaders/functions/UIntTextureLookup_Linear_2D.glsl"
)

file(GLOB entropy_COLORMAPS
  "${entropy_RES_DIR}/colormaps/ncl/*.csv"
  "${entropy_RES_DIR}/colormaps/matplotlib/*.csv"
  "${entropy_RES_DIR}/colormaps/peter_kovesi/*.csv"
)

set(entropy_FONTS
  "${entropy_RES_DIR}/fonts/Cousine/Cousine-Regular.ttf"
  "${entropy_RES_DIR}/fonts/ForkAwesome/forkawesome-webfont.ttf"
  "${entropy_RES_DIR}/fonts/IBMPlexSans/IBMPlexSans-Regular.ttf"
  "${entropy_RES_DIR}/fonts/Inter/Inter-Regular.ttf"
  "${entropy_RES_DIR}/fonts/Inter/Inter-Bold.ttf"
  "${entropy_RES_DIR}/fonts/NotoSans/NotoSans-Regular.ttf"
  "${entropy_RES_DIR}/fonts/Roboto/Roboto-Light.ttf"
  "${entropy_RES_DIR}/fonts/Roboto/Roboto-Regular.ttf"
  "${entropy_RES_DIR}/fonts/Satoshi/Satoshi-Light.ttf"
  "${entropy_RES_DIR}/fonts/SourceSans3/SourceSans3-Regular.ttf"
  "${entropy_RES_DIR}/fonts/SpaceGrotesk/SpaceGrotesk-Light.ttf"
  "${entropy_RES_DIR}/fonts/Supreme/Supreme-Light.ttf"
)

set(entropy_LICENSE_RESOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_licenses")
file(MAKE_DIRECTORY "${entropy_LICENSE_RESOURCE_DIR}/licenses")

set(entropy_LICENSES)
macro(entropy_add_license_resource resource_name source_path)
  configure_file("${source_path}" "${entropy_LICENSE_RESOURCE_DIR}/licenses/${resource_name}" COPYONLY)
  list(APPEND entropy_LICENSES "${entropy_LICENSE_RESOURCE_DIR}/licenses/${resource_name}")
endmacro()

entropy_add_license_resource("CLI11.txt" "${cli11_PREFIX}/src/LICENSE")
entropy_add_license_resource("CMakeRC.txt" "${cmakerc_PREFIX}/src/LICENSE.txt")
entropy_add_license_resource("curl.txt" "${entropy_RES_DIR}/licenses/curl.txt")
entropy_add_license_resource("GLFW.txt" "${glfw_PREFIX}/src/LICENSE.md")
entropy_add_license_resource("GLM.txt" "${glm_PREFIX}/src/copying.txt")
entropy_add_license_resource("IconFontCppHeaders.txt" "${entropy_RES_DIR}/licenses/IconFontCppHeaders-Zlib.txt")
entropy_add_license_resource("DearImGui.txt" "${entropy_RES_DIR}/licenses/DearImGui-MIT.txt")
entropy_add_license_resource("ImPlot.txt" "${implot_PREFIX}/src/implot/LICENSE")
entropy_add_license_resource("Apache-2.0.txt" "${itk_PREFIX}/src/LICENSE")
entropy_add_license_resource("ITK-NOTICE.txt" "${itk_PREFIX}/src/NOTICE")
entropy_add_license_resource("NanoVG.txt" "${nanovg_PREFIX}/src/LICENSE.txt")
entropy_add_license_resource("NativeFileDialogExtended.txt" "${nativefiledialog_PREFIX}/src/LICENSE")
entropy_add_license_resource("nlohmann-json.txt" "${nlohmann_json_PREFIX}/src/LICENSE.MIT")
entropy_add_license_resource("Qt-LGPL-3.0-only.txt" "${qtbase_PREFIX}/src/LICENSES/LGPL-3.0-only.txt")
entropy_add_license_resource("CC0-1.0.txt" "${qtbase_PREFIX}/src/LICENSES/CC0-1.0.txt")
entropy_add_license_resource("spdlog.txt" "${spdlog_PREFIX}/src/LICENSE")
entropy_add_license_resource("stduuid.txt" "${stduuid_PREFIX}/src/LICENSE")
entropy_add_license_resource("TinyFSM.txt" "${tinyfsm_PREFIX}/src/COPYING")
entropy_add_license_resource("GLAD.txt" "${entropy_EXT_DIR}/glad/LICENSE.txt")
entropy_add_license_resource("imGuIZMO.quat.txt" "${entropy_EXT_DIR}/imGuIZMO.quat/license.txt")
entropy_add_license_resource("imgui-knobs.txt" "${entropy_EXT_DIR}/imgui-knobs/LICENSE")
entropy_add_license_resource("PNPOLY.txt" "${entropy_EXT_DIR}/wrfranklin/LICENSE.txt")
entropy_add_license_resource("SafeCLib.txt" "${entropy_EXT_DIR}/safeclib/LICENSE.txt")
entropy_add_license_resource("CubicBSplineInterpolation.txt" "${entropy_APP_DIR}/rendering/shaders/functions/TricubicReadme.txt")
entropy_add_license_resource("Cousine.txt" "${entropy_RES_DIR}/fonts/Cousine/LICENSE.txt")
entropy_add_license_resource("ForkAwesome.txt" "${entropy_RES_DIR}/fonts/ForkAwesome/LICENSES.txt")
entropy_add_license_resource("IBMPlexSans.txt" "${entropy_RES_DIR}/fonts/IBMPlexSans/LICENSE.txt")
entropy_add_license_resource("Inter.txt" "${entropy_RES_DIR}/fonts/Inter/LICENSE.txt")
entropy_add_license_resource("NotoSans.txt" "${entropy_RES_DIR}/fonts/NotoSans/LICENSE.txt")
entropy_add_license_resource("Roboto.txt" "${entropy_RES_DIR}/fonts/Roboto/LICENSE.txt")
entropy_add_license_resource("Satoshi.txt" "${entropy_RES_DIR}/fonts/Satoshi/FFL.txt")
entropy_add_license_resource("SourceSans3.txt" "${entropy_RES_DIR}/fonts/SourceSans3/LICENSE.txt")
entropy_add_license_resource("SpaceGrotesk.txt" "${entropy_RES_DIR}/fonts/SpaceGrotesk/OFL.txt")
entropy_add_license_resource("Supreme.txt" "${entropy_RES_DIR}/fonts/Supreme/FFL.txt")
entropy_add_license_resource("PeterKovesiColourMaps.txt" "${entropy_RES_DIR}/colormaps/peter_kovesi/LICENSE.txt")
entropy_add_license_resource("MatplotlibColourMaps.txt" "${entropy_RES_DIR}/colormaps/matplotlib/LICENSE.txt")
entropy_add_license_resource("Cividis.txt" "${entropy_RES_DIR}/colormaps/ncl/LICENSE.txt")

if(APPLE)
  include_directories(/System/Library/Frameworks)

  find_library(COCOA_LIBRARY Cocoa)
  find_library(OpenGL_LIBRARY OpenGL)
  find_library(IOKit_LIBRARY IOKit)
  find_library(CoreVideo_LIBRARY CoreVideo)

  mark_as_advanced(COCOA_LIBRARY OpenGL_LIBRARY)
endif()

# Using CMakeRC, embed the resources into static libraries
cmrc_add_resource_library(entropy-colormaps ALIAS entropy::colormaps ${entropy_COLORMAPS} NAMESPACE colormaps)
cmrc_add_resource_library(entropy-fonts ALIAS entropy::fonts ${entropy_FONTS} NAMESPACE fonts)
cmrc_add_resource_library(entropy-licenses ALIAS entropy::licenses ${entropy_LICENSES} NAMESPACE licenses
  WHENCE "${entropy_LICENSE_RESOURCE_DIR}"
)
cmrc_add_resource_library(entropy-shaders ALIAS entropy::shaders ${entropy_SHADERS} NAMESPACE shaders
  WHENCE "${entropy_APP_DIR}"
  PREFIX "app"
)
cmrc_add_resource_library(entropy-icons ALIAS entropy::icons "${entropy_ABOUT_ICON_SOURCE}" NAMESPACE icons
  WHENCE "${CMAKE_CURRENT_SOURCE_DIR}"
)
entropy_disable_iwyu_for_targets(
  entropy-colormaps
  entropy-fonts
  entropy-icons
  entropy-licenses
  entropy-shaders
)

add_subdirectory("${entropy_LIB_DIR}/ui")

if(APPLE)
  add_executable(entropy MACOSX_BUNDLE)
else()
  add_executable(entropy)
endif()

if(APPLE)
  add_custom_command(
    TARGET entropy
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}"
      -D "entropy_BUNDLE_DIR=$<TARGET_BUNDLE_DIR:entropy>"
      -D "entropy_BUNDLE_EXECUTABLE_NAME=${APP_NAME}"
      -D "entropy_CLI_EXECUTABLE_NAME=entropy"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NormalizeMacOSBundleExecutable.cmake"
    COMMAND
      "${CMAKE_COMMAND}"
      -D "entropy_BUNDLE_DIR=$<TARGET_BUNDLE_DIR:entropy>"
      -D "entropy_SOURCE_ICON=${entropy_MACOS_APP_ICON}"
      -D "entropy_ICON_NAME=${entropy_MACOS_APP_ICON_NAME}"
      -D "entropy_MINIMUM_SYSTEM_VERSION=${Entropy_MACOSX_BUNDLE_MINIMUM_SYSTEM_VERSION}"
      -D "entropy_CODESIGN_IDENTITY=${Entropy_MACOS_CODESIGN_IDENTITY}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RefreshMacOSIcon.cmake"
    COMMAND
      "${CMAKE_COMMAND}" -E rm -f
      "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${APP_NAME}"
    COMMAND
      "${CMAKE_COMMAND}" -E copy_if_different
      "$<TARGET_BUNDLE_DIR:entropy>/Contents/MacOS/entropy"
      "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/entropy"
    COMMAND
      "${CMAKE_COMMAND}" -E make_directory
      "$<TARGET_BUNDLE_DIR:entropy>/Contents/Resources/python"
    COMMAND
      "${CMAKE_COMMAND}" -E copy_directory
      "${entropy_FIREANTS_BRIDGE_DIR}"
      "$<TARGET_BUNDLE_DIR:entropy>/Contents/Resources/python/fireants_bridge"
    VERBATIM
  )
endif()

target_sources(entropy PRIVATE
  ${entropy_SOURCES}
)

target_compile_definitions(entropy PRIVATE
  ENTROPY_ABOUT_ICON_RESOURCE_PATH="${entropy_ABOUT_ICON_RESOURCE_PATH}"
  ENTROPY_RESOURCE_DIR="${CMAKE_INSTALL_FULL_DATADIR}/entropy"
)

target_link_libraries(entropy PRIVATE
  CLI11::CLI11
  Entropy::Common
  Entropy::Image
  Entropy::Layout
  Entropy::UI
  Entropy::Viewer
  glfw
  glad
  glm::glm
  imgui
  implot
  nanovg
  nfd::nfd
  nlohmann_json::nlohmann_json
  Qt6::Core
  stduuid
  entropy::colormaps
  entropy::fonts
  entropy::icons
  entropy::licenses
  entropy::shaders
  entropy_warnings
)

entropy_enable_coverage_for_target(entropy)

if(APPLE)
  target_link_libraries(entropy PRIVATE
    ${COCOA_LIBRARY}
  )
endif()

if(WIN32)
  set_target_properties(entropy PROPERTIES
    WIN32_EXECUTABLE "$<$<NOT:$<CONFIG:Debug>>:TRUE>"
  )

  if(MSVC)
    target_link_options(entropy PRIVATE
      "/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'")
  endif()

  target_link_libraries(entropy PRIVATE
    comctl32
    shell32
    user32
    ws2_32
  )
endif()

target_include_directories(entropy PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}" # for BuildStamp.h
  "${entropy_APP_DIR}"
  "${entropy_LIB_DIR}"
  "${entropy_RES_DIR}"
  "${glad_INCLUDE_DIR}"
  "${iconfont_INCLUDE_DIR}"
  "${tinyfsm_INCLUDE_DIR}"
)

# Let IDEs index imported package include roots as project code; keep warning-prone vendored headers as system includes below.
set_target_properties(entropy PROPERTIES NO_SYSTEM_FROM_IMPORTED ON)

target_include_directories(entropy SYSTEM PRIVATE
  "${imgui_SRC_DIR}"
  "${imgui_SRC_DIR}/imgui" # due to ImPlot
  "${entropy_EXT_DIR}"
  "${entropy_EXT_DIR}/imGuIZMO.quat"
  "${entropy_EXT_DIR}/imgui-knobs"
  "${implot_SRC_DIR}"
  "${nanovg_SRC_DIR}"
)

entropy_copy_runtime_dlls(entropy)

target_compile_options(entropy PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
    -Werror
    -Wall -Wextra -Wpointer-arith -Winit-self -Wunreachable-code -Wshadow
    # -Wconversion -Wsign-conversion
    -Wno-error=array-bounds
    -Wno-error=empty-body
    -Wno-error=float-conversion
    -Wno-error=maybe-uninitialized
    -Wno-error=stringop-overflow
    -ftrapv
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

set_target_properties(entropy PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

if(BUILD_TESTING)
  add_subdirectory(app/logic/app/test)
  add_subdirectory(app/logic/camera/test)
  add_subdirectory(app/logic/interaction/test)
  add_subdirectory(app/logic/serialization/test)
  add_subdirectory(app/logic/sync/test)
  add_subdirectory(app/rendering/test)
  add_subdirectory(app/windowing/test)
  add_subdirectory(test/image_generator)
  add_subdirectory(test/warp_field_generator)
endif()

entropy_add_coverage_targets()

include(Packaging)
