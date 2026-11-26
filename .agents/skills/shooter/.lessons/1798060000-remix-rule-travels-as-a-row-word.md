---
title: A remixed item's own rule travels as one more word in the arsenal row
date: 2026-11-26
scope: games/shooters_edge/rules
concepts: [remix, cr-items, armour, health, ammo, arsenal-row, q2-body, q1-body, edicts]
---

- **The classic rules read another game's item from its `arsenal_*` setting.** A rule that
  differs (Quake II's armour adding up to a most, a box's own amount) is one more word at the end
  of that row (`ArmourRow::most`, `ItemLook::amount`), absent for the rules' own items, so the
  played game's own play and its digests never change. The armour item keeps the most in its
  unused `max_health`.
- **A test that changes a player's health on the classic rules writes the engine's edict**
  (`sv.edicts[1].health`), not the program's field: the field is filled from the edict each frame,
  so a write to it is lost. Read the program's fields by name (`find_field`), never by the
  multiplayer layout's offsets.
