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
if(TARGET ge_shooter_app)
  target_link_libraries(ge_shooter_app PUBLIC ge_brush_app)
endif()
