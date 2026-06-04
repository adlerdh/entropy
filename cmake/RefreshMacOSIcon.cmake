if(NOT entropy_BUNDLE_DIR)
  message(FATAL_ERROR "entropy_BUNDLE_DIR is required")
endif()

if(NOT entropy_SOURCE_ICON)
  message(FATAL_ERROR "entropy_SOURCE_ICON is required")
endif()

if(NOT entropy_ICON_NAME)
  message(FATAL_ERROR "entropy_ICON_NAME is required")
endif()

if(NOT entropy_MINIMUM_SYSTEM_VERSION)
  set(entropy_MINIMUM_SYSTEM_VERSION "13.0")
endif()

find_program(entropy_XCRUN_EXECUTABLE xcrun)
if(NOT entropy_XCRUN_EXECUTABLE)
  message(FATAL_ERROR "xcrun was not found; Xcode actool is required to compile ${entropy_SOURCE_ICON}")
endif()

set(entropy_RESOURCE_DIR "${entropy_BUNDLE_DIR}/Contents/Resources")
set(entropy_PARTIAL_INFO_PLIST "${entropy_RESOURCE_DIR}/assetcatalog_generated_info.plist")

file(MAKE_DIRECTORY "${entropy_RESOURCE_DIR}")

file(REMOVE_RECURSE
  "${entropy_RESOURCE_DIR}/Assets.car"
  "${entropy_RESOURCE_DIR}/${entropy_ICON_NAME}.icns"
  "${entropy_RESOURCE_DIR}/${entropy_ICON_NAME}.icon"
  "${entropy_PARTIAL_INFO_PLIST}"
)

execute_process(
  COMMAND
    "${entropy_XCRUN_EXECUTABLE}" actool
    "${entropy_SOURCE_ICON}"
    --app-icon "${entropy_ICON_NAME}"
    --compile "${entropy_RESOURCE_DIR}"
    --output-partial-info-plist "${entropy_PARTIAL_INFO_PLIST}"
    --minimum-deployment-target "${entropy_MINIMUM_SYSTEM_VERSION}"
    --platform macosx
    --target-device mac
  RESULT_VARIABLE entropy_ACTOOL_RESULT
  OUTPUT_VARIABLE entropy_ACTOOL_OUTPUT
  ERROR_VARIABLE entropy_ACTOOL_ERROR
)

if(NOT entropy_ACTOOL_RESULT EQUAL 0)
  message(FATAL_ERROR "actool failed for ${entropy_SOURCE_ICON}: ${entropy_ACTOOL_ERROR}${entropy_ACTOOL_OUTPUT}")
endif()

if(NOT EXISTS "${entropy_RESOURCE_DIR}/Assets.car")
  message(FATAL_ERROR "actool did not generate ${entropy_RESOURCE_DIR}/Assets.car: ${entropy_ACTOOL_ERROR}${entropy_ACTOOL_OUTPUT}")
endif()

if(NOT EXISTS "${entropy_RESOURCE_DIR}/${entropy_ICON_NAME}.icns")
  message(FATAL_ERROR "actool did not generate ${entropy_RESOURCE_DIR}/${entropy_ICON_NAME}.icns: ${entropy_ACTOOL_ERROR}${entropy_ACTOOL_OUTPUT}")
endif()

file(REMOVE "${entropy_PARTIAL_INFO_PLIST}")

if(entropy_CODESIGN_IDENTITY)
  find_program(entropy_CODESIGN_EXECUTABLE codesign)
  if(NOT entropy_CODESIGN_EXECUTABLE)
    message(FATAL_ERROR "codesign was not found; set entropy_MACOS_CODESIGN_IDENTITY to an empty value to skip signing")
  endif()

  execute_process(
    COMMAND
      "${entropy_CODESIGN_EXECUTABLE}"
      --force
      --deep
      --sign "${entropy_CODESIGN_IDENTITY}"
      "${entropy_BUNDLE_DIR}"
    RESULT_VARIABLE entropy_CODESIGN_RESULT
    ERROR_VARIABLE entropy_CODESIGN_ERROR
  )

  if(NOT entropy_CODESIGN_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to sign ${entropy_BUNDLE_DIR}: ${entropy_CODESIGN_ERROR}")
  endif()
endif()
