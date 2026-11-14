---
type: Changed
title: A real face is laid with lay_real_face; web viewers 40% smaller
---

`generate_level` no longer looks a real face up by its id: set it with `lay_real_face(opts, id)` (`mountain/mapgen/real_hints.hpp`), which decodes the face and its hints into `GenerateOptions::real` and `real_hints`; a level keeps the hints as `Level::hints`, which the tarn's mapped lakes and streams read. A program that never lays a face (Fall Line, its web viewer) no longer links the baked faces' data: Fall Line's viewer is 4.8 MB, 1.8 MB gzipped, down from 7.7 MB and 3.0 MB.
