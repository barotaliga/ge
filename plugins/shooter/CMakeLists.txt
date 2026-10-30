# The first-person shooter's simulation half: the classic player move in 32-bit floats over clip
# hulls (the box hull, point contents, the hull trace), and the recorded scenarios it is held to.
# Any first-person shooter on the engine links it (skill: shooter).
# Its labs also draw (APP: the `gpu-plunge` lab's pond, its water and the view through it); the
# simulation target links core, ai (the nav graph searches on ai/path), move and water (the
# engine mover's classic row), and the brush plugin
# (bsp_world: a read map's clip hulls as the server's brush models).
ge_add_plugin(shooter ENGINE core ai move water PLUGINS brush APP gpu render water wind)
