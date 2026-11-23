---
type: Added
title: Quake II's Ground Zero plays its own weapons, mines, A-M bomb and double damage
---

A game folder named `rogue` beside Quake II's base folder now plays Ground Zero's rules on Quake II's server: the chainfist, the ETF rifle, the prox launcher and its mines, the plasma beam, the disruptor and its tracking bolt, the thrown tesla, the A-M bomb used from the inventory, double damage (stacking with the quad in deathmatch unless `dmflags` 262144), the infra-red goggles, the pack's item list and numbers, its obituaries, and the weapon a dry body turns to. Its settings `strong_mines` and `g_disruptor` and its `dmflags` 131072, 524288 and 1048576 are read only while it plays. The base game is unchanged, and it again draws the weapon in hand when the mission packs' weapon models are not in its files. The `se-q2-weapon-fx` lab fires the pack's weapons on its maps with `--pack=rogue`.
