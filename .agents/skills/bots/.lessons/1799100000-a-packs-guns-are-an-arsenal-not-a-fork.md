---
title: A mission pack's guns are an arsenal picked by the mod folder, and its ammunition is read off the program, not the status bar
date: 2026-11-25
scope: games/shooters_edge/bots/arsenals.cpp, plugins/shooterbots/bot_arsenal.hpp, games/shooters_edge/server/rules_world.cpp
concepts: [bot-arsenal, mission-pack, aim-slot, arsenal-for-mod, stock, rogue, hipnotic]
---

- A pack's guns are rows in an arsenal of its own (`arsenal_for_mod`), never new branches in the
  fighter. Pack impulses lie past the base numbers (225, 60..64), so each row says its
  `aim_slot`; a row without one would be aimed by its impulse and index out of the traits.
- Check the base rows' bits against the pack's: Dissolution moves the axe to 2048 because its
  4096 is the lava nailgun. Quake's arsenal on that pack read the lava gun as a blade.
- When a program shows a display copy of the ammunition (Dissolution's four status-bar numbers
  switch meaning with the gun held), read the true stock off the program's own fields on the
  server (`OptionalFields::stock` into `PlayerNumbers::stock`): a reader that guesses from the
  held gun forgets the other half every switch.
- Teaching a bot a pack's ITEMS can cost more than its guns earn: Scourge's proximity gun as a
  weapon to fetch, and Dissolution's three ammunition boxes as things to fetch, each drew the bots
  from the fight (hipdm1 19 frags a match to 12; ctf1 9 to 3). Find it by ablation (the base
  arsenal with one piece of the pack's put back at a time, several seeds), then leave the lure out
  of the item table: the guns still fire from what is taken in passing.
- Long debug-build measurements die with the container: start them with the tool's background
  mode so a restart is noticed, and keep them short (a few minutes of game time).
