# The HUD kit (engine/hud, docs/hud.md): any game's overlay over its running picture, as widgets
# over the 2D layer's draw list, in TrueType or bitmap type: the frame and anchors, readouts,
# meters, pips, banners, the feed and the layout check. No GPU: the CPU raster draws it in the
# labs. Published as ge::hud.
file(GLOB GE_HUD_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/hud/*.cpp)
add_library(ge_hud STATIC ${GE_HUD_SOURCES})
target_link_libraries(ge_hud PUBLIC ge_ui ge_charset)

ge_module(ge_hud EXPORT hud)
