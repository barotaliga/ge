---
type: Added
title: Players carry the weapon they hold
---

In the first classic game, every player is now drawn holding the weapon he has out, on servers that offer visible weapons and in recordings made on the server's side. Players without the server's weapon models get stand-ins made from their own pack: the player figure with its gun cut out, and each weapon's pickup model held in the hands and moving with them. `r_drawvweps 0` draws the figure's own gun as before (`cl_vwep` in Quake II), and `cl_novweps 1` stops asking servers for them.
