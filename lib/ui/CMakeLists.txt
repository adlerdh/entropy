set(entropy_IMGUI_WRAPPER_SOURCES
  imgui/Lifecycle.cpp
  imgui/Layout.cpp
  imgui/TimePlayback.cpp
  imgui/Workspace.cpp
  imgui/Fonts.cpp
  imgui/ProjectionJobs.cpp
  imgui/WarpJobs.cpp
  imgui/RegistrationJobs.cpp
  imgui/UpdateChecks.cpp
  imgui/MeshJobs.cpp
  imgui/ImageSelection.cpp
  imgui/Frame.cpp
  imgui/Panels.cpp
  imgui/ViewOverlays.cpp
  imgui/Settings.cpp
  imgui/EditingActions.cpp
  imgui/MenuActions.cpp
  imgui/MenuState.cpp
  imgui/Menus.cpp
)

set(entropy_UI_SOURCES
  AboutIcon.cpp
  DicomMetadataTable.cpp
  ExportJobService.cpp
  dialogs/InputLoadErrorDialog.cpp
  dialogs/InputLoadErrorDialogNative.cpp
  dialogs/NativeMessageDialogModel.cpp
  dialogs/WarpFieldAssignment.cpp
  GradientBackgroundRenderer.cpp
  GuiData.cpp
  headers/AnnotationHeader.cpp
  headers/ImageHeader.cpp
  headers/ImageHeaderInformation.cpp
  headers/LandmarkHeader.cpp
  headers/SegmentationHeader.cpp
  Helpers.cpp
  ImageExport.cpp
  IsosurfaceRangeModel.cpp
  headers/IsosurfaceHeader.cpp
  ImGuiCustomControls.cpp
  ${entropy_IMGUI_WRAPPER_SOURCES}
  LinuxUiScale.cpp
  menus/MainMenuBar.cpp
  NativeFileDialogs.cpp
  RegionStatisticsExport.cpp
  popups/AboutPopup.cpp
  popups/AddLayoutPopup.cpp
  popups/ConfirmClosePopup.cpp
  popups/ConfirmRemoveImagePopup.cpp
  popups/ConfirmSetReferenceImagePopup.cpp
  popups/DicomFolderPathPopup.cpp
  popups/DicomSeriesSelectionPopup.cpp
  popups/LargeImageLoadPromptPopup.cpp
  popups/RasterImageHeaderPromptPopup.cpp
  popups/UnsavedProjectPopup.cpp
  Style.cpp
  settings/SettingsModel.cpp
  ThirdPartyLicenses.cpp
  toolbars/AnnotationToolbar.cpp
  toolbars/ModeToolbar.cpp
  toolbars/SegmentationToolbar.cpp
  UiScaleManager.cpp
  updates/UpdateCheck.cpp
  widgets/ActiveImageSelection.cpp
  widgets/ActiveImageSelectionModel.cpp
  widgets/LandmarkWidget.cpp
  widgets/PaletteWidget.cpp
  widgets/SegmentationLabelsWidget.cpp
  windows/AnnotationWindow.cpp
  windows/ExportStatusWindow.cpp
  windows/ImagePropertiesWindow.cpp
  windows/InspectionTableWindow.cpp
  windows/InspectionWindow.cpp
  windows/InspectionWindowSizing.cpp
  windows/IsosurfacesWindow.cpp
  windows/KeyboardShortcuts.cpp
  windows/LandmarkPropertiesWindow.cpp
  windows/LoadingStatusModel.cpp
  windows/OpacityMixerModel.cpp
  windows/OpacityMixerWindow.cpp
  windows/RegistrationWindow.cpp
  windows/RegionStatisticsWindow.cpp
  windows/RegionStatisticsController.cpp
  windows/SegmentationPropertiesWindow.cpp
  windows/SettingsWindow.cpp
  windows/ViewOverlayModel.cpp
  windows/ViewOverlayWindows.cpp

  "${entropy_EXT_DIR}/imGuIZMO.quat/imGuIZMOquat.cpp"
  "${entropy_EXT_DIR}/imGuIZMO.quat/ImGuiPaletteButton.cpp"
  "${entropy_EXT_DIR}/imgui-knobs/imgui-knobs.cpp"
)

