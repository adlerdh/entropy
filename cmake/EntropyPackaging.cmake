if(UNIX AND NOT APPLE)
  include(EntropyLinuxPackaging)
elseif(APPLE)
  include(EntropyMacOSPackaging)
endif()

include(CPack)
