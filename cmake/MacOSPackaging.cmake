option(Entropy_STRIP_PACKAGED_APP "Strip local symbols from the installed macOS app bundle before signing" ON)

set_target_properties(entropy PROPERTIES
    OUTPUT_NAME "${APP_NAME}"
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_LIST_DIR}/EntropyInfo.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "${APP_NAME}"
    MACOSX_BUNDLE_COPYRIGHT "${COPYRIGHT_LINE}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${Entropy_MACOSX_BUNDLE_IDENTIFIER}"
    MACOSX_BUNDLE_ICON_FILE "Entropy.icns"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_FEATURE}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${Entropy_MACOSX_BUNDLE_VERSION}"
)

install(TARGETS entropy
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

install(FILES "${entropy_ABOUT_ICON_SOURCE}"
    DESTINATION "${APP_NAME}.app/Contents/Resources"
    RENAME "${entropy_ABOUT_ICON_FILE_NAME}"
)

install(DIRECTORY "${entropy_FIREANTS_BRIDGE_DIR}"
    DESTINATION "${APP_NAME}.app/Contents/Resources/python"
)

set(entropy_BUNDLE_LIBRARY_DIRS
    "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    "${qtbase_PREFIX}/install/lib"
)
if(NOT Entropy_STATIC_BUNDLED_DEPENDENCIES)
    list(APPEND entropy_BUNDLE_LIBRARY_DIRS
        "${glfw_PREFIX}/install/lib"
        "${nativefiledialog_PREFIX}/install/lib"
        "${spdlog_PREFIX}/install/lib"
        "${itk_PREFIX}/build/lib"
    )
endif()

install(CODE "
    include(BundleUtilities)
    set(BU_CHMOD_BUNDLE_ITEMS ON)
    get_filename_component(entropy_INSTALL_PREFIX_ABSOLUTE \"\${CMAKE_INSTALL_PREFIX}\" ABSOLUTE)
    set(entropy_INSTALLED_BUNDLE \"\${entropy_INSTALL_PREFIX_ABSOLUTE}/${APP_NAME}.app\")
    fixup_bundle(\"\${entropy_INSTALLED_BUNDLE}\" \"\" \"${entropy_BUNDLE_LIBRARY_DIRS}\")

    set(entropy_STRIP_PACKAGED_APP \"${Entropy_STRIP_PACKAGED_APP}\")
    if(entropy_STRIP_PACKAGED_APP)
      find_program(entropy_STRIP_EXECUTABLE strip)
      if(entropy_STRIP_EXECUTABLE)
        file(GLOB entropy_BUNDLE_STRIP_ITEMS
          \"\${entropy_INSTALLED_BUNDLE}/Contents/MacOS/entropy\"
          \"\${entropy_INSTALLED_BUNDLE}/Contents/Frameworks/*.dylib\")

        foreach(entropy_BUNDLE_STRIP_ITEM IN LISTS entropy_BUNDLE_STRIP_ITEMS)
          if(NOT IS_SYMLINK \"\${entropy_BUNDLE_STRIP_ITEM}\")
            execute_process(
              COMMAND \"\${entropy_STRIP_EXECUTABLE}\" -x -S \"\${entropy_BUNDLE_STRIP_ITEM}\"
              RESULT_VARIABLE entropy_STRIP_RESULT
              ERROR_VARIABLE entropy_STRIP_ERROR
            )
            if(NOT entropy_STRIP_RESULT EQUAL 0)
              message(FATAL_ERROR \"Failed to strip \${entropy_BUNDLE_STRIP_ITEM}: \${entropy_STRIP_ERROR}\")
            endif()
          endif()
        endforeach()
      else()
        message(WARNING \"strip was not found; packaged app binaries will not be stripped\")
      endif()
    endif()

    set(entropy_CODESIGN_IDENTITY \"${Entropy_MACOS_CODESIGN_IDENTITY}\")
    if(entropy_CODESIGN_IDENTITY)
      find_program(entropy_CODESIGN_EXECUTABLE codesign)
      if(NOT entropy_CODESIGN_EXECUTABLE)
        message(FATAL_ERROR \"codesign was not found; set Entropy_MACOS_CODESIGN_IDENTITY to an empty value to skip signing\")
      endif()

      file(GLOB_RECURSE entropy_BUNDLE_DYLIBS
        \"\${entropy_INSTALLED_BUNDLE}/Contents/Frameworks/*.dylib\")
      file(GLOB entropy_BUNDLE_FRAMEWORKS
        \"\${entropy_INSTALLED_BUNDLE}/Contents/Frameworks/*.framework\")

      foreach(entropy_BUNDLE_DYLIB IN LISTS entropy_BUNDLE_DYLIBS)
        execute_process(
          COMMAND \"\${entropy_CODESIGN_EXECUTABLE}\" --force --sign \"\${entropy_CODESIGN_IDENTITY}\" \"\${entropy_BUNDLE_DYLIB}\"
          RESULT_VARIABLE entropy_CODESIGN_RESULT
        )
        if(NOT entropy_CODESIGN_RESULT EQUAL 0)
          message(FATAL_ERROR \"Failed to sign \${entropy_BUNDLE_DYLIB}\")
        endif()
      endforeach()

      foreach(entropy_BUNDLE_FRAMEWORK IN LISTS entropy_BUNDLE_FRAMEWORKS)
        execute_process(
          COMMAND \"\${entropy_CODESIGN_EXECUTABLE}\" --force --sign \"\${entropy_CODESIGN_IDENTITY}\" \"\${entropy_BUNDLE_FRAMEWORK}\"
          RESULT_VARIABLE entropy_CODESIGN_RESULT
        )
        if(NOT entropy_CODESIGN_RESULT EQUAL 0)
          message(FATAL_ERROR \"Failed to sign \${entropy_BUNDLE_FRAMEWORK}\")
        endif()
      endforeach()

      execute_process(
        COMMAND \"\${entropy_CODESIGN_EXECUTABLE}\" --force --sign \"\${entropy_CODESIGN_IDENTITY}\" \"\${entropy_INSTALLED_BUNDLE}\"
        RESULT_VARIABLE entropy_CODESIGN_RESULT
      )
      if(NOT entropy_CODESIGN_RESULT EQUAL 0)
        message(FATAL_ERROR \"Failed to sign \${entropy_INSTALLED_BUNDLE}\")
      endif()
    endif()
")

set(CPACK_GENERATOR "DragNDrop;ZIP")
set(CPACK_PACKAGE_NAME "${APP_NAME}")
set(CPACK_PACKAGE_VENDOR "${PUBLISHER_NAME}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${APP_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION "${VERSION_FULL}")
set(CPACK_PACKAGE_FILE_NAME "${APP_NAME}-${VERSION_FULL}-macOS-${Entropy_PACKAGE_ARCHITECTURE}")
set(CPACK_DMG_VOLUME_NAME "${APP_NAME} ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_FEATURE}")
set(CPACK_DMG_FORMAT "UDZO")
set(CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK OFF)
