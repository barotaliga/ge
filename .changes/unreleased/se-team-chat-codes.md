---
type: Added
title: Team messages fill in your health, armour and weapon
---

In Shooter's Edge, `say` and `say_team` fill in the codes teamplay configs use: `%h` health, `%a` armour, `%A` its class, `%w` the weapon in hand and its ammunition, `%b` the best weapon, `%p` powerups, `%u` what you need, `%l` and `%d` places, gold brackets round a low figure with `%[h]`, and widths such as `%<5r>h`. Fun characters (`$[`, `$]`, the gold digits, the lights, `$x41`) become the classic font's glyphs. A config's `f_newmap`, `f_spawn`, `f_weaponchange`, `f_death` and `f_respawn` aliases run on those events.
