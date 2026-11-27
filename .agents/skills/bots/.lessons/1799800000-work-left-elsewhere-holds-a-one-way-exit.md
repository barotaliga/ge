---
title: Steps that cannot be done keep the exit and the next step out of reach
date: 2026-11-27
scope: plugins/shooterbots/way_through.cpp, plugins/shooterbots/fighter.cpp, plugins/shooterbots/way_parts.cpp
concepts: [strict-work, round-trip, dooms, given-up, way-forever, climb-out, poke, arm, hangar1, biggun]
---

- `reach` only counts nodes he can come back from (`round_trip`) while any step is still open
  (`strict_work`). A one-way exit, like biggun's past its drop, stays out of reach until every
  open step is done. Filtering a step out of the pick is not enough: it must leave `strict_work`
  too. Otherwise the plan explores instead of taking the exit. After a `dooms` button, nothing
  counts as work.
- The trigger strips the countdown arms on biggun look like work, but they only drop crates and
  set off blasts. On main the bot touched them on the way out. Skipping them without clearing
  `strict_work` killed him at 368 s.
- A step left twice for "stood still" is gone for good (`kWayForever`), even if the reason was
  being stuck elsewhere, such as a climb out of a pool that kept failing. `given_from` records
  where it was given up. `way_forgive` gives it one more try from at least `kWayGivenFar` away.
- `w.kind` is overwritten by `arm` when he goes for a gun. The hands' press at the top of
  `way_plan` read `w.kind`, so a shot wall between him and the gun was never shot. Read the
  step's own kind.
- A shot door stays open about 3 s. Holding the poke for `kPressFor_s` (4 s) kept him still in
  front of it until it shut. Let go once the step is done. Leave the calm box out of this
  (`way_shot_door` only): letting go of hangar1's duct box early left him stuck against the fan.
- A lab's `--seconds` changes the plan (the time it has left), so a short debug run is not the
  same run as the 900 s one. Probe with the same flags.
- To tell whether a shot hit a door, print `map_damage` in `q2_map.cpp`: entity, damage and
  health. `sv.edicts[e].health` does not hold the Quake II door's health.
