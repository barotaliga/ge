# Content files (engine/vfs): the search path over game directories, packs, zips (stored,
# deflated through core's inflater, LZMA and zstd) and loose files; the wad and its pictures; the
# palette and the colour map; the one file inside packed or archived bytes (gzip, zstd, xz, bzip2,
# tar, zip, 7z, LHA, RAR) and the zstd stream a recording is packed with. The codecs (zstd, the LZMA
# SDK, bzip2, unRAR) are cmake/Archives.cmake's object library, carried inside ge_vfs so the package
# exports nothing more.
include(cmake/Archives.cmake)
file(GLOB GE_VFS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/vfs/*.cpp)
add_library(ge_vfs STATIC ${GE_VFS_SOURCES} $<TARGET_OBJECTS:ge_vfs_codecs>
  $<TARGET_OBJECTS:ge_vfs_unrar>)
target_link_libraries(ge_vfs PUBLIC ge_core)
target_include_directories(ge_vfs SYSTEM PRIVATE ${GE_ZSTD_INCLUDE} ${GE_LZMA_INCLUDE}
  ${GE_BZIP2_INCLUDE})

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/engine/vfs/rar.cpp PROPERTIES
  INCLUDE_DIRECTORIES "${GE_UNRAR_INCLUDE}")
if(NOT WIN32)
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/engine/vfs/rar.cpp PROPERTIES
    COMPILE_DEFINITIONS "RARDLL;_FILE_OFFSET_BITS=64;_LARGEFILE_SOURCE")
else()
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/engine/vfs/rar.cpp PROPERTIES
    COMPILE_DEFINITIONS "RARDLL")
endif()

ge_module(ge_vfs EXPORT vfs)
