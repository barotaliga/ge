# Fall Line, the demo game. Its simulation lands with M5 (games/fall_line/sim); what is built
# now is its app shell: `fall_line`, a window drawing the GPU test scene at vsync until the
# frame core draws the mountain (M7). It needs the platform and the GPU.
if(GE_PLATFORM AND GE_GPU)
  add_executable(fall_line app/main.cpp)
  target_link_libraries(fall_line PRIVATE ge_platform ge_gpu)
endif()
