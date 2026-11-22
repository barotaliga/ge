---
type: Added
title: Console paste, copy and selection; SELECT GAME remade; classic loading in the console; ce_enabled
---

The console pastes the clipboard (Command-V or Ctrl-V, line breaks as `;`) and copies text selected with the mouse (Command-C). SELECT GAME lists only the games added on this computer, each by its name and edition, the game playing in colour and the one classic remix plays with half; ADD GAME finds another. A classic game shows its loading in its own console over its own background ("Loading base1... 3.6s", rewritten as it goes), switching games prints each step, and Quake II's quit asks on its own tag alone. `ce_enabled 0` turns classic enhance off whole, each `ce_` switch kept as set. `bind_defaults` puts every key back to the game's layout. `scr_folder` takes a whole path (`~/Desktop/quake`). For pictures of one spot: `viewfrom` prints where you look from (and the game and map), `r_viewfrom` draws the world from there, `setpos` (a cheat) stands the player there, and `-hidden` runs the window without showing it.
