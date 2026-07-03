include(GNUInstallDirs)
include(PackageDocs)

set(entropy_WINDOWS_INSTALL_BINDIR ".")
set(entropy_WINDOWS_UPGRADE_GUID "93B363B9-12B9-4478-8D98-FB97AA2BB68E")
set(entropy_WINDOWS_RUNTIME_DEPENDENCY_DIRS
    "$<TARGET_FILE_DIR:entropy>"
    "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>"
    "${glfw_PREFIX}/install/bin"
    "${nativefiledialog_PREFIX}/install/bin"
    "${qtbase_PREFIX}/install/bin"
    "${spdlog_PREFIX}/install/bin"
)
if(NOT Entropy_STATIC_BUNDLED_DEPENDENCIES)
    list(APPEND entropy_WINDOWS_RUNTIME_DEPENDENCY_DIRS "${itk_PREFIX}/build/bin/$<CONFIG>")
endif()

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${entropy_WINDOWS_INSTALL_BINDIR}")
set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT Runtime)
include(InstallRequiredSystemLibraries)

install(TARGETS entropy
    RUNTIME_DEPENDENCY_SET entropy_WINDOWS_RUNTIME_DEPENDENCIES
    RUNTIME DESTINATION "${entropy_WINDOWS_INSTALL_BINDIR}"
    COMPONENT Runtime
)

install(RUNTIME_DEPENDENCY_SET entropy_WINDOWS_RUNTIME_DEPENDENCIES
    DIRECTORIES ${entropy_WINDOWS_RUNTIME_DEPENDENCY_DIRS}
    PRE_EXCLUDE_REGEXES
      "api-ms-.*"
      "ext-ms-.*"
    POST_EXCLUDE_REGEXES
      ".*[Ww][Ii][Nn][Dd][Oo][Ww][Ss][\\/].*"
      ".*[Ss][Yy][Ss][Tt][Ee][Mm]32[\\/].*"
    RUNTIME DESTINATION "${entropy_WINDOWS_INSTALL_BINDIR}"
    COMPONENT Runtime
)

if(NOT Entropy_STATIC_BUNDLED_DEPENDENCIES)
    install(DIRECTORY "${itk_PREFIX}/build/bin/$<CONFIG>/"
        DESTINATION "${entropy_WINDOWS_INSTALL_BINDIR}"
        COMPONENT Runtime
        FILES_MATCHING
          PATTERN "*.dll"
    )
endif()

install(DIRECTORY "${entropy_FIREANTS_BRIDGE_DIR}"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/entropy/python"
    COMPONENT Runtime
)

entropy_install_package_documents("." COMPONENT Runtime)

set(CPACK_GENERATOR "WIX;ZIP")
set(CPACK_PACKAGE_NAME "${APP_NAME}")
set(CPACK_PACKAGE_VENDOR "${PUBLISHER_NAME}")
set(CPACK_PACKAGE_CONTACT "${PUBLISHER_NAME}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${APP_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION "${VERSION_FULL}")
set(CPACK_PACKAGE_FILE_NAME "${APP_NAME}-${VERSION_FULL}-Windows-${Entropy_PACKAGE_ARCHITECTURE}")
set(CPACK_ARCHIVE_FILE_NAME "${APP_NAME}-${VERSION_FULL}-Windows-${Entropy_PACKAGE_ARCHITECTURE}-portable")
set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)
set(entropy_WINDOWS_CPACK_OPTIONS "${CMAKE_CURRENT_BINARY_DIR}/WindowsCPackOptions.cmake")
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/WindowsCPackOptions.cmake.in"
    "${entropy_WINDOWS_CPACK_OPTIONS}"
    @ONLY
)
set(CPACK_PROJECT_CONFIG_FILE "${entropy_WINDOWS_CPACK_OPTIONS}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${APP_NAME}")
set(CPACK_PACKAGE_EXECUTABLES "entropy" "${APP_NAME}")
set(CPACK_CREATE_DESKTOP_LINKS "entropy")

set(CPACK_WIX_UPGRADE_GUID "${entropy_WINDOWS_UPGRADE_GUID}")
set(Entropy_WIX_ROOT "" CACHE PATH "WiX Toolset v3 directory containing candle.exe and light.exe; leave empty to use PATH, WIX, or build-tree tools")
if(NOT Entropy_WIX_ROOT)
    set(entropy_WINDOWS_LOCAL_WIX_ROOT "${CMAKE_BINARY_DIR}/tools/wix/tools")
    if(EXISTS "${entropy_WINDOWS_LOCAL_WIX_ROOT}/candle.exe" AND EXISTS "${entropy_WINDOWS_LOCAL_WIX_ROOT}/light.exe")
      set(Entropy_WIX_ROOT "${entropy_WINDOWS_LOCAL_WIX_ROOT}" CACHE PATH
          "WiX Toolset v3 directory containing candle.exe and light.exe; leave empty to use PATH, WIX, or build-tree tools"
          FORCE)
    endif()
endif()
if(Entropy_WIX_ROOT)
    set(CPACK_WIX_ROOT "${Entropy_WIX_ROOT}")
endif()
set(CPACK_WIX_PRODUCT_ICON "${entropy_WINDOWS_APP_ICON}")
set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/res/installer/Windows/WixUIBanner.bmp")
set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/res/installer/Windows/WixUIDialog.bmp")
set(CPACK_WIX_PROGRAM_MENU_FOLDER "${APP_NAME}")
set(CPACK_WIX_UI_REF "WixUI_InstallDir")
set(CPACK_WIX_ROOT_FEATURE_TITLE "${APP_NAME}")
set(CPACK_WIX_ROOT_FEATURE_DESCRIPTION "${APP_DESCRIPTION}")
set(CPACK_WIX_PROPERTY_ARPCOMMENTS "${APP_DESCRIPTION}")
set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "https://github.com/adlerdh/entropy")
set(CPACK_WIX_PROPERTY_ARPURLUPDATEINFO "https://github.com/adlerdh/entropy/releases")
