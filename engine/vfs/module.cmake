# Content files (engine/vfs): the search path over game directories, packs, zips (stored,
# deflated through core's inflater, LZMA and zstd) and loose files; the wad and its pictures; the
# palette and the colour map; the one file inside packed or archived bytes (gzip, zstd, xz, tar,
# zip, 7z) and the zstd stream a recording is packed with. The codecs (zstd, the LZMA SDK) are
# cmake/Archives.cmake's object library, carried inside ge_vfs so the package exports nothing more.
include(cmake/Archives.cmake)
file(GLOB GE_VFS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/vfs/*.cpp)
add_library(ge_vfs STATIC ${GE_VFS_SOURCES} $<TARGET_OBJECTS:ge_vfs_codecs>)
target_link_libraries(ge_vfs PUBLIC ge_core)
target_include_directories(ge_vfs SYSTEM PRIVATE ${GE_ZSTD_INCLUDE} ${GE_LZMA_INCLUDE})

ge_module(ge_vfs EXPORT vfs)
