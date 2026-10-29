# The view model (engine/viewmodel): the first-person held weapon's model form, its place in the
# view (offset, bob, sway, kick) and its light from the world round the player, with the CPU
# reference raster the labs draw it with. No GPU and no platform. Simulation code: its place and
# light feed lab digests and goldens. Published as ge::viewmodel.
file(GLOB GE_VIEWMODEL_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/viewmodel/*.cpp)
add_library(ge_viewmodel STATIC ${GE_VIEWMODEL_SOURCES})
target_link_libraries(ge_viewmodel PUBLIC ge_core ge_camera ge_assets)

ge_module(ge_viewmodel EXPORT viewmodel SIMULATION)
