# The bots' layer (engine/ai): the decision record a bot writes and `ge why` reads back, and
# the pieces bots share. Deterministic, no platform; published as ge::ai.
file(GLOB GE_AI_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/ai/*.cpp)
add_library(ge_ai STATIC ${GE_AI_SOURCES})
target_link_libraries(ge_ai PUBLIC ge_core)

ge_module(ge_ai EXPORT ai SIMULATION)
