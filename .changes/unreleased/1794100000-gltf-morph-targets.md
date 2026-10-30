---
type: Added
title: glTF morph targets
---

The glTF loader reads each mesh's morph targets, its default weights and the clips' weights channels, `assets/pose.hpp` blends them into a mesh's bind before skinning (`sample_weights`, `morph_mesh`), and `write_glb` writes them back; the `models` lab draws a vertex-animated model's clips frame by frame.
