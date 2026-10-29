# The native GPU device over Dawn (engine/gpu, GE_GPU, cmake/Dawn.cmake), with its shaders
# embedded (cmake/Wgsl.cmake). Optional, and nothing in the simulation depends on it. Not
# published yet (a consumer would need Dawn from the package too).
ge_module(ge_gpu)
if(GE_GPU)
  file(GLOB GE_GPU_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/gpu/*.cpp)
  add_library(ge_gpu STATIC ${GE_GPU_SOURCES})
  target_link_libraries(ge_gpu PUBLIC ge_core dawn::webgpu_dawn)
  ge_embed_wgsl(ge_gpu engine/gpu/test_scene.wgsl)
endif()
