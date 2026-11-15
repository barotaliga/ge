# The first-person shooter's simulation half: the classic player move in 32-bit floats over clip
# hulls (the box hull, point contents, the hull trace), and the recorded scenarios it is held to.
# Any first-person shooter on the engine links it (skill: shooter).
# Its labs also draw (APP: the `gpu-plunge` lab's pond, its water and the view through it); the
# simulation target links core, ai (the nav graph searches on ai/path), move and water (the
# engine mover's classic row), and the brush plugin
# (bsp_world: a read map's clip hulls as the server's brush models). Its content/ is a classic
# game's files for any shooter game: the search path over an install and each game's load order
# (save: the save file; progs: the release's words by key) and the open calls every shooter game
# shares (content/open.hpp: maps, models by kind through morph and assets, sounds through audio).
# Its app/ half also holds the picture every shooter on classic maps shares (app/look.hpp: r_look,
# the r_ and ce_ cvars with a game's defaults, the world's and the post's options): it draws
# through the brush plugin's renderer (ge_brush_app, linked below) and reads the classic enhance
# switches (PLUGINS classicenhance).
ge_add_plugin(shooter ENGINE core ai assets audio console input move water net save vfs PLUGINS brush
  classicenhance morph progs
  APP gpu render water wind console)
# The soundtrack store's Ogg Vorbis reader (content/music_ogg.cpp, the only file that includes it):
# stb_vorbis (public domain or MIT, one file) at the commit cmake/Ui.cmake pins stb by, checked by
# its SHA-256. Moving the pin: change both, then rerun tests/shooter_content_import_test.cpp.
include(${PROJECT_SOURCE_DIR}/cmake/Ui.cmake)
set(GE_STB_VORBIS_SHA256 "4c7cb2ff1f7011e9d67950446b7eb9ca044f2e464d76bfbb0b84dd2e23e65636")
ge_fetch_file("stb_vorbis.c"
  "https://raw.githubusercontent.com/nothings/stb/${GE_STB_COMMIT}/stb_vorbis.c"
  ${GE_STB_VORBIS_SHA256} "${GE_STB_INCLUDE}/stb_vorbis.c")
# Built as C on its own, with its warnings off, and carried inside ge_shooter.
enable_language(C)
add_library(ge_stb_vorbis OBJECT "${GE_STB_INCLUDE}/stb_vorbis.c")
target_compile_definitions(ge_stb_vorbis PRIVATE STB_VORBIS_NO_STDIO STB_VORBIS_NO_PUSHDATA_API)
target_compile_options(ge_stb_vorbis PRIVATE $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>)
set_target_properties(ge_stb_vorbis PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_sources(ge_shooter PRIVATE $<TARGET_OBJECTS:ge_stb_vorbis>)
if(TARGET ge_shooter_app)
  target_link_libraries(ge_shooter_app PUBLIC ge_brush_app)
endif()
