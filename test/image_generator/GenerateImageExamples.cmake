if(NOT DEFINED IMAGE_GENERATOR)
  message(FATAL_ERROR "IMAGE_GENERATOR is required")
endif()
if(NOT DEFINED IMAGE_GENERATOR_EXAMPLE_DIR)
  message(FATAL_ERROR "IMAGE_GENERATOR_EXAMPLE_DIR is required")
endif()
if(NOT DEFINED IMAGE_GENERATOR_EXAMPLE_OUTPUT_DIR)
  message(FATAL_ERROR "IMAGE_GENERATOR_EXAMPLE_OUTPUT_DIR is required")
endif()

file(REMOVE_RECURSE "${IMAGE_GENERATOR_EXAMPLE_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${IMAGE_GENERATOR_EXAMPLE_OUTPUT_DIR}")
file(GLOB image_specs LIST_DIRECTORIES false "${IMAGE_GENERATOR_EXAMPLE_DIR}/*.json")
list(SORT image_specs)

if(NOT image_specs)
  message(FATAL_ERROR "No image generator example specs found in ${IMAGE_GENERATOR_EXAMPLE_DIR}")
endif()

foreach(spec IN LISTS image_specs)
  execute_process(
    COMMAND "${IMAGE_GENERATOR}" --spec "${spec}"
    RESULT_VARIABLE result
    COMMAND_ECHO STDOUT
    WORKING_DIRECTORY "${IMAGE_GENERATOR_EXAMPLE_OUTPUT_DIR}"
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Failed to generate image example from ${spec}")
  endif()
endforeach()

list(LENGTH image_specs count)
message(STATUS "Generated ${count} image example files in ${IMAGE_GENERATOR_EXAMPLE_OUTPUT_DIR}")
