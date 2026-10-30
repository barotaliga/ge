# The deterministic pool. Mostly headers; libm, solar, output and the worker pool are compiled.
# Threads: core/parallel.hpp runs a bake's independent rows side by side on a persistent pool.
find_package(Threads REQUIRED)
set(GE_LIBM_VENDORED
  engine/core/libm/fdlibm_trig.cpp
  engine/core/libm/fdlibm_arc.cpp
  engine/core/libm/fdlibm_exp_log.cpp)
add_library(ge_core STATIC ${GE_LIBM_VENDORED} engine/core/libm/pow.cpp
  engine/core/solar.cpp engine/core/moon.cpp engine/core/output.cpp engine/core/num_text.cpp engine/core/parallel.cpp
  engine/core/thread_name.cpp
  engine/core/json_read.cpp engine/core/inflate.cpp)
target_include_directories(ge_core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/engine>)
target_compile_features(ge_core PUBLIC cxx_std_20)
target_link_libraries(ge_core PUBLIC ge_fp ge_isa Threads::Threads)
# Our warnings only when this tree is the top-level build: a game that pulls it in
# with FetchContent, or links the installed package, gets ge_fp alone.
if(PROJECT_IS_TOP_LEVEL)
  target_link_libraries(ge_core PUBLIC $<BUILD_INTERFACE:ge_flags>)
endif()
# fdlibm is kept line for line with V8's copy; its deliberate int/double word conversions
# are not rewritten to quiet warnings. The FP flags above still apply to it.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set_source_files_properties(${GE_LIBM_VENDORED} PROPERTIES COMPILE_OPTIONS "-Wno-conversion")
elseif(MSVC)
  set_source_files_properties(${GE_LIBM_VENDORED} PROPERTIES COMPILE_OPTIONS "/wd4244;/wd4146;/wd4701;/wd4723")
endif()

ge_module(ge_core EXPORT core DIRS core loop SIMULATION)
