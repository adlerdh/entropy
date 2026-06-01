if(NOT DEFINED ENTROPY_RUNTIME_DLLS OR ENTROPY_RUNTIME_DLLS STREQUAL "")
  return()
endif()

if(NOT DEFINED ENTROPY_RUNTIME_OUTPUT_DIR OR ENTROPY_RUNTIME_OUTPUT_DIR STREQUAL "")
  message(FATAL_ERROR "ENTROPY_RUNTIME_OUTPUT_DIR is required")
endif()

string(REPLACE "," ";" _entropy_runtime_dlls "${ENTROPY_RUNTIME_DLLS}")

foreach(_entropy_runtime_dll IN LISTS _entropy_runtime_dlls)
  if(EXISTS "${_entropy_runtime_dll}")
    get_filename_component(_entropy_runtime_dll_name "${_entropy_runtime_dll}" NAME)
    file(COPY_FILE
      "${_entropy_runtime_dll}"
      "${ENTROPY_RUNTIME_OUTPUT_DIR}/${_entropy_runtime_dll_name}"
      ONLY_IF_DIFFERENT)
  endif()
endforeach()
