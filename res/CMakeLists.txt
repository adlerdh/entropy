file(GLOB entropy_COLORMAPS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/colormaps/ncl/*.csv"
  "${CMAKE_CURRENT_SOURCE_DIR}/colormaps/matplotlib/*.csv"
  "${CMAKE_CURRENT_SOURCE_DIR}/colormaps/peter_kovesi/*.csv"
)

set(entropy_FONTS
  "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Cousine/Cousine-Regular.ttf"
  "${CMAKE_CURRENT_SOURCE_DIR}/fonts/ForkAwesome/forkawesome-webfont.ttf"
  "${CMAKE_CURRENT_SOURCE_DIR}/fonts/IBMPlexSans/IBMPlexSans-Regular.ttf"
  "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Inter/Inter-Regular.ttf"
  "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Inter/Inter-Bold.ttf"
  "${CMAKE_CURRENT_SOURCE_DIR}/fonts/NotoSans/NotoSans-Regular.ttf"
  "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Roboto/Roboto-Light.ttf"
  "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Roboto/Roboto-Regular.ttf"
  "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Satoshi/Satoshi-Light.ttf"
  "${CMAKE_CURRENT_SOURCE_DIR}/fonts/SourceSans3/SourceSans3-Regular.ttf"
  "${CMAKE_CURRENT_SOURCE_DIR}/fonts/SpaceGrotesk/SpaceGrotesk-Light.ttf"
  "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Supreme/Supreme-Light.ttf"
)

set(entropy_LICENSE_RESOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_licenses")
file(MAKE_DIRECTORY "${entropy_LICENSE_RESOURCE_DIR}/licenses")

function(entropy_add_license_resource output_list resource_name source_path)
  configure_file("${source_path}" "${entropy_LICENSE_RESOURCE_DIR}/licenses/${resource_name}" COPYONLY)
  set(resources ${${output_list}})
  list(APPEND resources "${entropy_LICENSE_RESOURCE_DIR}/licenses/${resource_name}")
  set(${output_list} ${resources} PARENT_SCOPE)
endfunction()

set(entropy_LICENSES)
entropy_add_license_resource(entropy_LICENSES "Apache-2.0.txt" "${itk_PREFIX}/src/LICENSE")
entropy_add_license_resource(entropy_LICENSES "CC0-1.0.txt" "${qtbase_PREFIX}/src/LICENSES/CC0-1.0.txt")
entropy_add_license_resource(entropy_LICENSES "CLI11.txt" "${cli11_PREFIX}/src/LICENSE")
entropy_add_license_resource(entropy_LICENSES "CMakeRC.txt" "${cmakerc_PREFIX}/src/LICENSE.txt")
entropy_add_license_resource(entropy_LICENSES "curl.txt" "${CMAKE_CURRENT_SOURCE_DIR}/licenses/curl.txt")
entropy_add_license_resource(entropy_LICENSES "DearImGui.txt" "${CMAKE_CURRENT_SOURCE_DIR}/licenses/DearImGui-MIT.txt")
entropy_add_license_resource(entropy_LICENSES "GLFW.txt" "${glfw_PREFIX}/src/LICENSE.md")
entropy_add_license_resource(entropy_LICENSES "GLM.txt" "${glm_PREFIX}/src/copying.txt")
entropy_add_license_resource(entropy_LICENSES "IconFontCppHeaders.txt" "${CMAKE_CURRENT_SOURCE_DIR}/licenses/IconFontCppHeaders-Zlib.txt")
entropy_add_license_resource(entropy_LICENSES "ImPlot.txt" "${implot_PREFIX}/src/implot/LICENSE")
entropy_add_license_resource(entropy_LICENSES "ITK-NOTICE.txt" "${itk_PREFIX}/src/NOTICE")
entropy_add_license_resource(entropy_LICENSES "NanoVG.txt" "${nanovg_PREFIX}/src/LICENSE.txt")
entropy_add_license_resource(entropy_LICENSES "NativeFileDialogExtended.txt" "${nativefiledialog_PREFIX}/src/LICENSE")
entropy_add_license_resource(entropy_LICENSES "nlohmann-json.txt" "${nlohmann_json_PREFIX}/src/LICENSE.MIT")
entropy_add_license_resource(entropy_LICENSES "Qt-LGPL-3.0-only.txt" "${qtbase_PREFIX}/src/LICENSES/LGPL-3.0-only.txt")
entropy_add_license_resource(entropy_LICENSES "spdlog.txt" "${spdlog_PREFIX}/src/LICENSE")
entropy_add_license_resource(entropy_LICENSES "stduuid.txt" "${stduuid_PREFIX}/src/LICENSE")
entropy_add_license_resource(entropy_LICENSES "TinyFSM.txt" "${tinyfsm_PREFIX}/src/COPYING")
entropy_add_license_resource(entropy_LICENSES "VTK.txt" "${vtk_PREFIX}/src/Copyright.txt")

entropy_add_license_resource(entropy_LICENSES "GLAD.txt" "${entropy_EXT_DIR}/glad/LICENSE.txt")
entropy_add_license_resource(entropy_LICENSES "imGuIZMO.quat.txt" "${entropy_EXT_DIR}/imGuIZMO.quat/license.txt")
entropy_add_license_resource(entropy_LICENSES "imgui-knobs.txt" "${entropy_EXT_DIR}/imgui-knobs/LICENSE")
entropy_add_license_resource(entropy_LICENSES "PNPOLY.txt" "${entropy_EXT_DIR}/wrfranklin/LICENSE.txt")
entropy_add_license_resource(entropy_LICENSES "SafeCLib.txt" "${entropy_EXT_DIR}/safeclib/LICENSE.txt")
entropy_add_license_resource(entropy_LICENSES "CubicBSplineInterpolation.txt"
  "${entropy_APP_DIR}/rendering/shaders/functions/TricubicReadme.txt")

entropy_add_license_resource(entropy_LICENSES "Cousine.txt" "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Cousine/LICENSE.txt")
entropy_add_license_resource(entropy_LICENSES "ForkAwesome.txt" "${CMAKE_CURRENT_SOURCE_DIR}/fonts/ForkAwesome/LICENSES.txt")
entropy_add_license_resource(entropy_LICENSES "IBMPlexSans.txt" "${CMAKE_CURRENT_SOURCE_DIR}/fonts/IBMPlexSans/LICENSE.txt")
entropy_add_license_resource(entropy_LICENSES "Inter.txt" "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Inter/LICENSE.txt")
entropy_add_license_resource(entropy_LICENSES "NotoSans.txt" "${CMAKE_CURRENT_SOURCE_DIR}/fonts/NotoSans/LICENSE.txt")
entropy_add_license_resource(entropy_LICENSES "Roboto.txt" "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Roboto/LICENSE.txt")
entropy_add_license_resource(entropy_LICENSES "Satoshi.txt" "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Satoshi/FFL.txt")
entropy_add_license_resource(entropy_LICENSES "SourceSans3.txt" "${CMAKE_CURRENT_SOURCE_DIR}/fonts/SourceSans3/LICENSE.txt")
entropy_add_license_resource(entropy_LICENSES "SpaceGrotesk.txt" "${CMAKE_CURRENT_SOURCE_DIR}/fonts/SpaceGrotesk/OFL.txt")
entropy_add_license_resource(entropy_LICENSES "Supreme.txt" "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Supreme/FFL.txt")
entropy_add_license_resource(entropy_LICENSES "PeterKovesiColourMaps.txt"
  "${CMAKE_CURRENT_SOURCE_DIR}/colormaps/peter_kovesi/LICENSE.txt")
entropy_add_license_resource(entropy_LICENSES "MatplotlibColourMaps.txt"
  "${CMAKE_CURRENT_SOURCE_DIR}/colormaps/matplotlib/LICENSE.txt")
entropy_add_license_resource(entropy_LICENSES "Cividis.txt"
  "${CMAKE_CURRENT_SOURCE_DIR}/colormaps/ncl/LICENSE.txt")

set(entropy_ABOUT_ICON_SOURCE
  "${CMAKE_SOURCE_DIR}/${entropy_ABOUT_ICON_RESOURCE_PATH}")
if(NOT EXISTS "${entropy_ABOUT_ICON_SOURCE}")
  message(FATAL_ERROR "About dialog icon not found: ${entropy_ABOUT_ICON_SOURCE}")
endif()

cmrc_add_resource_library(entropy-colormaps ALIAS entropy::colormaps ${entropy_COLORMAPS} NAMESPACE colormaps
  WHENCE "${CMAKE_SOURCE_DIR}")
cmrc_add_resource_library(entropy-fonts ALIAS entropy::fonts ${entropy_FONTS} NAMESPACE fonts
  WHENCE "${CMAKE_SOURCE_DIR}")
cmrc_add_resource_library(entropy-licenses ALIAS entropy::licenses ${entropy_LICENSES} NAMESPACE licenses
  WHENCE "${entropy_LICENSE_RESOURCE_DIR}")
cmrc_add_resource_library(entropy-icons ALIAS entropy::icons "${entropy_ABOUT_ICON_SOURCE}" NAMESPACE icons
  WHENCE "${CMAKE_SOURCE_DIR}")

entropy_disable_linting_for_targets(entropy-colormaps entropy-fonts entropy-icons entropy-licenses)
