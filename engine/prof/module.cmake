# The profiler (engine/prof): the timing zones any code opens with GE_ZONE, the allocation
# counts and the resident set, every thread's timeline, the OS's thread and core sampler and the
# load analysis over them. The counting operator new is its own target, ge_prof_alloc: an
# executable that links it counts every allocation (the play executable does).
add_library(ge_prof STATIC engine/prof/zones.cpp engine/prof/memory.cpp engine/prof/timeline.cpp
  engine/prof/cpu_sampler.cpp engine/prof/load.cpp)
target_link_libraries(ge_prof PUBLIC ge_core)
if(WIN32)
  target_link_libraries(ge_prof PRIVATE psapi)
endif()
add_library(ge_prof_alloc STATIC engine/prof/alloc_hook.cpp)
target_link_libraries(ge_prof_alloc PUBLIC ge_prof)

ge_module(ge_prof EXPORT prof)
ge_module(ge_prof_alloc EXPORT prof_alloc OPT_IN)
