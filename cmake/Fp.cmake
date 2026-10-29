# THE DETERMINISM FLAGS: ge_fp (the floating-point half of the DETERMINISM CONTRACT, AGENTS.md) and
# ge_isa (the instruction-set baseline), stated once here. Included by the root CMakeLists.txt,
# which links both into ge_core so they travel into an installed package, and by cmake/Mods.cmake,
# so a mod built against an engine tree is compiled with the same flags as the game that loads it.
# The flags are generator expressions so a package picks them by the CONSUMER's compiler.
include_guard(GLOBAL)

add_library(ge_fp INTERFACE)
target_compile_options(ge_fp INTERFACE
  # Determinism: no fused multiply-add contraction, no fast-math reassociation.
  "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-ffp-contract=off;-fno-fast-math>"
  "$<$<CXX_COMPILER_ID:GNU>:-fexcess-precision=standard>"
  "$<$<CXX_COMPILER_ID:MSVC>:/fp:strict>")

# The instruction-set baseline. On x86-64 the engine requires AVX2 (x86-64-v3: AVX2, FMA, BMI2,
# every desktop x86 CPU since 2013), so the compiler vectorises with 256-bit registers; ARM64's
# baseline already has NEON. It changes no bits: ge_fp keeps contraction off, so the FMA
# instructions it allows are never formed from a*b+c. It travels with ge_core, so a game is
# built for the same CPUs as the engine it links.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
  set(GE_ISA_X86 ON)
endif()
add_library(ge_isa INTERFACE)
if(GE_ISA_X86)
  target_compile_options(ge_isa INTERFACE
    "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-march=x86-64-v3>"
    "$<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>")
endif()
