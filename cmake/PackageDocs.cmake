set(entropy_PACKAGE_DOCUMENTS
    "${CMAKE_SOURCE_DIR}/README.md"
    "${CMAKE_SOURCE_DIR}/LICENSE.txt"
    "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
)

function(entropy_install_package_documents destination)
  set(options)
  set(oneValueArgs COMPONENT)
  set(multiValueArgs)
  cmake_parse_arguments(entropy_DOCS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(entropy_DOCS_COMPONENT)
    install(FILES ${entropy_PACKAGE_DOCUMENTS}
        DESTINATION "${destination}"
        COMPONENT "${entropy_DOCS_COMPONENT}"
    )
  else()
    install(FILES ${entropy_PACKAGE_DOCUMENTS}
        DESTINATION "${destination}"
    )
  endif()
endfunction()
