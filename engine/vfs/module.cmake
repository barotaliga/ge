# Content files (engine/vfs): the pack archive's checked directory and its files as views, and
# zips (stored and deflated, read through ge_assets' inflate); the rest of the virtual file
# system (search paths, directories) lands with docs/goals.md M18a.
file(GLOB GE_VFS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/vfs/*.cpp)
add_library(ge_vfs STATIC ${GE_VFS_SOURCES})
target_link_libraries(ge_vfs PUBLIC ge_core ge_assets)

ge_module(ge_vfs EXPORT vfs)
