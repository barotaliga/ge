---
type: Added
title: The eye adapts to dark rooms
---

The exposure now follows what the eye sees: a new `eye` compute pass (`render/eye.hpp`) meters the frame into a log histogram on the GPU and opens the exposure past the sky's (up to 4 stops) where the world is darker than the sky lights it, opening over seconds in the dark and closing in a third of a second toward the light, so walking out of a hall into the day dazzles and settles. The sky's exposure stays the eye at rest, so outdoor scenes never pump and a frame that never runs the pass keeps its bits. Lab: `gpu-eye`.
