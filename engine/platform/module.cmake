# The platform layer (engine/platform, GE_PLATFORM, cmake/SDL.cmake): the window, input, the
# frame's clock and the audio device over SDL3. Optional, and nothing else depends on it: the
# simulation, the plugins and their tests build without it. Not published yet (a consumer would
# need SDL3 from the package too).
ge_module(ge_platform)
if(GE_PLATFORM)
  add_library(ge_platform STATIC engine/platform/window.cpp engine/platform/audio.cpp
    engine/platform/devices.cpp)
  target_link_libraries(ge_platform PUBLIC ge_core ge_input PRIVATE SDL3::SDL3-static)
endif()
