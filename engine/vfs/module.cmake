# Content files (engine/vfs): the search path over game directories, packs, zips (stored and
# deflated, through core's inflater) and loose files; the wad and its pictures; the palette and
# the colour map.
file(GLOB GE_VFS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/vfs/*.cpp)
add_library(ge_vfs STATIC ${GE_VFS_SOURCES})
target_link_libraries(ge_vfs PUBLIC ge_core)

ge_module(ge_vfs EXPORT vfs)
