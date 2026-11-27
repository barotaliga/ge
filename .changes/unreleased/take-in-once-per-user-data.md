---
type: Fixed
title: A game pointed at is copied once, into the player's own data folder
---

Shooter's Edge keeps the games it takes in, and its series settings, in the player's own folder (on a Mac `~/Library/Application Support/gameengine2/shooters_edge/data/`), so a rebuild or a new version keeps them. A second launch pointed at the same unchanged game opens the copy at once instead of reading and checking every file again; settings an older build kept beside the program are carried over.
