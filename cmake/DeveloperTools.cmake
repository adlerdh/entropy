option(Entropy_USE_CCACHE "Use ccache as the compiler launcher when available" ON)
option(Entropy_ENABLE_IWYU "Run Include What You Use during C++ compilation" OFF)
set(Entropy_IWYU_OPTIONS
  "-Xiwyu;--no_fwd_decls;-Xiwyu;--quoted_includes_first;-Xiwyu;--max_line_length=120;-Xiwyu;--error=0"
  CACHE STRING "Additional options passed to Include What You Use")
option(Entropy_ENABLE_CLANG_TIDY "Run clang-tidy during C++ compilation" OFF)
set(Entropy_CLANG_TIDY_OPTIONS "--quiet" CACHE STRING "Additional options passed to clang-tidy")
option(Entropy_ENABLE_CPPCHECK "Add the cppcheck static-analysis target" OFF)
set(Entropy_CPPCHECK_OPTIONS
  "--enable=warning,style,performance,portability;--addon=threadsafety;--inconclusive;--inline-suppr;--error-exitcode=1;--quiet;--template=gcc"
  CACHE STRING "Options passed to cppcheck")
set(Entropy_CPPCHECK_JOBS "4" CACHE STRING "Parallel cppcheck analysis jobs")

if(Entropy_USE_CCACHE)
  find_program(CCACHE_PROGRAM ccache)
  if(CCACHE_PROGRAM)
    message(STATUS "Using ccache compiler launcher: ${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE FILEPATH "C compiler launcher" FORCE)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE FILEPATH "CXX compiler launcher" FORCE)
    if(APPLE)
      set(CMAKE_OBJCXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}" CACHE FILEPATH "OBJCXX compiler launcher" FORCE)
    endif()
  else()
    message(STATUS "ccache was requested but was not found")
  endif()
else()
  unset(CMAKE_C_COMPILER_LAUNCHER CACHE)
  unset(CMAKE_CXX_COMPILER_LAUNCHER CACHE)
  if(APPLE)
    unset(CMAKE_OBJCXX_COMPILER_LAUNCHER CACHE)
  endif()
endif()

if(Entropy_ENABLE_IWYU)
  find_program(Entropy_IWYU_EXECUTABLE NAMES include-what-you-use iwyu)
  if(NOT Entropy_IWYU_EXECUTABLE)
    message(FATAL_ERROR "Entropy_ENABLE_IWYU=ON but include-what-you-use was not found")
  endif()
  message(STATUS "Using Include What You Use: ${Entropy_IWYU_EXECUTABLE}")
  set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${Entropy_IWYU_EXECUTABLE};${Entropy_IWYU_OPTIONS}"
      CACHE STRING "Include What You Use command" FORCE)
else()
  unset(CMAKE_CXX_INCLUDE_WHAT_YOU_USE CACHE)
endif()

if(Entropy_ENABLE_CLANG_TIDY)
  find_program(Entropy_CLANG_TIDY_EXECUTABLE NAMES clang-tidy)
  if(NOT Entropy_CLANG_TIDY_EXECUTABLE)
    message(FATAL_ERROR "Entropy_ENABLE_CLANG_TIDY=ON but clang-tidy was not found")
  endif()
  message(STATUS "Using clang-tidy: ${Entropy_CLANG_TIDY_EXECUTABLE}")
  set(CMAKE_CXX_CLANG_TIDY "${Entropy_CLANG_TIDY_EXECUTABLE};${Entropy_CLANG_TIDY_OPTIONS}"
      CACHE STRING "clang-tidy command" FORCE)
else()
  unset(CMAKE_CXX_CLANG_TIDY CACHE)
endif()

function(entropy_disable_linting_for_targets)
  if(NOT Entropy_ENABLE_IWYU AND NOT Entropy_ENABLE_CLANG_TIDY)
    return()
  endif()
  foreach(target IN LISTS ARGN)
    if(TARGET "${target}")
      set_target_properties("${target}" PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "" CXX_CLANG_TIDY "")
    endif()
  endforeach()
endfunction()

function(entropy_add_cppcheck_target)
  if(NOT Entropy_ENABLE_CPPCHECK)
    return()
  endif()

  find_program(Entropy_CPPCHECK_EXECUTABLE NAMES cppcheck)
  if(NOT Entropy_CPPCHECK_EXECUTABLE)
    message(FATAL_ERROR "Entropy_ENABLE_CPPCHECK=ON but cppcheck was not found")
  endif()

  message(STATUS "Using cppcheck: ${Entropy_CPPCHECK_EXECUTABLE}")
  add_custom_target(cppcheck
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/cppcheck"
    COMMAND
      "${Entropy_CPPCHECK_EXECUTABLE}"
      ${Entropy_CPPCHECK_OPTIONS}
      "-j${Entropy_CPPCHECK_JOBS}"
      "--cppcheck-build-dir=${CMAKE_BINARY_DIR}/cppcheck"
      "--suppressions-list=${CMAKE_SOURCE_DIR}/.cppcheck-suppressions"
      "--suppress=*:${CMAKE_SOURCE_DIR}/external/*"
      "--suppress=*:${CMAKE_SOURCE_DIR}/lib/image/external/*"
      "--suppress=*:${CMAKE_BINARY_DIR}/*"
      "--project=${CMAKE_BINARY_DIR}/compile_commands.json"
      "-i${CMAKE_SOURCE_DIR}/external"
      "-i${CMAKE_SOURCE_DIR}/lib/image/external"
      "-i${CMAKE_BINARY_DIR}"
      "-i${CMAKE_SOURCE_DIR}/app/logic/app/AppPathsMac.mm"
      "-i${CMAKE_SOURCE_DIR}/lib/ui/ClipboardMac.mm"
      "-i${CMAKE_SOURCE_DIR}/lib/ui/dialogs/NativeMessageDialogsMac.mm"
      "-i${CMAKE_SOURCE_DIR}/lib/ui/menus/MacNativeMainMenu.mm"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Running cppcheck static analysis"
    VERBATIM
  )
endfunction()

function(entropy_add_cppcheck_generated_dependencies)
  if(Entropy_ENABLE_CPPCHECK)
    add_dependencies(cppcheck ${ARGN})
  endif()
endfunction()
