---
type: Added
title: The sequel's weapon switches on our server
---

The sequel's server now has three of its weapon-handling switches. With `g_select_empty 0` (the default), switching to a weapon with no ammunition is refused, and the player reads, for example, "No Shells for Shotgun." With `g_quick_weap 1` (the default), the next- and previous-weapon keys step on from the weapon already chosen, so repeated presses skip ahead, and the chosen weapon shows on the status bar for a moment. `g_swap_speed N` lowers and raises weapons N times faster.
