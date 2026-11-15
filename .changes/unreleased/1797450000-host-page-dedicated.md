---
type: Added
title: Shooter's Edge's NEW GAME page can start a dedicated server
---

The NEW GAME page has a SERVER row with two choices, THIS WINDOW and DEDICATED. DEDICATED starts the game's own server program beside the window, with the page's rules handed over as a cfg, and joins it. The server keeps running after the window quits. It is recorded where the player CLI keeps its servers, so a later window or `secli host list` finds it, and STOP YOUR SERVER ends it.
