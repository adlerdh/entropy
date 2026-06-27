if(NOT DEFINED IMAGE_GENERATOR)
  message(FATAL_ERROR "IMAGE_GENERATOR is required")
endif()
if(NOT DEFINED WARP_FIELD_GENERATOR)
  message(FATAL_ERROR "WARP_FIELD_GENERATOR is required")
endif()
if(NOT DEFINED DEFORMATION_WARP_EXAMPLE_DIR)
  message(FATAL_ERROR "DEFORMATION_WARP_EXAMPLE_DIR is required")
endif()
if(NOT DEFINED DEFORMATION_WARP_EXAMPLE_OUTPUT_DIR)
  message(FATAL_ERROR "DEFORMATION_WARP_EXAMPLE_OUTPUT_DIR is required")
endif()

set(image_spec_dir "${DEFORMATION_WARP_EXAMPLE_DIR}/images")
set(field_spec_dir "${DEFORMATION_WARP_EXAMPLE_DIR}/fields")

file(REMOVE_RECURSE "${DEFORMATION_WARP_EXAMPLE_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${DEFORMATION_WARP_EXAMPLE_OUTPUT_DIR}")

file(GLOB image_specs LIST_DIRECTORIES false "${image_spec_dir}/*.json")
file(GLOB field_specs LIST_DIRECTORIES false "${field_spec_dir}/*.json")
list(SORT image_specs)
list(SORT field_specs)

if(NOT image_specs)
  message(FATAL_ERROR "No image specs found in ${image_spec_dir}")
endif()
if(NOT field_specs)
  message(FATAL_ERROR "No deformation field specs found in ${field_spec_dir}")
endif()

foreach(spec IN LISTS image_specs)
  execute_process(
    COMMAND "${IMAGE_GENERATOR}" --spec "${spec}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    WORKING_DIRECTORY "${DEFORMATION_WARP_EXAMPLE_OUTPUT_DIR}"
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Failed to generate image from ${spec}\n${stdout}\n${stderr}")
  endif()
endforeach()

foreach(spec IN LISTS field_specs)
  execute_process(
    COMMAND "${WARP_FIELD_GENERATOR}" --spec "${spec}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    WORKING_DIRECTORY "${DEFORMATION_WARP_EXAMPLE_OUTPUT_DIR}"
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Failed to generate deformation field from ${spec}\n${stdout}\n${stderr}")
  endif()
endforeach()

list(LENGTH image_specs image_count)
list(LENGTH field_specs field_count)
message(
  STATUS
  "Generated ${image_count} images and ${field_count} deformation fields in ${DEFORMATION_WARP_EXAMPLE_OUTPUT_DIR}")
