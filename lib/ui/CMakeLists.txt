set(entropy_UI_SOURCES
  ui/AboutIcon.cpp
  ui/DicomMetadataTable.cpp
  ui/ExportJobService.cpp
  ui/dialogs/InputLoadErrorDialog.cpp
  ui/dialogs/InputLoadErrorDialogNative.cpp
  ui/dialogs/NativeMessageDialogModel.cpp
  ui/dialogs/WarpFieldAssignment.cpp
  ui/GradientBackgroundRenderer.cpp
  ui/GuiData.cpp
  ui/headers/AnnotationHeader.cpp
  ui/headers/ImageHeader.cpp
  ui/headers/ImageHeaderInformation.cpp
  ui/headers/LandmarkHeader.cpp
  ui/headers/SegmentationHeader.cpp
  ui/Helpers.cpp
  ui/ImageExport.cpp
  ui/IsosurfaceRangeModel.cpp
  ui/headers/IsosurfaceHeader.cpp
  ui/ImGuiCustomControls.cpp
  ui/ImGuiWrapper.cpp
  ui/LinuxUiScale.cpp
  ui/menus/MainMenuBar.cpp
  ui/NativeFileDialogs.cpp
  ui/RegionStatisticsExport.cpp
  ui/popups/AboutPopup.cpp
  ui/popups/AddLayoutPopup.cpp
  ui/popups/ConfirmClosePopup.cpp
  ui/popups/ConfirmRemoveImagePopup.cpp
  ui/popups/ConfirmSetReferenceImagePopup.cpp
  ui/popups/DicomFolderPathPopup.cpp
  ui/popups/DicomSeriesSelectionPopup.cpp
  ui/popups/LargeImageLoadPromptPopup.cpp
  ui/popups/RasterImageHeaderPromptPopup.cpp
  ui/popups/UnsavedProjectPopup.cpp
  ui/Style.cpp
  ui/settings/SettingsModel.cpp
  ui/ThirdPartyLicenses.cpp
  ui/toolbars/AnnotationToolbar.cpp
  ui/toolbars/ModeToolbar.cpp
  ui/toolbars/SegmentationToolbar.cpp
  ui/UiScaleManager.cpp
  ui/updates/UpdateCheck.cpp
  ui/widgets/ActiveImageSelection.cpp
  ui/widgets/ActiveImageSelectionModel.cpp
  ui/widgets/LandmarkWidget.cpp
  ui/widgets/PaletteWidget.cpp
  ui/widgets/SegmentationLabelsWidget.cpp
  ui/windows/AnnotationWindow.cpp
  ui/windows/ExportStatusWindow.cpp
  ui/windows/ImagePropertiesWindow.cpp
  ui/windows/InspectionTableWindow.cpp
  ui/windows/InspectionWindow.cpp
  ui/windows/InspectionWindowSizing.cpp
  ui/windows/IsosurfacesWindow.cpp
  ui/windows/KeyboardShortcuts.cpp
  ui/windows/LandmarkPropertiesWindow.cpp
  ui/windows/LoadingStatusModel.cpp
  ui/windows/OpacityMixerModel.cpp
  ui/windows/OpacityMixerWindow.cpp
  ui/windows/RegistrationWindow.cpp
  ui/windows/RegionStatisticsWindow.cpp
  ui/windows/RegionStatisticsController.cpp
  ui/windows/SegmentationPropertiesWindow.cpp
  ui/windows/SettingsWindow.cpp
  ui/windows/ViewOverlayModel.cpp
  ui/windows/ViewOverlayWindows.cpp

  "${entropy_EXT_DIR}/imGuIZMO.quat/imGuIZMOquat.cpp"
  "${entropy_EXT_DIR}/imGuIZMO.quat/ImGuiPaletteButton.cpp"
  "${entropy_EXT_DIR}/imgui-knobs/imgui-knobs.cpp"
)

if(APPLE)
  list(APPEND entropy_UI_SOURCES
    ui/ClipboardMac.mm
    ui/dialogs/NativeMessageDialogsMac.mm
    ui/menus/MacNativeMainMenu.mm
  )
elseif(WIN32)
  list(APPEND entropy_UI_SOURCES
    ui/ClipboardWin.cpp
    ui/dialogs/NativeMessageDialogsWin.cpp
    ui/menus/WinNativeMainMenu.cpp
  )
else()
  list(APPEND entropy_UI_SOURCES
    ui/Clipboard.cpp
    ui/dialogs/NativeMessageDialogs.cpp
  )
endif()

add_library(EntropyUI STATIC ${entropy_UI_SOURCES})
add_library(Entropy::UI ALIAS EntropyUI)

