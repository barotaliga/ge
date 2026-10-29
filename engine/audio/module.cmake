# The synth (M9): plain C++ over the core's libm and stream, so it builds, renders and is
# metered with no platform; engine/platform's audio device plays it.
file(GLOB GE_AUDIO_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/audio/*.cpp)
add_library(ge_audio STATIC ${GE_AUDIO_SOURCES})
target_link_libraries(ge_audio PUBLIC ge_core)

ge_module(ge_audio EXPORT audio SIMULATION)
