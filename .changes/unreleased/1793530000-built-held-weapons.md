---
type: Added
title: Held weapons built in Blender, with hands, animations and a staging lab
---

Shooter's Edge's held weapons are being rebuilt one at a time as built assets (`ge asset build`): detailed hard-surface models from a shared Blender kit (`content/models/view/weapon_kit.py`) with one baked texture each (procedural materials with edge wear, grain and cavity dirt), the same gloved hands and jointed arms on every weapon (shoulder, elbow and wrist, the elbow found by IK each frame), and a `fire` clip the hand plays on every shot. The axe is the first. The glTF loader can keep a model's base colour and emissive textures whole (`LoadOptions::keep_textures`), held models carry their clips (`held_with_clips`), and the new `se-weapon-stage` lab puts any candidate `.glb` beside the game's model of a weapon in first person, on a bench, in wire and as its texture, and writes each clip as an animated GIF (`encode_gif_frames`) and a WebM (`capture::write_clip`).
