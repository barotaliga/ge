---
title: A rigged, animated character in GLB is in the free kits; the larger rigged ones are FBX only
date: 2026-10-24
scope: games/shooters_edge/content/models
concepts: [model, character, rig, skin, animation, glb, player]
---

The larger free animated characters ship as FBX only, which needs the Blender path to become a
GLB. The small-character kits ship GLB with a skin and named clips (idle, walk, sprint, jump,
fall, crouch, die, pick-up, holding): one loads straight into the `models` lab and plays every
clip there. Look for a GLB kit with `skins` and `animations` in its JSON chunk before
converting anything.
