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
ge_add_plugin(classic ENGINE core assets audio net save vfs PLUGINS shooter brush classicenhance
  morph progs
  APP gpu render water wind console)
if(TARGET ge_classic_app)
  target_link_libraries(ge_classic_app PUBLIC ge_brush_app)
endif()
