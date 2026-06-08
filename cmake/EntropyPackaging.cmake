if(UNIX AND NOT APPLE)
  include(EntropyLinuxPackaging)
elseif(APPLE)
  include(EntropyMacOSPackaging)
elseif(WIN32)
  include(EntropyWindowsPackaging)
endif()

set(Entropy_PACKAGE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/packages" CACHE PATH "Directory for generated CPack package artifacts")
set(CPACK_PACKAGE_DIRECTORY "${Entropy_PACKAGE_OUTPUT_DIR}")

include(CPack)
