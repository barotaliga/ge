---
type: Added
title: Authored models: the glTF loader, posing and GPU skinning
---

`engine/assets` (published as `ge::assets`) loads a glTF 2.0 model (.glb, or .gltf with its buffers) into a `Model` in the world's frame: every node a joint, every mesh skinned (a rigid part bound whole to its node), base colours as materials, clips baked at 60 Hz from STEP, LINEAR and CUBICSPLINE samplers, and what it skips named in its notes. `assets/pose.hpp` samples a clip, builds the bone palette and skins on the CPU without allocating; `render/skinning.hpp` poses a skinned mesh on the GPU in a compute pass that writes straight into an instance family's vertices, so the cull, the LODs and the shadow casters draw it unchanged. The new `models` lab shows any model at rest, each clip as a filmstrip and an onion skin, and its joints' angles and angular acceleration graphed over time, so a pop or a jolt shows as a spike.
