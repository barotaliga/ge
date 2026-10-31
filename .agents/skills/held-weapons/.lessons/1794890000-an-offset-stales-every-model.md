---
title: Moving a weapon's offset in weapons.cpp stales every built model; rebuild them in the same PR
date: 2026-10-31
scope: games/shooters_edge/view/weapons.cpp, games/shooters_edge/assets/view_data.cpp, games/shooters_edge/content/models/view
concepts: [held-weapon, offset, placement, se_view, stamp, assets_check, bpy, rebuild]
---

Each weapon's offset from the eye is part of the `se_view` data set, because the builder's arm
reaches from the fixed shoulder to the grip at that offset. Lowering the weapons therefore made all
eight `.glb` stamps stale, and `assets_check` failed on CI even though `make precheck` passed
locally. After any offset, shoulder or arm change, run `ge asset build <name>` for every weapon
and commit the models and stamps with the change. In the container, Blender is the bpy wheel, and
the kit imports `mathutils` before `bpy`. A `sitecustomize.py` that runs `import bpy`, placed on
`PYTHONPATH` ahead of the bpy target, makes the builders run unchanged:
`GE_BLENDER=python3.13 PYTHONPATH=<site>:<bpy> build/debug/ge asset build <name>`. The rebuilt arms
then move the `se-viewmodel` and `se-weapon-stage` digests.
