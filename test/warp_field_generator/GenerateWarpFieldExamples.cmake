if(NOT DEFINED WARP_FIELD_GENERATOR)
  message(FATAL_ERROR "WARP_FIELD_GENERATOR is required")
endif()

if(NOT DEFINED WARP_FIELD_EXAMPLE_DIR)
  message(FATAL_ERROR "WARP_FIELD_EXAMPLE_DIR is required")
endif()

if(NOT DEFINED WARP_FIELD_EXAMPLE_OUTPUT_DIR)
  message(FATAL_ERROR "WARP_FIELD_EXAMPLE_OUTPUT_DIR is required")
endif()

file(MAKE_DIRECTORY "${WARP_FIELD_EXAMPLE_OUTPUT_DIR}")
file(GLOB warp_field_specs LIST_DIRECTORIES false "${WARP_FIELD_EXAMPLE_DIR}/*.json")
list(SORT warp_field_specs)

if(NOT warp_field_specs)
  message(FATAL_ERROR "No warp field example specs found in ${WARP_FIELD_EXAMPLE_DIR}")
endif()

foreach(spec IN LISTS warp_field_specs)
  get_filename_component(name "${spec}" NAME_WE)
  set(output "${WARP_FIELD_EXAMPLE_OUTPUT_DIR}/${name}.nii.gz")

  execute_process(
    COMMAND "${WARP_FIELD_GENERATOR}" --spec "${spec}" --output "${output}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )

  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Failed to generate ${output}\n${stdout}\n${stderr}")
  endif()
endforeach()

list(LENGTH warp_field_specs count)
message(STATUS "Generated ${count} warp field example images in ${WARP_FIELD_EXAMPLE_OUTPUT_DIR}")
