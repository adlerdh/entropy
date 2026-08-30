# Versions are centralized because both the SuperBuild and application build consume them.
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
set(vtk_VERSION "9.6.2")

include(ItkComponents)

set(EXTERNAL_DIR "${CMAKE_BINARY_DIR}/external")
foreach(dependency IN ITEMS
    catch2
    cli11
    cmakerc
    curl
    glfw
    glm
    iconfont
    imgui
    implot
    itk
    nanovg
    nativefiledialog
    nlohmann_json
    qtbase
    spdlog
    stduuid
    tinyfsm
    vtk)
  set(${dependency}_PREFIX "${EXTERNAL_DIR}/${dependency}-${${dependency}_VERSION}")
endforeach()
