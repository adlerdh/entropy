add_library(EntropyRegistration STATIC)
add_library(Entropy::Registration ALIAS EntropyRegistration)

target_sources(EntropyRegistration
  PRIVATE
    registration/AffineTransformIO.cpp
    registration/Artifacts.cpp
    registration/Availability.cpp
    registration/Capabilities.cpp
    registration/Commands.cpp
    registration/Config.cpp
    registration/Execution.cpp
    registration/ImportPlan.cpp
    registration/Json.cpp
    registration/Jobs.cpp
    registration/Process.cpp
    registration/Progress.cpp
    registration/Setup.cpp
    registration/Types.cpp
    registration/Validation.cpp
  PUBLIC
    FILE_SET public_headers
    TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
      registration/AffineTransformIO.h
      registration/Artifacts.h
      registration/Availability.h
      registration/Capabilities.h
      registration/Commands.h
      registration/Config.h
      registration/Execution.h
      registration/ImportPlan.h
      registration/Jobs.h
      registration/Json.h
      registration/Process.h
      registration/Progress.h
      registration/Setup.h
      registration/Types.h
      registration/Validation.h
)

target_include_directories(EntropyRegistration
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_link_libraries(EntropyRegistration
  PUBLIC
    glm::glm
    nlohmann_json::nlohmann_json
  PRIVATE
    entropy_logging_level
    entropy_warnings
)

target_compile_definitions(EntropyRegistration PRIVATE
  ENTROPY_FIREANTS_BRIDGE_PYTHONPATH="${CMAKE_CURRENT_SOURCE_DIR}/fireants_bridge"
)

entropy_enable_coverage_for_target(EntropyRegistration)

set_target_properties(EntropyRegistration PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