target_sources(EntropyUI
  PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
      ui/AboutIcon.h
      ui/Clipboard.h
      ui/DicomMetadataTable.h
      ui/ExportJobService.h
      ui/GradientBackgroundRenderer.h
      ui/GuiData.h
      ui/Helpers.h
      ui/ImGuiCustomControls.h
      ui/ImGuiWrapper.h
      ui/ImageExport.h
      ui/IsosurfaceRangeModel.h
      ui/LinuxUiScale.h
      ui/NativeFileDialogs.h
      ui/RegionStatisticsExport.h
      ui/Scaling.h
      ui/Style.h
      ui/ThirdPartyLicenses.h
      ui/UiControls.h
      ui/UiScaleManager.h
      ui/dialogs/InputLoadErrorDialog.h
      ui/dialogs/NativeMessageDialogModel.h
      ui/dialogs/NativeMessageDialogs.h
      ui/dialogs/WarpFieldAssignment.h
      ui/entropy_imgui_config.h
      ui/headers/AnnotationHeader.h
      ui/headers/HeaderCommon.h
      ui/headers/Headers.h
      ui/headers/ImageHeader.h
      ui/headers/IsosurfaceHeader.h
      ui/headers/LandmarkHeader.h
      ui/headers/SegmentationHeader.h
      ui/menus/MacNativeMainMenu.h
      ui/menus/MainMenuBar.h
      ui/menus/WinNativeMainMenu.h
      ui/popups/AboutPopup.h
      ui/popups/AddLayoutPopup.h
      ui/popups/ConfirmationPopups.h
      ui/popups/DicomPopups.h
      ui/popups/LargeImageLoadPromptPopup.h
      ui/popups/PopupCommon.h
      ui/popups/Popups.h
      ui/popups/RasterImageHeaderPromptPopup.h
      ui/settings/SettingsModel.h
      ui/toolbars/AnnotationToolbar.h
      ui/toolbars/ModeToolbar.h
      ui/toolbars/SegmentationToolbar.h
      ui/toolbars/ToolbarCommon.h
      ui/toolbars/Toolbars.h
      ui/updates/UpdateCheck.h
      ui/widgets/ActiveImageSelection.h
      ui/widgets/ActiveImageSelectionModel.h
      ui/widgets/ImageHistogram.h
      ui/widgets/LandmarkWidget.h
      ui/widgets/PaletteWidget.h
      ui/widgets/SegmentationLabelsWidget.h
      ui/widgets/Widgets.h
      ui/windows/AnnotationWindow.h
      ui/windows/ExportStatusWindow.h
      ui/windows/ImagePropertiesWindow.h
      ui/windows/InspectionWindow.h
      ui/windows/InspectionWindowSizing.h
      ui/windows/IsosurfacesWindow.h
      ui/windows/KeyboardShortcuts.h
      ui/windows/LandmarkPropertiesWindow.h
      ui/windows/LoadingStatusModel.h
      ui/windows/OpacityMixerModel.h
      ui/windows/OpacityMixerWindow.h
      ui/windows/RegionStatisticsController.h
      ui/windows/RegionStatisticsWindow.h
      ui/windows/RegistrationWindow.h
      ui/windows/SegmentationPropertiesWindow.h
      ui/windows/SettingsWindow.h
      ui/windows/ViewOverlayModel.h
      ui/windows/ViewOverlayWindows.h
      ui/windows/Windows.h
)

set(entropy_UI_VENDOR_SOURCES
  "${entropy_EXT_DIR}/imGuIZMO.quat/imGuIZMOquat.cpp"
  "${entropy_EXT_DIR}/imGuIZMO.quat/ImGuiPaletteButton.cpp"
  "${entropy_EXT_DIR}/imgui-knobs/imgui-knobs.cpp"
)

set_source_files_properties(${entropy_UI_VENDOR_SOURCES}
  PROPERTIES
    COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:MSVC>:/W0;/WX->$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>"
)

if(Entropy_ENABLE_IWYU)
  set_target_properties(EntropyUI PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "")
endif()
if(Entropy_ENABLE_CLANG_TIDY)
  set_target_properties(EntropyUI PROPERTIES CXX_CLANG_TIDY "")
endif()

if(Entropy_ENABLE_IWYU OR Entropy_ENABLE_CLANG_TIDY)
  set_source_files_properties(${entropy_UI_VENDOR_SOURCES}
    PROPERTIES SKIP_LINTING ON
  )
endif()

if(APPLE AND (Entropy_ENABLE_IWYU OR Entropy_ENABLE_CLANG_TIDY))
  set_source_files_properties(
    ui/ImGuiWrapper.cpp
    PROPERTIES SKIP_LINTING ON
  )
endif()

target_compile_definitions(EntropyUI PRIVATE
  ENTROPY_VERSION="${PROJECT_VERSION}"
  ENTROPY_RESOURCE_DIR="${CMAKE_INSTALL_FULL_DATADIR}/entropy"
)

target_link_libraries(EntropyUI
  PUBLIC
    Entropy::Common
    Entropy::Image
    Entropy::Registration
    Entropy::Viewer
    glm::glm
    imgui
    implot
    spdlog::spdlog
    stduuid
  PRIVATE
    Entropy::Layout
    Entropy::Mesh
    Entropy::Rendering
    glad
    glfw
    nanovg
    nfd::nfd
    nlohmann_json::nlohmann_json
    CURL::libcurl
    Qt6::Core
    entropy::colormaps
    entropy::fonts
    entropy::icons
    entropy::licenses
    entropy_warnings
)

if(APPLE)
  target_link_libraries(EntropyUI PRIVATE
    ${COCOA_LIBRARY}
  )
endif()

if(WIN32)
  target_link_libraries(EntropyUI PRIVATE
    comctl32
    shell32
    user32
  )
endif()

target_include_directories(EntropyUI
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/ui"
    "${entropy_APP_DIR}"
)

target_include_directories(EntropyUI PRIVATE
  "${PROJECT_BINARY_DIR}" # for BuildStamp.h
  "${entropy_RES_DIR}"
  "${glad_INCLUDE_DIR}"
  "${iconfont_INCLUDE_DIR}"
  "${tinyfsm_INCLUDE_DIR}"
)

target_include_directories(EntropyUI SYSTEM PRIVATE
  "${imgui_SRC_DIR}"
  "${imgui_SRC_DIR}/imgui" # due to ImPlot
  "${entropy_EXT_DIR}"
  "${entropy_EXT_DIR}/imGuIZMO.quat"
  "${entropy_EXT_DIR}/imgui-knobs"
  "${implot_SRC_DIR}"
  "${nanovg_SRC_DIR}"
)

set_target_properties(EntropyUI PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  NO_SYSTEM_FROM_IMPORTED ON
)

entropy_enable_coverage_for_target(EntropyUI)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
