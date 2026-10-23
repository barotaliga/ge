---
title: A blend into a worn lens flies through the body; picture it, then clip past the head
date: 2026-10-23
scope: games/fall_line/camera/subject.cpp, games/fall_line/labs/gpu_camera_world_lab.cpp
concepts: [hand-over, blend, worn-lens, helmet, near-plane, camera-world]
---

A blend from a boom behind the skier to his helmet's eye moves the eye in a line that passes
through the back of his head, and with a near plane of 0.1 m the frame two thirds of the way in
is the inside of the helmet's shell. Only the strip of the hand-over in `camera-world` showed it;
every number was fine. The game knows where the head is, so while a lens off the helmet rung is
within 0.6 m of the head's centre it pushes the near plane out past the head (the distance plus
0.22 m), and the lens looks through the head on the way in. Shoot every hand-over as a strip.
