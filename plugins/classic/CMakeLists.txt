# Playing the classic games (skill: shooter): what only an imported classic game needs on top of
# the shooter kit (plugins/shooter), so a shooter of our own links the kit without it. Its
# content/ is a classic game's files: the search path over an install and each game's load order
# (save: the save file; progs: the release's words by key; classicenhance: the content row) and
# the open calls (maps through brush, models by kind through morph and assets, sounds through
# audio). Its protocol/ is the classic protocol family every classic game's own protocol shares:
# the net channel, the info strings, the wire's fields, quantization and CRC-16 (net).
# Its app/ half is the picture every shooter on classic maps shares (app/look.hpp: r_look, the r_
# and ce_ cvars with a game's defaults, the world's and the post's options), drawn through the brush
# plugin's renderer (ge_brush_app, linked below) with the classic enhance switches.
ge_add_plugin(classic ENGINE core assets audio console net overview save server ui vfs PLUGINS shooter shooterbots brush
  classicenhance morph progs
  APP gpu render water wind console)
if(TARGET ge_classic_app)
  target_link_libraries(ge_classic_app PUBLIC ge_brush_app)
endif()
# The soundtrack store's Ogg Vorbis reader (content/music_ogg.cpp, the only file that includes it):
# stb_vorbis (public domain or MIT, one file) at the commit cmake/Ui.cmake pins stb by, checked by
# its SHA-256. Moving the pin: change both, then rerun tests/classic_content_import_test.cpp.
include(${PROJECT_SOURCE_DIR}/cmake/Ui.cmake)
set(GE_STB_VORBIS_SHA256 "4c7cb2ff1f7011e9d67950446b7eb9ca044f2e464d76bfbb0b84dd2e23e65636")
ge_fetch_file("stb_vorbis.c"
  "https://raw.githubusercontent.com/nothings/stb/${GE_STB_COMMIT}/stb_vorbis.c"
  ${GE_STB_VORBIS_SHA256} "${GE_STB_INCLUDE}/stb_vorbis.c")
# Built as C on its own, with its warnings off, and carried inside ge_classic.
enable_language(C)
add_library(ge_stb_vorbis OBJECT "${GE_STB_INCLUDE}/stb_vorbis.c")
target_compile_definitions(ge_stb_vorbis PRIVATE STB_VORBIS_NO_STDIO STB_VORBIS_NO_PUSHDATA_API)
target_compile_options(ge_stb_vorbis PRIVATE $<IF:$<C_COMPILER_ID:MSVC>,/w,-w>)
set_target_properties(ge_stb_vorbis PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_sources(ge_classic PRIVATE $<TARGET_OBJECTS:ge_stb_vorbis>)
