include(GNUInstallDirs)
include(PackageDocs)

set(entropy_LINUX_APP_ID "io.github.adlerdh.entropy")
set(entropy_LINUX_PRIVATE_LIB_DIR "${CMAKE_INSTALL_LIBDIR}/entropy")
set(entropy_LINUX_PRIVATE_LIB_FULL_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/entropy")
set(entropy_LINUX_DESKTOP_EXEC "entropy")
set(entropy_LINUX_DESKTOP_FILE "${CMAKE_CURRENT_BINARY_DIR}/${entropy_LINUX_APP_ID}.desktop")
set(entropy_LINUX_PRIVATE_LIBRARY_RPATHS)
if(NOT Entropy_STATIC_BUNDLED_DEPENDENCIES)
    list(APPEND entropy_LINUX_PRIVATE_LIBRARY_RPATHS
        "${glfw_PREFIX}/install/lib"
        "${nativefiledialog_PREFIX}/install/lib"
        "${spdlog_PREFIX}/install/lib"
        "${itk_PREFIX}/build/lib"
    )
endif()

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/Entropy.desktop.in"
    "${entropy_LINUX_DESKTOP_FILE}"
    @ONLY
)

set_target_properties(entropy PROPERTIES
    INSTALL_RPATH "$ORIGIN/../${entropy_LINUX_PRIVATE_LIB_DIR}"
)

install(TARGETS entropy
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(FILES "${entropy_LINUX_DESKTOP_FILE}"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
)

install(DIRECTORY "${entropy_FIREANTS_BRIDGE_DIR}/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/entropy/python/fireants_bridge"
)

entropy_install_package_documents("${CMAKE_INSTALL_DOCDIR}")

foreach(entropy_LINUX_ICON_SIZE IN ITEMS 16 32 48 64 128 256 512)
    install(FILES "${entropy_RES_DIR}/icons/Linux/hicolor/${entropy_LINUX_ICON_SIZE}x${entropy_LINUX_ICON_SIZE}/apps/${entropy_LINUX_APP_ID}.png"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${entropy_LINUX_ICON_SIZE}x${entropy_LINUX_ICON_SIZE}/apps"
    )
endforeach()

install(DIRECTORY "${qtbase_PREFIX}/install/lib/"
    DESTINATION "${entropy_LINUX_PRIVATE_LIB_DIR}"
    FILES_MATCHING
      PATTERN "libQt6Core.so*"
      PATTERN "cmake" EXCLUDE
      PATTERN "pkgconfig" EXCLUDE
)

foreach(entropy_LINUX_LIBRARY_DIR IN LISTS entropy_LINUX_PRIVATE_LIBRARY_RPATHS)
    if(EXISTS "${entropy_LINUX_LIBRARY_DIR}")
      install(DIRECTORY "${entropy_LINUX_LIBRARY_DIR}/"
        DESTINATION "${entropy_LINUX_PRIVATE_LIB_DIR}"
        FILES_MATCHING
          PATTERN "*.so"
          PATTERN "*.so.*"
          PATTERN "cmake" EXCLUDE
          PATTERN "pkgconfig" EXCLUDE
      )
    endif()
endforeach()

find_program(entropy_READELF_EXECUTABLE readelf REQUIRED)
string(CONCAT entropy_LINUX_PRIVATE_LIBRARY_VERIFY_CODE
[==[
    set(entropy_PRIVATE_LIB_DIR "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/]==]
"${entropy_LINUX_PRIVATE_LIB_DIR}"
[==[")
    file(GLOB entropy_PRIVATE_LIBS
      "${entropy_PRIVATE_LIB_DIR}/*.so"
      "${entropy_PRIVATE_LIB_DIR}/*.so.*")

    foreach(entropy_PRIVATE_LIB IN LISTS entropy_PRIVATE_LIBS)
      if(IS_SYMLINK "${entropy_PRIVATE_LIB}")
        continue()
      endif()

      execute_process(
        COMMAND "]==]
"${entropy_READELF_EXECUTABLE}"
[==[" -d "${entropy_PRIVATE_LIB}"
        OUTPUT_VARIABLE entropy_READELF_OUTPUT
        ERROR_VARIABLE entropy_READELF_ERROR
        RESULT_VARIABLE entropy_READELF_RESULT)
      if(NOT entropy_READELF_RESULT EQUAL 0)
        message(FATAL_ERROR "readelf failed for ${entropy_PRIVATE_LIB}: ${entropy_READELF_ERROR}")
      endif()

      if(entropy_READELF_OUTPUT MATCHES "]==]
"${CMAKE_BINARY_DIR}"
[==[")
        message(FATAL_ERROR "Packaged private library ${entropy_PRIVATE_LIB} contains a build-tree RPATH or RUNPATH. Rebuild the Linux superbuild so private shared libraries use $ORIGIN before packaging.")
      endif()
    endforeach()
]==]
)
install(CODE "${entropy_LINUX_PRIVATE_LIBRARY_VERIFY_CODE}")

set(CPACK_GENERATOR "${Entropy_LINUX_CPACK_GENERATORS}")
set(CPACK_PACKAGE_NAME "${APP_NAME}")
set(CPACK_PACKAGE_VENDOR "${PUBLISHER_NAME}")
set(CPACK_PACKAGE_CONTACT "${PUBLISHER_NAME}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${APP_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION "${VERSION_FULL}")
set(CPACK_PACKAGE_FILE_NAME "${APP_NAME}-${VERSION_FULL}-${Entropy_LINUX_PACKAGE_PLATFORM_LABEL}-${Entropy_PACKAGE_ARCHITECTURE}")
set(CPACK_ARCHIVE_FILE_NAME "${APP_NAME}-${VERSION_FULL}-${Entropy_LINUX_PACKAGE_PLATFORM_LABEL}-${Entropy_PACKAGE_ARCHITECTURE}-portable")
set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)
set(entropy_LINUX_CPACK_OPTIONS "${CMAKE_CURRENT_BINARY_DIR}/LinuxCPackOptions.cmake")
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/LinuxCPackOptions.cmake.in"
    "${entropy_LINUX_CPACK_OPTIONS}"
    @ONLY
)
set(CPACK_PROJECT_CONFIG_FILE "${entropy_LINUX_CPACK_OPTIONS}")
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CPACK_STRIP_FILES TRUE)
endif()
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")

set(CPACK_DEBIAN_PACKAGE_SECTION "science")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/adlerdh/entropy")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS "${entropy_LINUX_PRIVATE_LIB_FULL_DIR}")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "ca-certificates")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS
    "xdg-desktop-portal, xdg-desktop-portal-gtk | xdg-desktop-portal-kde | xdg-desktop-portal-gnome")

set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Engineering")
set(CPACK_RPM_PACKAGE_URL "https://github.com/adlerdh/entropy")
set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
set(CPACK_RPM_PACKAGE_AUTOPROV OFF)
set(CPACK_RPM_REQUIRES_EXCLUDE_FROM ".*/lib(64)?/entropy/.*")
set(CPACK_RPM_SPEC_MORE_DEFINE "%global __requires_exclude ^libQt6Core[.]so[.]6.*$")
