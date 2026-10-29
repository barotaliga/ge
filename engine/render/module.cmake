# The frame core (engine/render, M7; built on ge_gpu, so only with GE_GPU): the pass table's
# mechanisms, the sky, the terrain and its horizon map, the Hi-Z pyramid, TAA-U, post, the
# instance families and their meshes, the sun's shadows. Not published yet.
ge_module(ge_render)
if(GE_GPU)
  file(GLOB GE_RENDER_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/render/*.cpp)
  add_library(ge_render STATIC ${GE_RENDER_SOURCES})
  target_link_libraries(ge_render PUBLIC ge_gpu ge_assets ge_ui ge_water ge_grass)
  ge_embed_wgsl(ge_render engine/render/sky_tables.wgsl engine/render/sky.wgsl
    engine/render/terrain.wgsl engine/render/terrain_cull.wgsl engine/render/terrain_normals.wgsl
    engine/render/horizon.wgsl engine/render/hiz.wgsl engine/render/taa.wgsl engine/render/post.wgsl
    engine/render/instances.wgsl engine/render/instances_cull.wgsl engine/render/bloom.wgsl
    engine/render/skinning.wgsl engine/render/particles.wgsl engine/render/particles_sim.wgsl engine/render/precip.wgsl engine/render/precip_sim.wgsl
    engine/render/clouds.wgsl engine/render/water.wgsl engine/render/water_copy.wgsl
    engine/render/water_stream.wgsl
    engine/render/underwater.wgsl engine/render/wet_lens.wgsl engine/render/grass.wgsl engine/render/grass_scatter.wgsl
    engine/render/ui_pass.wgsl engine/render/eye.wgsl engine/render/eye_adapt.wgsl engine/render/explosions.wgsl)
endif()
