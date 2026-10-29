# Input devices (engine/input, docs/input.md): the device model, the events and the state they
# fold into, dead zones, actions and bindings, rebinding, the device database, touch controls,
# players, feedback and force-feedback effects, the event log and the simulated backend. No
# platform: engine/platform's backend feeds it events, and the simulated one runs anywhere.
file(GLOB GE_INPUT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/input/*.cpp)
add_library(ge_input STATIC ${GE_INPUT_SOURCES})
target_link_libraries(ge_input PUBLIC ge_core)

ge_module(ge_input EXPORT input)
