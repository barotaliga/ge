# The 2D layer (engine/ui, the `ui` skill): paints, the draw list, images, fonts over
# stb_truetype with the built-in family compiled in (cmake/Ui.cmake), the CPU raster, backdrops,
# and menus as a model and a view. No GPU and no platform: the labs picture it headless, and the
# GPU pass (M22b) draws the same draw list. Published as ge::ui.
include(cmake/Ui.cmake)
include(cmake/EmbedBinary.cmake)
file(GLOB GE_UI_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/ui/*.cpp)
add_library(ge_ui STATIC ${GE_UI_SOURCES})
target_link_libraries(ge_ui PUBLIC ge_core ge_console ge_input)
target_include_directories(ge_ui SYSTEM PRIVATE ${GE_STB_INCLUDE})
ge_embed_binary(ge_ui ge_ui_font_semibold "${GE_FONTS_DIR}/BarlowCondensed-SemiBold.ttf")
ge_embed_binary(ge_ui ge_ui_font_medium "${GE_FONTS_DIR}/BarlowCondensed-Medium.ttf")

ge_module(ge_ui EXPORT ui)
