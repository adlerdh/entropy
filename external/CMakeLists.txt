set(nanovg_SRC_DIR "${nanovg_PREFIX}/src/src")
add_library(nanovg STATIC "${nanovg_SRC_DIR}/nanovg.c")
target_include_directories(nanovg PRIVATE "${nanovg_SRC_DIR}")
entropy_disable_linting_for_targets(nanovg)

set(Entropy_GLAD_GL_VERSION "3.3" CACHE STRING "OpenGL Core profile version loaded by GLAD")
set_property(CACHE Entropy_GLAD_GL_VERSION PROPERTY STRINGS 3.3 4.1 4.6)
set(Entropy_GLAD_GL_DEBUG false CACHE BOOL "Use OpenGL debugging in GLAD")

string(REPLACE "." ";" entropy_GLAD_GL_VERSION_LIST "${Entropy_GLAD_GL_VERSION}")
list(GET entropy_GLAD_GL_VERSION_LIST 0 entropy_GLAD_GL_VERSION_MAJOR)
list(GET entropy_GLAD_GL_VERSION_LIST 1 entropy_GLAD_GL_VERSION_MINOR)

set(glad_BASE "glad_gl_${Entropy_GLAD_GL_VERSION}_core")
if(Entropy_GLAD_GL_DEBUG)
  string(APPEND glad_BASE "_debug")
endif()
set(glad_DIR "${CMAKE_CURRENT_SOURCE_DIR}/glad/${glad_BASE}")
set(glad_INCLUDE_DIR "${glad_DIR}/include")

add_library(glad STATIC "${glad_DIR}/src/glad.c")
target_include_directories(glad PRIVATE "${glad_INCLUDE_DIR}")
entropy_disable_linting_for_targets(glad)

set(imgui_SRC_DIR "${imgui_PREFIX}/src")
set(imgui_SOURCES
  "${imgui_SRC_DIR}/imgui/imgui.cpp"
  "${imgui_SRC_DIR}/imgui/imgui_demo.cpp"
  "${imgui_SRC_DIR}/imgui/imgui_draw.cpp"
  "${imgui_SRC_DIR}/imgui/imgui_tables.cpp"
  "${imgui_SRC_DIR}/imgui/imgui_widgets.cpp"
  "${imgui_SRC_DIR}/imgui/misc/cpp/imgui_stdlib.cpp"
  "${imgui_SRC_DIR}/imgui/backends/imgui_impl_glfw.cpp"
  "${imgui_SRC_DIR}/imgui/backends/imgui_impl_opengl3.cpp"
)
add_library(imgui STATIC ${imgui_SOURCES})
target_compile_definitions(imgui PRIVATE IMGUI_USER_CONFIG="ui/entropy_imgui_config.h")
target_include_directories(imgui PRIVATE
  "${imgui_SRC_DIR}/imgui"
  "${entropy_LIB_DIR}"
  "${glfw_INCLUDE_DIR}"
)
target_link_libraries(imgui PRIVATE glfw spdlog::spdlog)
target_compile_options(imgui PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
    -Werror -Wall -Wextra -Wpointer-arith -Winit-self -Wunreachable-code
    -Wshadow -pedantic-errors -ftrapv
    -Wno-error=deprecated-copy
    -Wno-error=empty-body
    -Wno-empty-body
  >
  $<$<CXX_COMPILER_ID:MSVC>:
    /W4
    /wd4390
  >
)
set_target_properties(imgui PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
entropy_disable_linting_for_targets(imgui)

set(implot_SRC_DIR "${implot_PREFIX}/src")
set(implot_SOURCES
  "${implot_SRC_DIR}/implot/implot_demo.cpp"
  "${implot_SRC_DIR}/implot/implot_items.cpp"
  "${implot_SRC_DIR}/implot/implot.cpp"
)
add_library(implot STATIC ${implot_SOURCES})
target_compile_definitions(implot PRIVATE IMGUI_USER_CONFIG="ui/entropy_imgui_config.h")
target_include_directories(implot PRIVATE
  "${implot_SRC_DIR}/implot"
  "${imgui_SRC_DIR}/imgui"
  "${entropy_LIB_DIR}"
)
target_link_libraries(implot PRIVATE glfw imgui spdlog::spdlog)
target_compile_options(implot PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
    -Wall -Wextra -Wpointer-arith -Winit-self -Wunreachable-code -Wshadow -ftrapv
  >
  $<$<CXX_COMPILER_ID:GNU>:-Wno-format-truncation -Wno-deprecated-enum-enum-conversion>
  $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wno-deprecated-enum-enum-conversion>
  $<$<CXX_COMPILER_ID:MSVC>:
    /W4
    /wd4390
    /wd5054
  >
)
set_target_properties(implot PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
entropy_disable_linting_for_targets(implot)

# These paths are consumed by first-party targets in sibling directories.
set(nanovg_SRC_DIR "${nanovg_SRC_DIR}" PARENT_SCOPE)
set(glad_INCLUDE_DIR "${glad_INCLUDE_DIR}" PARENT_SCOPE)
set(imgui_SRC_DIR "${imgui_SRC_DIR}" PARENT_SCOPE)
set(implot_SRC_DIR "${implot_SRC_DIR}" PARENT_SCOPE)
set(entropy_GLAD_GL_VERSION_MAJOR "${entropy_GLAD_GL_VERSION_MAJOR}" PARENT_SCOPE)
set(entropy_GLAD_GL_VERSION_MINOR "${entropy_GLAD_GL_VERSION_MINOR}" PARENT_SCOPE)
