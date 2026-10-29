# Saves (engine/save, docs/saves.md): the save file (its header, chunks, directory and
# checksums) and the state described once for its writer and reader. No platform and no clock;
# published as ge::save.
file(GLOB GE_SAVE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/save/*.cpp)
add_library(ge_save STATIC ${GE_SAVE_SOURCES})
target_link_libraries(ge_save PUBLIC ge_core)

ge_module(ge_save EXPORT save SIMULATION)
