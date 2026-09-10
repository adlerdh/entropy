# ITK and VTK are private implementation details of the mesh I/O library.
find_package(ITK ${itk_VERSION} REQUIRED
  COMPONENTS
    ITKCommon
    ITKIOMeshFreeSurfer
    ITKIOMeshGifti
    ITKIOMeshOFF
  HINTS "${itk_PREFIX}/build")
entropy_configure_itk_directory()

add_library(EntropyMesh STATIC
  MeshFormat.cpp
  MeshIO.cpp
  MeshTransform.cpp
  private/FreeSurferCoordinates.cpp
  private/ItkMeshCodec.cpp
  private/VtkMeshCodec.cpp
)
add_library(Entropy::Mesh ALIAS EntropyMesh)

target_include_directories(EntropyMesh
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/.."
  SYSTEM PRIVATE ${ITK_INCLUDE_DIRS}
)

target_link_libraries(EntropyMesh
  PUBLIC
    glm::glm
  PRIVATE
    ${ITK_LIBRARIES}
    VTK::FiltersCore
    VTK::IOGeometry
    VTK::IOLegacy
    VTK::IOPLY
    VTK::IOXML
    entropy_warnings
)

set_target_properties(EntropyMesh PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

entropy_enable_coverage_for_target(EntropyMesh)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
