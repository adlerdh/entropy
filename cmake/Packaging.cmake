set(entropy_PACKAGE_ARCHITECTURE_DEFAULT "${CMAKE_SYSTEM_PROCESSOR}")
string(TOLOWER "${entropy_PACKAGE_ARCHITECTURE_DEFAULT}" entropy_PACKAGE_ARCHITECTURE_LOWER)
if(entropy_PACKAGE_ARCHITECTURE_LOWER MATCHES "^(amd64|x86_64)$")
  set(entropy_PACKAGE_ARCHITECTURE_DEFAULT "x86_64")
elseif(entropy_PACKAGE_ARCHITECTURE_LOWER MATCHES "^(arm64|aarch64)$")
  set(entropy_PACKAGE_ARCHITECTURE_DEFAULT "arm64")
endif()

set(Entropy_PACKAGE_ARCHITECTURE "${entropy_PACKAGE_ARCHITECTURE_DEFAULT}"
    CACHE STRING "Architecture label used in generated package filenames")

if(UNIX AND NOT APPLE)
  include(LinuxPackaging)
elseif(APPLE)
  include(MacOSPackaging)
elseif(WIN32)
  include(WindowsPackaging)
endif()

set(Entropy_PACKAGE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/packages" CACHE PATH "Directory for generated CPack package artifacts")
set(CPACK_PACKAGE_DIRECTORY "${Entropy_PACKAGE_OUTPUT_DIR}")

include(CPack)
