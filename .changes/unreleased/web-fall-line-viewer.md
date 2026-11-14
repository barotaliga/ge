---
type: Added
title: Fall Line's runs are watched again in a browser, off their input logs
---

Fall Line's web viewer (`fall_line_viewer`, built by the `web` preset with `-DGE_WEB_GAME=fall_line`) takes `--run=<url>`, a run's input log. It grows the mountain again from the log's seed in the page, steps the run with the replay's player and draws the game's own frame from the camera `--view=<name>` names (`chase`, `high`, or one of the map's views). On the canvas, Space or K pause, Left and Right skip five seconds, Up and Down change the speed, 1 and 2 switch between the chase and the high camera, and Home goes back to the start. Fall Line's frame now fits WebGPU's default limits: the terrain and the fog bind lean parts of the frame's group 0 (`render::kFrameGround`, `render::kFrameFog`). On a device that offers fewer than 18 sampled textures a stage, the snow's material gives way to the default ground material. `encode_fog` and the terrain's encoders take a `render::FrameGroup`. `app::run_camera` is public.
