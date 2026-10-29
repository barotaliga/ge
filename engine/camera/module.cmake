# The camera (engine/camera, docs/camera.md): the pose, the followers, the subject, the rigs
# (first person, boom, mounted, orbit, planted, free), the shake, the transitions and the stack.
# No GPU and no platform: a game hands it a subject and the world's queries and gets a pose back
# (camera/to_view.hpp writes it into the renderer's FrameView, so that header is not installed).
# Simulation code: its poses feed lab digests and goldens. Published as ge::camera.
file(GLOB GE_CAMERA_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/camera/*.cpp)
add_library(ge_camera STATIC ${GE_CAMERA_SOURCES})
# Its cvars (camera/cvars.hpp) bind the player's settings on the console.
target_link_libraries(ge_camera PUBLIC ge_core ge_console)

ge_module(ge_camera EXPORT camera SIMULATION)
