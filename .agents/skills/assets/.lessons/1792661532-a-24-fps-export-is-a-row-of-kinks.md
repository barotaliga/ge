---
title: A clip exported at Blender's default 24 frames a second shows a spike in the models lab's acceleration every few frames, which is the export, not the loader
date: 2026-10-22
scope: engine/lab/labs/assets_models_lab.cpp, engine/assets/gltf.cpp
concepts: [animation, blender, export, clip-bake, jolt]
---

A three-bone arm waved by four Bezier keys in Blender 5.2 and exported with the glTF
exporter's defaults comes in as LINEAR samples at the scene's 24 frames a second. Baked at
60 Hz, each sample is a kink, and the `models` lab's angular acceleration graph shows a spike
every 2.5 baked frames (peaks of about 2500 deg/s2 on a 50-degree wave). The same motion keyed
at 60 frames a second, or exported as CUBICSPLINE, is smooth. First check: read the clip's
frame count against its duration in the lab's `clips` table. Measuring the turn as a rotation
vector (`turn_from_rest`) matters too: graphing the second difference of the unsigned angle put
a false spike wherever a swing passed through its rest.
