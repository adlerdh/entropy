if(NOT entropy_BUNDLE_DIR)
  message(FATAL_ERROR "entropy_BUNDLE_DIR is required")
endif()

if(NOT entropy_SOURCE_ICON)
  message(FATAL_ERROR "entropy_SOURCE_ICON is required")
endif()

set(entropy_RESOURCE_DIR "${entropy_BUNDLE_DIR}/Contents/Resources")
set(entropy_TARGET_ICON_DIR "${entropy_RESOURCE_DIR}/Entropy.icon")

file(MAKE_DIRECTORY "${entropy_RESOURCE_DIR}")

file(GLOB entropy_APP_ICON_RESOURCES "${entropy_RESOURCE_DIR}/Entropy*")
foreach(entropy_APP_ICON_RESOURCE IN LISTS entropy_APP_ICON_RESOURCES)
  if(NOT entropy_APP_ICON_RESOURCE STREQUAL entropy_TARGET_ICON_DIR)
    file(REMOVE_RECURSE "${entropy_APP_ICON_RESOURCE}")
  endif()
endforeach()

file(REMOVE_RECURSE "${entropy_TARGET_ICON_DIR}")
file(COPY "${entropy_SOURCE_ICON}" DESTINATION "${entropy_RESOURCE_DIR}")
