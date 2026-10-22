# The generated ski mountain: the world generator (mapgen) and the rule book it is held to
# (analysis). Any game on a generated snow mountain links it (skill: mountain).
# Its app/ half (ge_mountain_app, with the GPU) is the mountain as drawn: the snow's material.
ge_add_plugin(mountain ENGINE core APP render gpu)
if(TARGET ge_mountain_app)
  ge_embed_wgsl(ge_mountain_app app/snow.wgsl)
endif()
