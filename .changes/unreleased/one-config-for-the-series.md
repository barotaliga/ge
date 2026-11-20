---
type: Added
title: One config for every classic game, in our own setting names
---

Shooter's Edge keeps the player's settings and binds in one config for the whole series, `<data folder>/settings/config.cfg`, written in our own setting names; each game's own spellings (Quake II's `vid_gamma`, `s_volume`, `cl_gun` and kin) reach the same settings, so a game's own configs still work. Beside each game, its folder's `settings/config.cfg` keeps only what that game means differently by a shared name, such as Quake II's crosshair pictures, so Quake's crosshair stays its own. A game folder's older config is read once and then written back split.
