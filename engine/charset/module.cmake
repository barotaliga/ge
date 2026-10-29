# The engine's bitmap fonts (engine/charset, docs/charsets.md): ten monospaced ASCII charsets, the
# atlas builder and the effect layers, on the CPU; any renderer uploads the atlases. Simulation
# code: its atlases and looks feed lab digests and goldens. Published as ge::charset.
file(GLOB GE_CHARSET_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/charset/*.cpp)
add_library(ge_charset STATIC ${GE_CHARSET_SOURCES})
target_link_libraries(ge_charset PUBLIC ge_core)

ge_module(ge_charset EXPORT charset SIMULATION)
