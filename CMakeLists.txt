cmake_minimum_required(VERSION 3.28.0 FATAL_ERROR)

if(POLICY CMP0175)
  cmake_policy(SET CMP0175 OLD)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ProjectMetadata)
project(Entropy VERSION ${VERSION_FULL} LANGUAGES C CXX)

if(APPLE)
  enable_language(OBJCXX)
endif()

include(GNUInstallDirs)
include(CTest)
include(Coverage)
include(DeveloperTools)
include(BuildConfiguration)
include(DependencyVersions)
include(RuntimeDeployment)

option(Entropy_SUPERBUILD "Run the SuperBuild" ON)
if(Entropy_SUPERBUILD)
  get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  include(SuperBuild)
  return()
endif()

include(CompilerWarnings)
entropy_configure_coverage()
entropy_add_cppcheck_target()
include(Dependencies)

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

  include_directories(/System/Library/Frameworks)
  find_library(COCOA_LIBRARY Cocoa REQUIRED)
  find_library(OpenGL_LIBRARY OpenGL)
  find_library(IOKit_LIBRARY IOKit)
  find_library(CoreVideo_LIBRARY CoreVideo)
  mark_as_advanced(COCOA_LIBRARY OpenGL_LIBRARY)
endif()

if(MSVC)
  set_source_files_properties(
    "${entropy_EXT_DIR}/imGuIZMO.quat/imGuIZMOquat.cpp"
    PROPERTIES COMPILE_OPTIONS "/wd4305")
  set_source_files_properties(
    "${entropy_EXT_DIR}/imgui-knobs/imgui-knobs.cpp"
    PROPERTIES COMPILE_OPTIONS "/wd4244")
endif()

add_subdirectory(external)

# The build stamp records the configured OpenGL profile established by the GLAD target.
include(BuildStamp)
BuildStamp()

add_subdirectory(lib/common)
add_subdirectory(lib/image)
add_subdirectory(lib/viewer)
add_subdirectory(lib/layout)
add_subdirectory(lib/registration)
add_subdirectory(res)
add_subdirectory(lib/ui)
add_subdirectory(app)

entropy_add_cppcheck_generated_dependencies(
  entropy-colormaps
  entropy-fonts
  entropy-icons
  entropy-licenses
  entropy-shaders
)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

entropy_add_coverage_targets()
include(Packaging)
