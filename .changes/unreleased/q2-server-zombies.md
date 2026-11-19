---
type: Added
title: Quake II's server holds a dropped player's slot for two seconds
---

When a player leaves or is dropped from Quake II's server, their slot keeps resending the goodbye for `sv_zombietime` seconds (2 by default; Quake II's `zombietime` works too) after their last packet, and `status` shows it as `ZMBI`. Only then can someone else take the slot.
