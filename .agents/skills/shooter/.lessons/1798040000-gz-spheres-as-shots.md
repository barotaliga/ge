---
title: A pack's companion that floats by its owner is a shot row of its own kind, not a new entity list
date: 2026-11-23
scope: games/shooters_edge/server
concepts: [sphere, defender, hunter, vengeance, missile, shot-kind, blow-from, huntercam, power-table]
---

- **A floating companion is a `Q2Missile`.** Ground Zero's spheres take a shot kind each
  (`SqShot::*_sphere`): the missile rows already give a model by kind, effects, render bits, a
  loop sound and the snapshot, and `gz_missile` runs their whole frame (return true), so no new
  entity list or wire field was needed. Their drawn angles come from `Q2Missile::facing`, not the
  velocity. A new shot kind must be added to `kSqShotEffects`, `kSqShotRender`, `kSqShotPacks`,
  `kMissileModels` and the client's `demo_mvd2.cpp` switch (`-Wswitch` catches the last).
- **A monster's blow on a player says who dealt it only through `Q2Server::blow_from`.** `hurt`
  takes a slot or -1; a monster's melee, bullet, hurt-foe, missile hit and blast set
  `FightBlowFrom` (q2_fight_kit.hpp) so a rule that must know the monster (the defender's target)
  reads `-2 - blow_from`.
- **A timer on the bar is a power-table row with no tint.** The spheres' rows in `q2_powers.cpp`
  have alpha 0 (the blend's `lay` skips it), no warning and no drop; `use` must catch the sphere
  items before the table's `extend`, which would only lengthen a timer.
- **A dead player's view moved by the rules** needs the move frozen (`run_command` gives
  `Q2Type::freeze` while `hunter_cam` is set), the origin packed each frame, `dead_angles` turned
  and the snapshot's fov and view offsets overridden.
