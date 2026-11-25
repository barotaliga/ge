---
title: A pack's rows grow every table indexed by the base game's enums: grep their counts first
date: 2026-11-23
scope: games/shooters_edge/server, games/shooters_edge/bots, games/shooters_edge/labs
concepts: [mission-pack, q2-pack, enum-growth, item-count, weapon-count, sqammo, asan, digest]
---

The Reckoning appended weapons, shots, ammunitions and items to Quake II's tables. Every loop over
`kSqItemCount` or `kSqWeaponCount` outside the server then met rows it did not know: the bots'
item table indexed a 7-wide array by `SqAmmo` (a stack overflow only ASan caught, in the nav, unit
and listen tests, not the pack's own test), and the fight lab looked pack weapons up without their
pack. Before the first build, `grep -rn "kSqItemCount\|kSqWeaponCount\|SqAmmo" games plugins` and
give each hit a rule: skip rows whose `pack` is not base (bots, base-only labs, so digests hold),
or look the row up by its own pack (`sq_item(name, row.pack)`). Saves of such tables are written
length-first (q2_save.cpp `grown`) so the next pack's rows load old saves.
