# gameengine2 — the top-level build. Targets:
#   ge_core      the deterministic pool (header-only today)
#   <x>_test     one executable per tests/<x>_test.cpp, registered with CTest
# The flags below are part of the DETERMINISM CONTRACT (AGENTS.md); a target
# that drops them can silently change the bits a simulation produces.
cmake_minimum_required(VERSION 3.25)
project(gameengine2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GE_WERROR "Treat warnings as errors" ON)
option(GE_SANITIZE "Build with address + undefined sanitizers" OFF)

# Applied to every target in the tree.
add_library(ge_flags INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ge_flags INTERFACE
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion
    # Determinism: no fused multiply-add contraction, no fast-math reassociation.
    -ffp-contract=off -fno-fast-math)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ge_flags INTERFACE -fexcess-precision=standard)
  endif()
  if(GE_WERROR)
    target_compile_options(ge_flags INTERFACE -Werror)
  endif()
  if(GE_SANITIZE)
    target_compile_options(ge_flags INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(ge_flags INTERFACE -fsanitize=address,undefined)
  endif()
elseif(MSVC)
  target_compile_options(ge_flags INTERFACE /W4 /fp:strict /permissive-)
  if(GE_WERROR)
    target_compile_options(ge_flags INTERFACE /WX)
  endif()
endif()

add_library(ge_core INTERFACE)
target_include_directories(ge_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/engine)
target_link_libraries(ge_core INTERFACE ge_flags)

enable_testing()
add_subdirectory(tests)
