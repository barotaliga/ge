---
type: Added
title: The held weapon's muzzle
---

A held model names its muzzle's tip (`muzzle_vertices`, found on the barrel's front by `find_muzzle`) and the way the shot leaves; `muzzle_at` follows it through every morph frame, and `held_to_world` / `held_dir_to_world` carry it into the world through the gun's own lens, so a muzzle flash, its smoke or its light drawn in the world sits on the barrel. A model's own flash triangles (`flame`) can be hidden (`DrawLit::hidden`) when the engine draws the flash instead.
