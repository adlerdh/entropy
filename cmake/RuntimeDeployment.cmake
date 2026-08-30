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

function(entropy_configure_macos_runtime target)
  if(NOT APPLE)
    return()
  endif()

  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND
      "${CMAKE_COMMAND}"
      -D "entropy_BUNDLE_DIR=$<TARGET_BUNDLE_DIR:${target}>"
      -D "entropy_BUNDLE_EXECUTABLE_NAME=${APP_NAME}"
      -D "entropy_CLI_EXECUTABLE_NAME=entropy"
      -P "${CMAKE_SOURCE_DIR}/cmake/NormalizeMacOSBundleExecutable.cmake"
    COMMAND
      "${CMAKE_COMMAND}"
      -D "entropy_BUNDLE_DIR=$<TARGET_BUNDLE_DIR:${target}>"
      -D "entropy_SOURCE_ICON=${entropy_MACOS_APP_ICON}"
      -D "entropy_ICON_NAME=${entropy_MACOS_APP_ICON_NAME}"
      -D "entropy_MINIMUM_SYSTEM_VERSION=${Entropy_MACOSX_BUNDLE_MINIMUM_SYSTEM_VERSION}"
      -D "entropy_CODESIGN_IDENTITY=${Entropy_MACOS_CODESIGN_IDENTITY}"
      -P "${CMAKE_SOURCE_DIR}/cmake/RefreshMacOSIcon.cmake"
    COMMAND "${CMAKE_COMMAND}" -E rm -f "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${APP_NAME}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "$<TARGET_BUNDLE_DIR:${target}>/Contents/MacOS/entropy"
      "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/entropy"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
      "$<TARGET_BUNDLE_DIR:${target}>/Contents/Resources/python"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${entropy_FIREANTS_BRIDGE_DIR}"
      "$<TARGET_BUNDLE_DIR:${target}>/Contents/Resources/python/fireants_bridge"
    VERBATIM
  )
endfunction()
