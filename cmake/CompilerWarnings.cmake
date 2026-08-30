add_library(entropy_warnings INTERFACE)
add_library(Entropy::Warnings ALIAS entropy_warnings)

# Keep the first-party warning policy comparable across supported compilers.
# /W4 is MSVC's practical counterpart to the GNU-family baseline. MSVC's
# /Wall additionally enables many low-value compiler and system-header warnings.
target_compile_options(entropy_warnings INTERFACE
  $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:
    -Werror
    -Wall
    -Wextra
    -Wpedantic
    -Wpointer-arith
    -Winit-self
    -Wunreachable-code
    -Wshadow
    -Wno-error=array-bounds
    -ftrapv
  >
  $<$<AND:$<CXX_COMPILER_ID:GNU>,$<NOT:$<OR:$<BOOL:${Entropy_ENABLE_IWYU}>,$<BOOL:${Entropy_ENABLE_CLANG_TIDY}>>>>:
    -Wno-error=maybe-uninitialized
    -Wno-error=stringop-overflow
  >
  $<$<CXX_COMPILER_ID:MSVC>:
    /W4
    /WX
    /permissive-
    /wd4702
  >
  $<$<AND:$<CONFIG:Debug>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>>:
    -fstack-protector-all
  >
)

add_library(entropy_logging_level INTERFACE)
add_library(Entropy::LoggingLevel ALIAS entropy_logging_level)
target_compile_definitions(entropy_logging_level INTERFACE
  $<$<OR:$<CONFIG:Debug>,$<BOOL:${Entropy_ENABLE_TRACE_LOGGING}>>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE>
  $<$<NOT:$<OR:$<CONFIG:Debug>,$<BOOL:${Entropy_ENABLE_TRACE_LOGGING}>>>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG>
  $<$<CONFIG:Debug>:ENTROPY_DEFAULT_LOG_LEVEL=SPDLOG_LEVEL_DEBUG>
  $<$<NOT:$<CONFIG:Debug>>:ENTROPY_DEFAULT_LOG_LEVEL=SPDLOG_LEVEL_INFO>
)
