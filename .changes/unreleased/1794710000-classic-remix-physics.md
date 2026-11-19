---
type: Changed
title: Classic remix's cr_physics carries the body's whole physics, and extra weak weapons become ammunition
---

`cr_moves` is now `cr_physics`, and it brings everything of the other game's physics but what a weapon does: besides running, friction, the air and the landing, swimming (the swimmer's speed, the water's drag and the jump out onto a bank) and the harm from a hard fall. The classic game playing by Quake II's physics hurts a hard landing as Quake II does; Quake II playing by the classic game's hurts nobody for a fall.

A remixed map keeps at most two copies of one weak weapon (`cr_weak_same`); the rest become boxes of its ammunition, so a map no longer fills with shotguns.

New server settings on the classic rules: `pm_swimscale`, `pm_waterjump` and the arena's `sv_falldamage`.
