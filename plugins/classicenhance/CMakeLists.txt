# The classic content enhanced on the player's own machine: the classic packs the player already
# has (the shareware release zip as published, or the packs in an installed game folder, the full
# game's included), then enhanced into a local cache: sharper textures with generated normal,
# height, roughness and glow maps, and the lightmap stage rebaked light plugs into. Nothing it
# makes is committed or shipped (skill: classicenhance).
# Its app/ half (ge_classicenhance_app, with the GPU) runs the trained texture upscaler through
# compute shaders; the tool links it when it is built and runs the upscaler on the CPU otherwise.
ge_add_plugin(classicenhance ENGINE core assets audio lab vfs wind PLUGINS brush morph APP gpu)
if(TARGET ge_classicenhance_app)
  ge_embed_wgsl(ge_classicenhance_app app/upscaler.wgsl)
  target_link_libraries(classicenhance PRIVATE ge_classicenhance_app)
  target_compile_definitions(classicenhance PRIVATE GE_CLASSICENHANCE_GPU=1)
endif()
