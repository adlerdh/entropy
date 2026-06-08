if(NOT entropy_BUNDLE_DIR)
  message(FATAL_ERROR "entropy_BUNDLE_DIR is required")
endif()

if(NOT entropy_BUNDLE_EXECUTABLE_NAME)
  message(FATAL_ERROR "entropy_BUNDLE_EXECUTABLE_NAME is required")
endif()

if(NOT entropy_CLI_EXECUTABLE_NAME)
  message(FATAL_ERROR "entropy_CLI_EXECUTABLE_NAME is required")
endif()

set(entropy_MACOS_DIR "${entropy_BUNDLE_DIR}/Contents/MacOS")
set(entropy_INFO_PLIST "${entropy_BUNDLE_DIR}/Contents/Info.plist")
set(entropy_OLD_EXECUTABLE "${entropy_MACOS_DIR}/${entropy_BUNDLE_EXECUTABLE_NAME}")
set(entropy_NEW_EXECUTABLE "${entropy_MACOS_DIR}/${entropy_CLI_EXECUTABLE_NAME}")
set(entropy_TEMP_EXECUTABLE "${entropy_MACOS_DIR}/${entropy_CLI_EXECUTABLE_NAME}.rename-tmp")

if(EXISTS "${entropy_OLD_EXECUTABLE}")
  file(REMOVE "${entropy_TEMP_EXECUTABLE}")
  file(RENAME "${entropy_OLD_EXECUTABLE}" "${entropy_TEMP_EXECUTABLE}")
  file(REMOVE "${entropy_NEW_EXECUTABLE}")
  file(RENAME "${entropy_TEMP_EXECUTABLE}" "${entropy_NEW_EXECUTABLE}")
elseif(NOT EXISTS "${entropy_NEW_EXECUTABLE}")
  message(FATAL_ERROR "Neither ${entropy_OLD_EXECUTABLE} nor ${entropy_NEW_EXECUTABLE} exists")
endif()

file(READ "${entropy_INFO_PLIST}" entropy_INFO_PLIST_CONTENTS)
string(REGEX REPLACE
  "(<key>CFBundleExecutable</key>[ \t\r\n]*<string>)[^<]*(</string>)"
  "\\1${entropy_CLI_EXECUTABLE_NAME}\\2"
  entropy_INFO_PLIST_CONTENTS
  "${entropy_INFO_PLIST_CONTENTS}")
file(WRITE "${entropy_INFO_PLIST}" "${entropy_INFO_PLIST_CONTENTS}")
