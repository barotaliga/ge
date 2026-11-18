# The overview (engine/overview): a level seen from above for any game's spectators: the picture
# of its floors shaded on the CPU with its liquids as a layer of their own (plate.hpp), a model's
# mesh drawn small on the CPU as a figure or an icon (mesh_picture.hpp). No GPU, no platform; a
# game's map reader fills the floors and its own match drives what is drawn. Published as
# ge::overview.
file(GLOB GE_OVERVIEW_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/overview/*.cpp)
add_library(ge_overview STATIC ${GE_OVERVIEW_SOURCES})
target_link_libraries(ge_overview PUBLIC ge_core ge_ui)

ge_module(ge_overview EXPORT overview)