if(APPLE)
  list(APPEND entropy_UI_SOURCES
    ClipboardMac.mm
    dialogs/NativeMessageDialogsMac.mm
    menus/MacNativeMainMenu.mm
  )
  set_source_files_properties(
    ClipboardMac.mm
    dialogs/NativeMessageDialogsMac.mm
    menus/MacNativeMainMenu.mm
    PROPERTIES COMPILE_OPTIONS "-fobjc-arc"
  )
elseif(WIN32)
  list(APPEND entropy_UI_SOURCES
    ClipboardWin.cpp
    dialogs/NativeMessageDialogsWin.cpp
    menus/WinNativeMainMenu.cpp
  )
else()
  list(APPEND entropy_UI_SOURCES
    Clipboard.cpp
    dialogs/NativeMessageDialogs.cpp
  )
endif()

add_library(EntropyUI STATIC ${entropy_UI_SOURCES})
add_library(Entropy::UI ALIAS EntropyUI)

target_sources(EntropyUI
  PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/.."
    FILES
      AboutIcon.h
      Clipboard.h
      DicomMetadataTable.h
      ExportJobService.h
      GradientBackgroundRenderer.h
      GuiData.h
      Helpers.h
      ImGuiCustomControls.h
      ImGuiWrapper.h
      ImageExport.h
      IsosurfaceRangeModel.h
      LinuxUiScale.h
      NativeFileDialogs.h
      RegionStatisticsExport.h
      Scaling.h
      Style.h
      ThirdPartyLicenses.h
      UiControls.h
      UiScaleManager.h
      dialogs/InputLoadErrorDialog.h
      dialogs/NativeMessageDialogModel.h
      dialogs/NativeMessageDialogs.h
      dialogs/WarpFieldAssignment.h
      entropy_imgui_config.h
      headers/AnnotationHeader.h
      headers/HeaderCommon.h
      headers/Headers.h
      headers/ImageHeader.h
      headers/IsosurfaceHeader.h
      headers/LandmarkHeader.h
      headers/SegmentationHeader.h
      menus/MacNativeMainMenu.h
      menus/MainMenuBar.h
      menus/WinNativeMainMenu.h
      popups/AboutPopup.h
      popups/AddLayoutPopup.h
      popups/ConfirmationPopups.h
      popups/DicomPopups.h
      popups/LargeImageLoadPromptPopup.h
      popups/PopupCommon.h
      popups/Popups.h
      popups/RasterImageHeaderPromptPopup.h
      settings/SettingsModel.h
      toolbars/AnnotationToolbar.h
      toolbars/ModeToolbar.h
      toolbars/SegmentationToolbar.h
      toolbars/ToolbarCommon.h
      toolbars/Toolbars.h
      updates/UpdateCheck.h
      widgets/ActiveImageSelection.h
      widgets/ActiveImageSelectionModel.h
      widgets/ImageHistogram.h
      widgets/LandmarkWidget.h
      widgets/PaletteWidget.h
      widgets/SegmentationLabelsWidget.h
      widgets/Widgets.h
      windows/AnnotationWindow.h
      windows/ExportStatusWindow.h
      windows/ImagePropertiesWindow.h
      windows/InspectionWindow.h
      windows/InspectionWindowSizing.h
      windows/IsosurfacesWindow.h
      windows/KeyboardShortcuts.h
      windows/LandmarkPropertiesWindow.h
      windows/LoadingStatusModel.h
      windows/OpacityMixerModel.h
      windows/OpacityMixerWindow.h
      windows/RegionStatisticsController.h
      windows/RegionStatisticsWindow.h
      windows/RegistrationWindow.h
      windows/SegmentationPropertiesWindow.h
      windows/SettingsWindow.h
      windows/ViewOverlayModel.h
      windows/ViewOverlayWindows.h
      windows/Windows.h
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
    ${entropy_IMGUI_WRAPPER_SOURCES}
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
  if(MSVC)
    # TaskDialogIndirect needs Common Controls v6. Every executable using this
    # library needs the manifest entry, including tests that never open a dialog.
    target_link_options(EntropyUI INTERFACE
      "/MANIFESTDEPENDENCY:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'")
  endif()
endif()

target_include_directories(EntropyUI
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
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
