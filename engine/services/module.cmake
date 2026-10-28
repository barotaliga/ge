# The platform services (engine/services, docs/services.md): the one interface a game uses for a
# store's achievements, stats, leaderboards, cloud files, presence, friends, overlay, ownership and
# screenshots, and the simulated and offline stores behind it; Discord's presence beside it over
# the Discord app's local pipe (ge_net). No platform, no SDK: a store's own backend (engine/steam,
# docs/goals.md M20b) is built beside it only when its SDK is supplied.
file(GLOB GE_SERVICES_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/services/*.cpp)
add_library(ge_services STATIC ${GE_SERVICES_SOURCES})
target_link_libraries(ge_services PUBLIC ge_core ge_net ge_input)

ge_module(ge_services EXPORT services)
