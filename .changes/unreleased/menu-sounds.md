---
type: Fixed
title: the main menu clicks again
---

Shooter's Edge's main menu was silent. It now plays the loaded game's own menu sounds: a click as the cursor moves, another when the menu opens or a page or item is entered, and the back sound when a setting changes, a page is left or the menu closes. The engine's menu model gives any game the cue for a frame's presses (`ui::menu_cue`), and the built-in arena plays our own UI sounds under the same names.
