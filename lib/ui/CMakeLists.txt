set(entropy_UI_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/AboutIcon.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/DicomMetadataTable.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/dialogs/NativeMessageDialogModel.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/GradientBackgroundRenderer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/GuiData.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/headers/AnnotationHeader.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/headers/ImageHeader.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/headers/ImageHeaderInformation.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/headers/LandmarkHeader.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/headers/SegmentationHeader.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Helpers.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ImageExport.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/IsosurfaceRangeModel.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/headers/IsosurfaceHeader.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ImGuiCustomControls.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ImGuiWrapper.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LinuxUiScale.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/menus/MainMenuBar.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/NativeFileDialogs.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/popups/AboutPopup.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/popups/AddLayoutPopup.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/popups/ConfirmClosePopup.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/popups/ConfirmRemoveImagePopup.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/popups/ConfirmSetReferenceImagePopup.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/popups/DicomFolderPathPopup.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/popups/DicomSeriesSelectionPopup.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/popups/LargeImageLoadPromptPopup.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/popups/RasterImageHeaderPromptPopup.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/popups/UnsavedProjectPopup.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Style.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/settings/SettingsModel.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/ThirdPartyLicenses.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/toolbars/AnnotationToolbar.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/toolbars/ModeToolbar.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/toolbars/SegmentationToolbar.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/UiScaleManager.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/updates/UpdateCheck.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/widgets/ActiveImageSelection.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/widgets/ActiveImageSelectionModel.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/widgets/LandmarkWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/widgets/PaletteWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/widgets/SegmentationLabelsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/windows/AnnotationWindow.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/windows/ImagePropertiesWindow.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/windows/InspectionTableWindow.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/windows/InspectionWindow.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/windows/IsosurfacesWindow.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/windows/KeyboardShortcuts.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/windows/LandmarkPropertiesWindow.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/windows/LoadingStatusModel.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/windows/OpacityMixerModel.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/windows/OpacityMixerWindow.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/windows/RegistrationWindow.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/windows/SegmentationPropertiesWindow.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/windows/SettingsWindow.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/windows/ViewOverlayModel.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/windows/ViewOverlayWindows.cpp"

  "${entropy_EXT_DIR}/imGuIZMO.quat/imGuIZMOquat.cpp"
  "${entropy_EXT_DIR}/imGuIZMO.quat/ImGuiPaletteButton.cpp"
  "${entropy_EXT_DIR}/imgui-knobs/imgui-knobs.cpp"
)

if(APPLE)
  list(APPEND entropy_UI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ClipboardMac.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/dialogs/NativeMessageDialogsMac.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/menus/MacNativeMainMenu.mm"
  )
elseif(WIN32)
  list(APPEND entropy_UI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ClipboardWin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dialogs/NativeMessageDialogsWin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menus/WinNativeMainMenu.cpp"
  )
else()
  list(APPEND entropy_UI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Clipboard.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dialogs/NativeMessageDialogs.cpp"
  )
endif()

add_library(EntropyUI STATIC ${entropy_UI_SOURCES})
add_library(Entropy::UI ALIAS EntropyUI)

set(entropy_UI_VENDOR_SOURCES
  "${entropy_EXT_DIR}/imGuIZMO.quat/imGuIZMOquat.cpp"
  "${entropy_EXT_DIR}/imGuIZMO.quat/ImGuiPaletteButton.cpp"
  "${entropy_EXT_DIR}/imgui-knobs/imgui-knobs.cpp"
)

set_source_files_properties(${entropy_UI_VENDOR_SOURCES}
  PROPERTIES
    COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:MSVC>:/W0;/WX->$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>"
)

if(Entropy_ENABLE_IWYU OR Entropy_ENABLE_CLANG_TIDY)
  set_source_files_properties(${entropy_UI_VENDOR_SOURCES}
    PROPERTIES SKIP_LINTING ON
  )
endif()

if(APPLE AND (Entropy_ENABLE_IWYU OR Entropy_ENABLE_CLANG_TIDY))
  set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/ImGuiWrapper.cpp"
    PROPERTIES SKIP_LINTING ON
  )
endif()

target_compile_definitions(EntropyUI PRIVATE
  ENTROPY_ABOUT_ICON_RESOURCE_PATH="${entropy_ABOUT_ICON_RESOURCE_PATH}"
  ENTROPY_VERSION="${PROJECT_VERSION}"
  ENTROPY_RESOURCE_DIR="${CMAKE_INSTALL_FULL_DATADIR}/entropy"
)

target_link_libraries(EntropyUI
  PUBLIC
    Entropy::Common
    Entropy::Image
    Entropy::Registration
    glm::glm
    imgui
    implot
    spdlog::spdlog
    stduuid
  PRIVATE
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
    entropy::shaders
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

target_include_directories(EntropyUI PUBLIC
  "${entropy_LIB_DIR}"
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
