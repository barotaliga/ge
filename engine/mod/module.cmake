# Mods (engine/mod, docs/mods.md): the C ABI a mod compiles against, the manifest, SHA-256, the
# loader and the one platform file that opens a shared library (dlopen, LoadLibraryW). Simulation
# code: a mod runs inside the match. Published as ge::mod.
file(GLOB GE_MOD_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/mod/*.cpp)
add_library(ge_mod STATIC ${GE_MOD_SOURCES})
target_link_libraries(ge_mod PUBLIC ge_core ${CMAKE_DL_LIBS})

ge_module(ge_mod EXPORT mod SIMULATION)
